#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "znl_systemd.h"
#include "znc_log.h"

static pthread_t g_wd_thread;
static pthread_mutex_t g_wd_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_wd_cond;
static bool g_wd_thread_running = false;
static uint64_t g_wd_interval_usec = 0;

bool znl_systemd_is_available(void)
{
    const char *sock = getenv("NOTIFY_SOCKET");
    return (sock != NULL && sock[0] != '\0');
}

int znl_systemd_notify(const char *state)
{
    if (!state || state[0] == '\0')
    {
        return 0;
    }

    const char *socket_path = getenv("NOTIFY_SOCKET");
    if (!socket_path || socket_path[0] == '\0')
    {
        return 0; // Not running under systemd or notify not enabled
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    size_t path_len = strlen(socket_path);
    if (path_len >= sizeof(addr.sun_path))
    {
        log_format(LOG_LEVEL_ERROR, "systemd NOTIFY_SOCKET path is too long: %zu bytes", path_len);
        return -1;
    }

    socklen_t addr_len;
    if (socket_path[0] == '@')
    {
        // Linux abstract namespace socket: first byte must be 0
        addr.sun_path[0] = '\0';
        memcpy(&addr.sun_path[1], &socket_path[1], path_len - 1);
        addr_len = offsetof(struct sockaddr_un, sun_path) + path_len;
    }
    else
    {
        memcpy(addr.sun_path, socket_path, path_len);
        addr_len = sizeof(struct sockaddr_un);
    }

    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
    {
        return -1;
    }

    ssize_t sent = sendto(fd, state, strlen(state), MSG_NOSIGNAL, (struct sockaddr *)&addr, addr_len);
    close(fd);

    return (sent >= 0) ? 0 : -1;
}

int znl_systemd_notifyf(const char *fmt, ...)
{
    if (!znl_systemd_is_available())
    {
        return 0;
    }

    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (written <= 0)
    {
        return -1;
    }

    return znl_systemd_notify(buffer);
}

int znl_systemd_notify_ready(const char *status_msg)
{
    if (status_msg && status_msg[0] != '\0')
    {
        return znl_systemd_notifyf("READY=1\nSTATUS=%s", status_msg);
    }
    return znl_systemd_notify("READY=1");
}

int znl_systemd_notify_status(const char *fmt, ...)
{
    if (!znl_systemd_is_available())
    {
        return 0;
    }

    char status_buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(status_buf, sizeof(status_buf), fmt, args);
    va_end(args);

    return znl_systemd_notifyf("STATUS=%s", status_buf);
}

int znl_systemd_notify_stopping(const char *status_msg)
{
    if (status_msg && status_msg[0] != '\0')
    {
        return znl_systemd_notifyf("STOPPING=1\nSTATUS=%s", status_msg);
    }
    return znl_systemd_notify("STOPPING=1");
}

int znl_systemd_notify_watchdog(void)
{
    return znl_systemd_notify("WATCHDOG=1");
}

bool znl_systemd_watchdog_is_enabled(uint64_t *out_usec)
{
    const char *watchdog_usec_env = getenv("WATCHDOG_USEC");
    if (!watchdog_usec_env || watchdog_usec_env[0] == '\0')
    {
        return false;
    }

    const char *watchdog_pid_env = getenv("WATCHDOG_PID");
    if (watchdog_pid_env && watchdog_pid_env[0] != '\0')
    {
        pid_t pid = (pid_t)strtol(watchdog_pid_env, NULL, 10);
        if (pid > 0 && pid != getpid())
        {
            return false;
        }
    }

    char *endptr = NULL;
    errno = 0;
    unsigned long long val = strtoull(watchdog_usec_env, &endptr, 10);
    if (errno != 0 || endptr == watchdog_usec_env || val == 0)
    {
        return false;
    }

    if (out_usec)
    {
        *out_usec = (uint64_t)val;
    }
    return true;
}

static void *watchdog_worker(void *arg)
{
    (void)arg;

    log_format(LOG_LEVEL_INFO, "systemd watchdog thread started (ping every %llu ms)",
               (unsigned long long)(g_wd_interval_usec / 1000ULL));

    // Send initial watchdog keepalive
    znl_systemd_notify_watchdog();

    pthread_mutex_lock(&g_wd_mutex);
    while (g_wd_thread_running)
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);

        uint64_t sec = g_wd_interval_usec / 1000000ULL;
        uint64_t nsec = (g_wd_interval_usec % 1000000ULL) * 1000ULL;

        ts.tv_sec += sec;
        ts.tv_nsec += nsec;
        if (ts.tv_nsec >= 1000000000L)
        {
            ts.tv_sec += ts.tv_nsec / 1000000000L;
            ts.tv_nsec %= 1000000000L;
        }

        int rc = pthread_cond_timedwait(&g_wd_cond, &g_wd_mutex, &ts);
        if (!g_wd_thread_running)
        {
            break;
        }

        if (rc == ETIMEDOUT)
        {
            pthread_mutex_unlock(&g_wd_mutex);
            znl_systemd_notify_watchdog();
            pthread_mutex_lock(&g_wd_mutex);
        }
    }
    pthread_mutex_unlock(&g_wd_mutex);

    log_format(LOG_LEVEL_INFO, "systemd watchdog thread stopped");
    return NULL;
}

int znl_systemd_watchdog_start(void)
{
    uint64_t watchdog_usec = 0;
    if (!znl_systemd_watchdog_is_enabled(&watchdog_usec))
    {
        log_format(LOG_LEVEL_INFO, "systemd watchdog is disabled or not configured");
        return 0;
    }

    pthread_mutex_lock(&g_wd_mutex);
    if (g_wd_thread_running)
    {
        pthread_mutex_unlock(&g_wd_mutex);
        return 0;
    }

    // Recommended ping interval is half of watchdog timeout
    g_wd_interval_usec = watchdog_usec / 2;
    if (g_wd_interval_usec < 100000ULL) // minimum 100ms
    {
        g_wd_interval_usec = 100000ULL;
    }

    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&g_wd_cond, &attr);
    pthread_condattr_destroy(&attr);

    g_wd_thread_running = true;

    int rc = pthread_create(&g_wd_thread, NULL, watchdog_worker, NULL);
    if (rc != 0)
    {
        log_format(LOG_LEVEL_ERROR, "Failed to create watchdog thread: %s", strerror(rc));
        g_wd_thread_running = false;
        pthread_cond_destroy(&g_wd_cond);
        pthread_mutex_unlock(&g_wd_mutex);
        return -1;
    }

    pthread_mutex_unlock(&g_wd_mutex);
    return 0;
}

void znl_systemd_watchdog_stop(void)
{
    pthread_mutex_lock(&g_wd_mutex);
    if (!g_wd_thread_running)
    {
        pthread_mutex_unlock(&g_wd_mutex);
        return;
    }

    g_wd_thread_running = false;
    pthread_cond_broadcast(&g_wd_cond);
    pthread_mutex_unlock(&g_wd_mutex);

    pthread_join(g_wd_thread, NULL);
    pthread_cond_destroy(&g_wd_cond);
}

void znl_systemd_watchdog_touch(void)
{
    if (znl_systemd_is_available())
    {
        znl_systemd_notify_watchdog();
    }
}
