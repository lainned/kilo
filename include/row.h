#pragma once
#include "common.h"

void editorFreeRow(erow* row);
void editorDelRow(i32 at);
void editorUpdateRow(erow* row);
void editorRowAppendString(erow* row, char* s, size_t len);
void editorInsertRow(i32 at, char* s, size_t len);
void editorRowInsertChar(erow* row, i32 at, i32 c);
void editorRowDelChar(erow* row, i32 at);
char* editorRowsToString(i32* buflen);
