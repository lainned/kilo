#pragma once
#include "common.h"

char* editorPrompt(char* prompt, void(*callback)(char*, i32));
void editorOpen(const char* filename);
void editorSave(void);