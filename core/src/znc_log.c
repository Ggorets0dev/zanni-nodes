#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include "znc_log.h"
#include "znc_attributes.h"

#define LOG_FILE_DEFAULT    "zanni-gate-node.log"
#define LOG_DIR_DEFAULT     "/var/log/zanni-gate-node"
#define LOG_BUFFEER_SIZE    1024

static char log_file_path[256];
static FILE* log_file;

__CONSTRUCTOR static void log_init(void)
{
    mkdir(LOG_DIR_DEFAULT, 0777);

    snprintf(log_file_path, sizeof(log_file_path), "%s/%s", LOG_DIR_DEFAULT, LOG_FILE_DEFAULT);

    log_file = fopen(log_file_path, "a");

    if (!log_file)
    {
        fprintf(stderr, "Warning: Failed to open log file '%s', continuing with stdout only\n", log_file_path);
    }

    setvbuf(stdout, NULL, _IOLBF, 0);
    if (log_file)
    {
        setvbuf(log_file, NULL, _IOLBF, 0);
    }
}

__DESTRUCTOR void log_deinit(void)
{
    if (log_file)  fclose(log_file);
}

void log_format(log_level_e level, const char *format, ...) 
{
    static char log_buffer[LOG_BUFFEER_SIZE];

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    const char *level_strings[] = {"INFO", "WARN", "ERROR"};

    int written = snprintf(log_buffer, sizeof(log_buffer), "[%s] [%s] ", time_str, level_strings[level]);

    if (written > 0 && (size_t)written < sizeof(log_buffer)) 
    {
        va_list args;
        va_start(args, format);
        vsnprintf(log_buffer + written, sizeof(log_buffer) - written, format, args);
        va_end(args);
    }

    printf("%s\n", log_buffer);
    
    if (log_file)
        fprintf(log_file, "%s\n", log_buffer);
}

void log_apply_rotation(long max_size)
{
    struct stat st;

    if (stat(log_file_path, &st) == 0) 
    {
        if (st.st_size > max_size) 
        {
            fflush(log_file);
            fclose(log_file);
            remove(log_file_path);
            log_file = fopen(log_file_path, "a");
        }
    }
}