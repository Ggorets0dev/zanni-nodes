#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "znc.h"
#include "znc_log.h"
#include "znc_config.h"
#include "znc_execute.h"
#include "znl_systemd.h"

static znc_config_t config;

static void on_signal(int sig)
{
    (void)sig;
    znl_systemd_notify_stopping("Shutting down ZanniGate node...");
    znc_stop_server();
}

static void on_server_ready(void)
{
    log_format(LOG_LEVEL_INFO, "ZanniGate node server is ready and listening on port %d", config.server.port);
    znl_systemd_notify_ready("ZanniGate node is ready");
    znl_systemd_notify_status("Listening on port %d", config.server.port);
    znl_systemd_watchdog_start();
}

static int on_recv(zanni_message_t *message)
{
    int exit_code = 0;
    
    if (!message) return 1;

    znl_systemd_watchdog_touch();

    log_format(LOG_LEVEL_INFO, "Recieved message: [tag=%s, dev_type=%d, status=%d]", message->tag, message->device_type, message->payload.dev_status);

    const znc_virtual_device_t* dev = znc_config_get_device_by_tag(&config, message->tag);

    if (!dev)
    {
        log_format(LOG_LEVEL_WARN, "Failed to find virtual device from message: [tag=%s]", message->tag);
        znl_systemd_notify_status("Listening on port %d | Unknown tag=%s", config.server.port, message->tag);
        return 1;
    }

    if (dev->type == ZANNI_VIRT_DEV_TOGGLE)
    {
        char* output = znc_execute_toggle(&dev->commands.toggle, message->payload.dev_status, &exit_code);
    
        if (!output)
        {
            log_format(LOG_LEVEL_WARN, "Failed to execute command for collected message: [tag=%s, dev_type=%d, status=%d]",
                       message->tag, message->device_type, message->payload.dev_status);
        }
        else
        {
            log_format(LOG_LEVEL_INFO, "Executed command for collected message: [tag=%s, dev_type=%d, status=%d, output=%s]",
                       message->tag, message->device_type, message->payload.dev_status, output);
        }
    
        free(output);
    }
    else
    {
        log_format(LOG_LEVEL_WARN, "Unsupported device type: [dev_type=%d]", dev->type);
        return 1;
    }

    znl_systemd_notify_status("Listening on port %d | Last msg: tag=%s (rc=%d)",
                             config.server.port, message->tag, exit_code);

    return exit_code;
}

int main(int argc, char** argv) 
{
    int rc = 0;

    log_format(LOG_LEVEL_INFO, "Starting ZanniGate node...");

    if (argc != 2)
    {
        log_format(LOG_LEVEL_ERROR, "Wrong count of command line arguments, usage: zanni-gate-node config.json");
        return 1;
    }
    
    if (access(argv[1], F_OK) != 0) 
    {
        log_format(LOG_LEVEL_ERROR, "Failed to find config file: [path=%s]", argv[1]);
        return 1;
    }

    char* config_path = argv[1];

    rc = znc_config_load(config_path, &config);
    if (rc != 0)
    {
        log_format(LOG_LEVEL_ERROR, "Failed to load config: [path=%s]", config_path);
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    znc_set_ready_callback(on_server_ready);

    rc = znc_start_server(config.server.port, 10, on_recv);

    log_format(LOG_LEVEL_INFO, "Stopping ZanniGate node...");
    znl_systemd_watchdog_stop();
    znc_config_free(&config);
    log_format(LOG_LEVEL_INFO, "ZanniGate node stopped cleanly.");
    
    return rc;
}
