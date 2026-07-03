#ifndef __LIBS_H
#define __LIBS_H

#include <stdio.h>
#include <stdint.h>
#include "../include/config.h"

extern volatile char     g_log_buffer[1024];
extern volatile uint32_t g_log_length;

void log_printf(const char *fmt, ...);

#define xprintf(...) log_printf(__VA_ARGS__)

#endif /* __LIBS_H */
