#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "znc_execute.h"

char* znc_execute_toggle(const znc_commands_toggle_t* cmds,
                         bool state,
                         int *exit_code)
{
    if (exit_code != NULL)
    {
        *exit_code = 1;
    }

    if (cmds == NULL)
    {
        return NULL;
    }

    const char *command = state ? cmds->on : cmds->off;

    if (command == NULL)
    {
        return NULL;
    }

    FILE *fp = popen(command, "r");
    if (fp == NULL)
    {
        return NULL;
    }

    size_t buffer_size = 1024;
    char *output = malloc(buffer_size);
    if (output == NULL)
    {
        pclose(fp);
        return NULL;
    }

    output[0] = '\0';
    size_t current_len = 0;
    char temp_buffer[256];

    while (fgets(temp_buffer, sizeof(temp_buffer), fp) != NULL)
    {
        size_t temp_len = strlen(temp_buffer);

        if (current_len + temp_len + 1 > buffer_size)
        {
            size_t new_size = buffer_size;

            while (current_len + temp_len + 1 > new_size)
            {
                new_size *= 2;
            }

            char *new_output = realloc(output, new_size);
            if (new_output == NULL)
            {
                free(output);
                pclose(fp);
                return NULL;
            }

            output = new_output;
            buffer_size = new_size;
        }

        memcpy(output + current_len, temp_buffer, temp_len + 1);
        current_len += temp_len;
    }

    int status = pclose(fp);

    if (status == -1)
    {
        free(output);
        return NULL;
    }

    if (exit_code != NULL)
    {
        if (WIFEXITED(status))
        {
            *exit_code = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status))
        {
            *exit_code = 128 + WTERMSIG(status);
        }
    }

    return output;
}