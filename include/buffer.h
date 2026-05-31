#pragma once
#include "common.h"

struct abuf{
    char* b;
    i32 len;
};

#define ABUF_INIT {NULL, 0}
void abAppend(struct abuf* ab, const char* s, i32 len);
void abFree(struct abuf* ab);