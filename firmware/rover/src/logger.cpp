#include "logger.h"
#include <stdarg.h>
#include <time.h>

static Print *volatile g_mirror = nullptr;

void logSetMirror(Print *mirror)
{
    g_mirror = mirror;
}

static void emit(const char *format, va_list args, bool newline)
{
    char message[256];
    vsnprintf(message, sizeof(message), format, args);

    // Wall clock once NTP has set it, uptime in ms before that. Checking the
    // epoch directly keeps this non-blocking (getLocalTime() can wait seconds).
    char stamp[32];
    time_t now = time(nullptr);
    if (now > 1700000000)
    {
        struct tm info;
        localtime_r(&now, &info);
        strftime(stamp, sizeof(stamp), "[%Y-%m-%d %H:%M:%S]", &info);
    }
    else
    {
        snprintf(stamp, sizeof(stamp), "[%010lu]", millis());
    }

    Serial.print(stamp);
    Serial.print(' ');
    Serial.print(message);
    if (newline)
    {
        Serial.println();
    }

    Print *mirror = g_mirror;
    if (mirror)
    {
        mirror->print(stamp);
        mirror->print(' ');
        mirror->print(message);
        if (newline)
        {
            mirror->println();
        }
    }
}

void logPrint(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    emit(format, args, false);
    va_end(args);
}

void logPrintln(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    emit(format, args, true);
    va_end(args);
}
