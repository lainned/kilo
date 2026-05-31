#pragma once
#include "common.h"


char* C_HL_extensions[] = {".c", ".cpp", ".h", ".hpp", NULL};
char* C_HL_keywords[] = {"break", "else", "if", "continue", "union", "typedef", "void|", "switch", "case", "default", "while", "return", "class", "static", "for", "do", "define",
"int|", "double|", "float|", "char|", "long|", "unsigned|", "signed|", "struct|", "class|", "enum|", "bool|", NULL};

struct editorSyntax HLDB[] = {
    {
        "c",
        C_HL_extensions,
        C_HL_keywords,
        "//", "/*", "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    }
};

#define HLDB_ENTRIES sizeof(HLDB) / sizeof(HLDB[0])

i32 isSeparator(char c);
void editorRowUpdateSyntax(erow* row);
i32 editorSyntaxToColor(i32 hl);
void editorSelectSyntaxHighlightning(void);
