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

    fprintf(logf, "[%02d:%02d:%02d] ",
            tm->tm_hour, tm->tm_min, tm->tm_sec);

    va_list args;
    va_start(args, fmt);
    vfprintf(logf, fmt, args);
    va_end(args);

    fprintf(logf, "\n");
    fflush(logf);
}

void log_close()
{
    if (logf) fclose(logf);
}