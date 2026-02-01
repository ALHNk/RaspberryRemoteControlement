#include "logger.h"
#include <stdarg.h>
#include <time.h>

static FILE *logf = NULL;

int log_init(const char *path)
{
    logf = fopen(path, "a");
    return logf ? 0 : -1;
}

void log_msg(const char *fmt, ...)
{
    if (!logf) return;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    char timebuf[16];
    snprintf(timebuf, sizeof(timebuf),
             "%02d:%02d:%02d",
             tm->tm_hour, tm->tm_min, tm->tm_sec);

    va_list args;
    va_start(args, fmt);

    // --- В ФАЙЛ ---
    fprintf(logf, "[%s] ", timebuf);

    va_list args_file;
    va_copy(args_file, args);
    vfprintf(logf, fmt, args_file);
    va_end(args_file);

    fprintf(logf, "\n");
    fflush(logf);

    // --- В ТЕРМИНАЛ ---
    fprintf(stdout, "[%s] ", timebuf);
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    fflush(stdout);

    va_end(args);
}

void log_close()
{
    if (logf) fclose(logf);
}
