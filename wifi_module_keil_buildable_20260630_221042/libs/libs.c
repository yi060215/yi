#include "libs.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

volatile char     g_log_buffer[1024];
volatile uint32_t g_log_length = 0;

static void log_write(const char *buf, size_t len)
{
    if (!buf || !len) {
        return;
    }

    if (len >= sizeof(g_log_buffer)) {
        buf += len - (sizeof(g_log_buffer) - 1U);
        len = sizeof(g_log_buffer) - 1U;
    }

    if ((g_log_length + len) >= sizeof(g_log_buffer)) {
        size_t keep = sizeof(g_log_buffer) - len - 1U;
        memmove((void *)g_log_buffer,
                (const void *)(g_log_buffer + g_log_length - keep),
                keep);
        g_log_length = (uint32_t)keep;
    }

    memcpy((void *)(g_log_buffer + g_log_length), buf, len);
    g_log_length += (uint32_t)len;
    g_log_buffer[g_log_length] = '\0';
}

void log_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len <= 0) {
        return;
    }

    if ((size_t) len >= sizeof(buf)) {
        len = (int) sizeof(buf) - 1;
    }

    log_write(buf, (size_t) len);
}
