#include "common.h"

char* editorPrompt(char* prompt, void(*callback)(char*, i32));
char* editorRowsToString(i32* buflen);
void editorOpen(const char* filename);
void editorMoveCursor(i32 key);
void editorProcessPress(void);
