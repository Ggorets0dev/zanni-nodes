#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/in.h> 
#include <netinet/tcp.h>
#include "znc.h"
#include "znc_log.h"

#define MAX_EVENTS                  10

#define KEEP_ALIVE_IDLE_SEC         30
#define KEEP_ALIVE_INTERVAL_SEC     10
#define KEEP_ALIVE_ATTEMPTS_CNT     3

static void set_nonblocking(int fd) 
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void set_keepalive(int sock)
{
    int keep_alive_enable = 1; 
    int keep_idle         = KEEP_ALIVE_IDLE_SEC;        // sec
    int keep_interval     = KEEP_ALIVE_INTERVAL_SEC;    // sec
    int keep_count        = KEEP_ALIVE_ATTEMPTS_CNT;    // times

    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keep_alive_enable, sizeof(keep_alive_enable));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keep_interval, sizeof(keep_interval));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keep_count, sizeof(keep_count));
}

static znc_ready_callback_t g_ready_callback = NULL;
static volatile sig_atomic_t g_server_running = 0;
static int g_stop_event_fd = -1;

void znc_set_ready_callback(znc_ready_callback_t callback)
{
    g_ready_callback = callback;
}

void znc_stop_server(void)
{
    g_server_running = 0;
    if (g_stop_event_fd >= 0)
    {
        uint64_t val = 1;
        ssize_t s = write(g_stop_event_fd, &val, sizeof(val));
        (void)s;
    }
}

int znc_start_server(int port, int listen_backlog, zanni_recv_callback_t callback)
{
    int rc = 0;
    zanni_decode_status_e dcd_status = ZANNI_DCD_STATUS_OK;
    zanni_message_t msg = {0};

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) 
    {
        close(server_fd);
        return 1;
    }
    
    if (listen(server_fd, listen_backlog) < 0) 
    {
        close(server_fd);
        return 1;
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        close(server_fd);
        return 1;
    }

    set_nonblocking(server_fd);

    struct epoll_event ev;
    
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

    g_stop_event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (g_stop_event_fd >= 0)
    {
        struct epoll_event ev_stop;
        ev_stop.events = EPOLLIN;
        ev_stop.data.fd = g_stop_event_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, g_stop_event_fd, &ev_stop);
    }

    if (g_ready_callback)
    {
        g_ready_callback();
    }

    struct epoll_event events[MAX_EVENTS];
    g_server_running = 1;

    while (g_server_running)
    {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            log_format(LOG_LEVEL_ERROR, "epoll_wait error: %s", strerror(errno));
            break;
        }

        for (int i = 0; i < n; i++)
        {
            int active_fd = events[i].data.fd;

            if (active_fd == g_stop_event_fd)
            {
                g_server_running = 0;
                break;
            }

            if (active_fd == server_fd)
            {
                for (;;)
                {
                    char ip[INET_ADDRSTRLEN] = {0};
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

                    if (client_fd == -1) 
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) 
                        {
                            break;
                        } 
                        else 
                        {
                            break;
                        }
                    }

                    set_nonblocking(client_fd);
                    set_keepalive(client_fd);
                    
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = client_fd;
                    
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) 
                    {
                        perror("epoll_ctl: client_fd");
                        close(client_fd);
                    }

                    if (inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip)) != NULL) 
                    {
                        log_format(LOG_LEVEL_INFO, "New client connected: [ip=%s, fd=%d]", ip, client_fd);
                    }
                }
            }
            else 
            {
                uint8_t buffer[ZANNI_MSG_SIZE] = {0};

                for (;;)
                {
                    const ssize_t bytes_read = read(active_fd, buffer, sizeof(buffer));
                    
                    // ======= ERROR SECTION
                    if (bytes_read == -1) 
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) 
                        {
                            break;
                        } 
                        else 
                        {
                            close(active_fd);
                            log_format(LOG_LEVEL_INFO, "Client disconnected: [fd=%d]", active_fd);
                            break;
                        }
                    } 
                    else if (bytes_read != sizeof(buffer)) 
                    {
                        close(active_fd);
                        log_format(LOG_LEVEL_INFO, "Client disconnected: [fd=%d]", active_fd);
                        break;
                    }
                    // =======

                    dcd_status = zanni_message_decode(buffer, bytes_read, &msg);

                    if (dcd_status == ZANNI_DCD_STATUS_OK)
                    {
                        const int exit_code = callback(&msg);

                        msg.payload.exec_status = ((zanni_exec_status_e)exit_code == EXIT_SUCCESS) ? true : false;
                        zanni_message_encode(&msg, buffer, sizeof(buffer));
                        const ssize_t bytes_wrote = send(active_fd, buffer, sizeof(buffer), 0);

                        continue;
                    }
                    else if (dcd_status == ZANNI_DCD_STATUS_WRONG_VER)
                    {
                        log_format(LOG_LEVEL_WARN, "Message with wrong major protocol version collected: [curr_ver=%d]", ZANNI_PROTOCOL_VERSION_MAJOR);
                    }
                    else if (dcd_status == ZANNI_DCD_STATUS_ERROR)
                    {
                        log_format(LOG_LEVEL_WARN, "Unknown error occured while decoding message");
                    }
                }
            }
        }
    }

    if (g_stop_event_fd >= 0)
    {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, g_stop_event_fd, NULL);
        close(g_stop_event_fd);
        g_stop_event_fd = -1;
    }

    if (server_fd >= 0)
    {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, server_fd, NULL);
        close(server_fd);
    }

    if (epoll_fd >= 0)
    {
        close(epoll_fd);
    }

    return 0;
}