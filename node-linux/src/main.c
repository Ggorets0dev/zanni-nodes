#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "znc.h"
#include "znc_log.h"
#include "znc_config.h"
#include "znc_execute.h"

static znc_config_t config;

static int on_recv(zanni_message_t *message)
{
    int exit_code = 0;
    
    if (!message) return 1;

    log_format(LOG_LEVEL_INFO, "Recieved message: [tag=%s, dev_type=%d, status=%d]", message->tag, message->device_type, message->payload.dev_status);

    const znc_virtual_device_t* dev = znc_config_get_device_by_tag(&config, message->tag);

    if (!dev)
    {
        log_format(LOG_LEVEL_WARN, "Failed to find virtual device from message: [tag=%s]", message->tag);
        return 1;
    }

    if (dev->type == ZANNI_VIRT_DEV_TOGGLE)
    {
        char* output = znc_execute_toggle(&dev->commands.toggle, message->payload.dev_status, &exit_code);
    
        if (!output)    log_format(LOG_LEVEL_WARN, "Failed to execute command for collected message: [tag=%s, dev_type=%d, status=%d]", message->tag, message->device_type, message->payload.dev_status);
        else            log_format(LOG_LEVEL_INFO, "Executed command for collected message: [tag=%s, dev_type=%d, status=%d, output=%s]", message->tag, message->device_type, message->payload.dev_status, output);
    
        free(output);
    }
    else
    {
        log_format(LOG_LEVEL_WARN, "Unsupported device type: [dev_type=%d]", dev->type);
        return 1;
    }

    return exit_code;
}

int main(int argc, char** argv) 
{
    int rc = 0;

    log_format(LOG_LEVEL_INFO, "Starting ZanniGate node...");

    if (argc != 2)
    {
        log_format(LOG_LEVEL_ERROR, "Wrong count of command line arguments, usage: zanni-gate-node config.json");
    }
    else if (access(argv[1], F_OK) != 0) 
    {
        log_format(LOG_LEVEL_ERROR, "Failed to find config file: [path=%s]", argv[1]);
    }

    char* config_path = argv[1];

    rc = znc_config_load(config_path, &config);

    if (rc != 0)
    {
        log_format(LOG_LEVEL_ERROR, "Failed to load config: [path=%s]", config_path);
        return 1;
    }

    rc = znc_start_server(config.server.port, 10, on_recv);
    
    return rc;
}
