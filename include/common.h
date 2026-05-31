#pragma once
#include <signal.h>
#include <stdint.h>
#include <termios.h>
#include <time.h>
#include <stdbool.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef bool b32;

#define CTRL_KEY(k) ((k) & 0x1f)
#define VERSION "0.0.1"
#define KILO_TAB_STOP 8
#define KILO_QUIT_TIMES 3

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

enum editorKey{
    BACKSPACE = 127,
    ARROW_UP = 1000,
    ARROW_DOWN,
    ARROW_LEFT,
    ARROW_RIGHT,
    HOME_KEY,
    END_KEY,
    DEL_KEY,
    PAGE_UP, 
    PAGE_DOWN
};

enum editorHighlight{
    HL_NORMAL = 0,
    HL_NUMBER,
    HL_STRING,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_MATCH
};

/*** DATA ***/

struct editorSyntax{
    char* filetype;
    char** filematch;
    char** keywords;
    char* singleline_comment_start;
    char* multiline_comment_start;
    char* multiline_comment_end;
    i32 flags;
};

typedef struct erow{
    int size;
    i32 rsize;
    i32 idx;
    i32 hl_comment_open;
    char* render;
    unsigned char* hl;
    char* chars;
} erow;

struct editorConfig{
    i32 cx, cy;
    i32 rx;
    i32 screen_rows;
    i32 screen_cols;
    i32 numrows;
    i32 rowoff;
    i32 coloff;
    i32 dirty;
    char statusmsg[80];
    time_t statusmsg_time;
    erow* row;
    char* filename;
    struct termios orig_termios;
    struct editorSyntax* syntax;
};

extern struct editorConfig E;

extern volatile sig_atomic_t window_resized = 0;


