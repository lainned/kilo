#pragma once
#include "common.h"

void disableRawMode(void);
void enableRawMode(void);
i32 editorReadKey(void);
i32 getCursorPosition(i32* rows, i32* cols);
i32 getWindowSize(i32* rows, i32* cols);
