#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "output.h"

void capture_init(OutputCapture *cap) {
    cap->capacity = 1024;
    cap->size = 0;
    cap->buffer = malloc(cap->capacity);
    cap->buffer[0] = '\0';
}

void capture_write(OutputCapture *cap, const char *str) {
    size_t len = strlen(str);
    if(cap->size + len + 1 >= cap->capacity) {
        cap->capacity = (cap->size + len + 1) * 2;
        cap->buffer = realloc(cap->buffer, cap->capacity);
    }
    strcat(cap->buffer, str);
    cap->size += len;
}

void capture_printf(OutputCapture *cap, const char *format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    capture_write(cap, buffer);
}

void capture_free(OutputCapture *cap) {
    free(cap->buffer);
    cap->buffer = NULL;
    cap->size = cap->capacity = 0;
}

const char* capture_get(OutputCapture *cap) {
    return cap->buffer;
}