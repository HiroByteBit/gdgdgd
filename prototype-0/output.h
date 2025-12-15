#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdio.h>

typedef struct {
    char *buffer;
    size_t size;
    size_t capacity;
} OutputCapture;

void capture_init(OutputCapture *cap);
void capture_write(OutputCapture *cap, const char *str);
void capture_printf(OutputCapture *cap, const char *format, ...);
void capture_free(OutputCapture *cap);
const char* capture_get(OutputCapture *cap);

#endif