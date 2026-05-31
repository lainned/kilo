/**** INCLUDES ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#include <signal.h>z
#include <stdarg.h>
#include <time.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/ioctls.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdint.h>

/*** DEFINES ***/
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

struct editorConfig E;

volatile sig_atomic_t window_resized = 0;

/*** filetypes ***/

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

/*** logging ***/

void kiloLog(const char* fmt, ...){
    FILE *logfile = fopen("kilo.log", "a");
    if(logfile == NULL) return;

    va_list args;
    va_start(args, fmt);

    vfprintf(logfile, fmt, args);

    va_end(args);
    fflush(logfile);
    fclose(logfile);
}

/*** SIGNAL HANDLING ***/

void signalHandler(int sig){
    (void)sig;
    window_resized = 1;
}

/*** append buffer ***/

struct abuf{
    char* b;
    i32 len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf* ab, const char* s, i32 len){
    char* new = realloc(ab->b, ab->len + len);
    if(new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;

}

void abFree(struct abuf* ab){
    free(ab->b);
}

/*** PROTOTYPES ***/

void editorRefreshScreen(void);
void editorSetStatusMessage(const char* fmt, ...);
void editorRefreshSize(void);
char* editorPrompt(char* prompt, void(*callback)(char*, i32));
i32 editorRxToCx(erow* row, i32 rx);

/*** FREE ***/

void editorFree(void){
    for(i32 i = 0; i < E.numrows; i++){
        free(E.row[i].chars);
        free(E.row[i].render);
        free(E.row[i].hl);
    }
    free(E.row);
    free(E.filename);
}

/*** TERMINAL ***/

void die(const char* c){
    editorRefreshScreen();
    editorFree();
    perror(c);
    exit(1);
}




/*** syntax highlightning ***/



i32 isSeparator(char c){
    return (c == '\0' || isspace(c) || strchr(",.()+-/*=~%<>[];", c) != NULL);
}

void editorRowUpdateSyntax(erow* row){
    row->hl = realloc(row->hl, row->rsize);
    memset(row->hl, HL_NORMAL, row->rsize);

    if(E.syntax == NULL) return;

    char* scs = E.syntax->singleline_comment_start;
    char** keywords = E.syntax->keywords;
    int scs_len = scs ? strlen(scs) : 0;

    char* mcs = E.syntax->multiline_comment_start;
    char* mce = E.syntax->multiline_comment_end;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    i32 prev_sep = 1;
    i32 i = 0;
    i32 in_string = 0;
    i32 in_comment = (row->idx > 0 && E.row[row->idx-1].hl_comment_open);
    while(i < row->size){
        char c = row->render[i];
        unsigned char prev_hl = (i > 0 ? row->hl[i-1] : HL_NORMAL);
        if(scs_len && !in_string && !in_comment){
            if(!strncmp(&row->render[i], scs, scs_len)){
                memset(&row->hl[i], HL_COMMENT, row->rsize - i);
                break;
            }
        }
        if(mcs_len && mce_len && !in_string){
            if(in_comment){
                row->hl[i] = HL_MLCOMMENT;
                if(!strncmp(&row->render[i], mce, mce_len)){
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i+=mce_len;
                    prev_sep = 1;
                    in_comment = 0;
                    continue;
                }
                else{
                    i++;
                    continue;
                }
            }
            else if(!strncmp(&row->render[i], mcs, mcs_len)){
                memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
                in_comment = 1;
                i+=mcs_len;
                continue;
            }
        }
        if(E.syntax-> flags & HL_HIGHLIGHT_STRINGS){
            if(in_string){
                row->hl[i] = HL_STRING;
                if(c == '\\' && i + 1 < row->size){
                    row->hl[i+1] = HL_STRING;
                    i+=2;
                    continue;
                }
                if(c == in_string) in_string = 0;
                i++;
                prev_sep = 1;
                continue;
            }
            else{
                if(c == '"' || c == '\''){
                    in_string = c;
                    row->hl[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }
        if(E.syntax->flags & HL_HIGHLIGHT_NUMBERS){
            if((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) || (c == '.' && prev_hl == HL_NUMBER)){
                row->hl[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }
        if(prev_sep){
            i32 j;
            for(j = 0; keywords[j]; j++){
                i32 klen = strlen(keywords[j]);
                i32 kw2 = keywords[j][klen-1] == '|';
                if(kw2) klen--;
                if(!strncmp(keywords[j], &row->render[i], klen) && isSeparator(row->render[i + klen])){
                    memset(&row->hl[i], (kw2 ? HL_KEYWORD2 : HL_KEYWORD1), klen);
                    i+=klen;
                    break;
                }
            }
            if(keywords[j] != NULL){
                prev_sep = 0;
                continue;
            }
        }
        prev_sep = isSeparator(c);
        i++;
    }

    i32 changed = (in_comment != row->hl_comment_open);
    row->hl_comment_open = in_comment;
    if(changed && row->idx + 1 < E.numrows){
        editorRowUpdateSyntax(&E.row[row->idx+1]);
    }
}

i32 editorSyntaxToColor(i32 hl){
    switch(hl){
        case HL_NUMBER:
            return 92;
        case HL_COMMENT:
        case HL_MLCOMMENT:
            return 32;
        case HL_MATCH:
            return 34;
        case HL_STRING:
            return 35;
        case HL_KEYWORD1:
            return 33;
        case HL_KEYWORD2:
            return 36;
        default:
            return 37;
    }
}

void editorSelectSyntaxHighlightning(void){
    E.syntax = NULL;
    if(E.filename == NULL) return;

    char* ext = strchr(E.filename, '.');

    for(u32 j = 0; j < HLDB_ENTRIES; j++){
        struct editorSyntax* s = &HLDB[j];
        u32 i = 0;
        while(s->filematch[i]){
            i32 is_ext = (s->filematch[i][0] == '.');
            if(((is_ext && ext && !strcmp(s->filematch[i], ext)) || (!is_ext && strstr(E.filename, s->filematch[i])))){
                E.syntax = s;
                for(i32 filerow = 0; filerow < E.numrows; filerow++){
                    editorRowUpdateSyntax(&E.row[filerow]);
                }
                return;
            }
            i++;
        }
    }
}

/*** row operations***/

void editorFreeRow(erow* row){
    free(row->chars);
    free(row->render);
    free(row->hl);
}

void editorDelRow(i32 at){
    if(at < 0 || at >= E.numrows) return;
    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at+1], sizeof(erow) * (E.numrows - at - 1));
    E.numrows--;
    E.dirty++;
}

void editorUpdateRow(erow* row){
    i32 tabs = 0, j;
    for(j = 0; j < row->size; j++){
        if(row->chars[j] == '\t'){
            tabs++;
        }
    }
    free(row->render);
    row->render = malloc(row->size + 1 + tabs*(KILO_TAB_STOP-1));
    
    
    i32 idx = 0;
    for(j = 0; j < row->size; j++){
        if(row->chars[j] == '\t'){
            row->render[idx++] = ' ';
            while(idx % KILO_TAB_STOP != 0){
                row->render[idx++] = ' ';
            }
        }
        else{
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;

    editorRowUpdateSyntax(row);
}

void editorRowAppendString(erow* row, char* s, size_t len){
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size+=len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

void editorInsertRow(i32 at, char* s, size_t len){
    if(at < 0 || at > E.numrows) return; 
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    memmove(&E.row[at+1], &E.row[at], sizeof(erow) * (E.numrows - at));
    E.row[at].idx = at;
    E.row[at].size = len;
    E.row[at].chars = malloc(len+1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.row[at].render = NULL;
    E.row[at].rsize = 0;
    E.row[at].hl = NULL;
    E.row[at].hl_comment_open = 0;
    
    editorUpdateRow(&E.row[at]);
    E.numrows++;
    E.dirty++;
    for(i32 j = at + 1; j < E.numrows; j++){
        E.row[j].idx++;
    }
}

void editorRowInsertChar(erow* row, i32 at, i32 c){
    if(at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at+1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowDelChar(erow* row, i32 at){
    if(at < 0 || at > row->size) return;
    memmove(&row->chars[at], &row->chars[at+1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}


/*** file i/o***/

// receives the format of prompt and handles input in the prompt bar

char* editorPrompt(char* prompt, void(*callback)(char*, i32)){
    size_t bufsize = 128;
    char* buf = malloc(bufsize);
    if(buf == NULL) die("editorPrompt");

    size_t buflen = 0;
    buf[0] = '\0';
    while(1){
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();
        i32 c = editorReadKey();
        if(c == '\r'){
            if(buflen != 0){
                editorSetStatusMessage("");
                if(callback) callback(buf, c);
                return buf;
            }
        } 
        else if(c == '\x1b'){
            editorSetStatusMessage("");
            if(callback) callback(buf, c);
            free(buf);
            return NULL;
        }
        else if(c == BACKSPACE){
            if(buflen > 0){
                buf[buflen-1] = '\0';
                buflen--;
            }
        }
        else if(c < 128 && !iscntrl(c)){
            if(buflen == bufsize - 1){
                bufsize*=2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }
        if(callback) callback(buf, c);
    }
}

char* editorRowsToString(i32* buflen){
    i32 totlen = 0;
    i32 j;
    for(j = 0; j < E.numrows; j++){
        totlen += E.row[j].size + 1;
    }
    *buflen = totlen;
    char* buf = malloc(totlen);
    char* p = buf;
    for(j = 0; j < E.numrows; j++){
        memcpy(p, E.row[j].chars, E.row[j].size);
        p+=E.row[j].size;
        *p = '\n';
        p++;
    }
    return buf;
}

// opens the provided file

void editorOpen(const char* filename){
    FILE* fp = fopen(filename, "r");
    if(fp == NULL) die("fopen");
    free(E.filename);
    E.filename = strdup(filename);
    editorSelectSyntaxHighlightning();
    char* line = NULL;
    ssize_t linelen = 0;
    size_t linecap = 0;
    while((linelen = getline(&line, &linecap, fp)) != -1){
        while(linelen > 0 && (line[linelen-1] == '\r' || line[linelen-1] == '\n')) linelen--;
        editorInsertRow(E.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
}



void editorSave(void){
    if(E.filename == NULL){
        E.filename = editorPrompt("Enter the filename as %s:", NULL);
        if(E.filename == NULL){
            editorSetStatusMessage("Save aborted.");
            return;
        }
        editorSelectSyntaxHighlightning();
    }
    
    i32 len;
    char* buf = editorRowsToString(&len);
    // open the file if its not null with flags and permission
    i32 fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    i32 success = -1;
    if(fd != -1){
        // setting the file size to the specified length
        if(ftruncate(fd, len) != -1){
            if(write(fd, buf, len) == len){
                success = 0;
            }
            
        }
        close(fd);
    }
    free(buf);
    if(success != 0){
        editorSetStatusMessage("CAN'T SAVE THE FILE I/O ERROR: %s", strerror(errno));
    }
    else{
        editorSetStatusMessage("%d bytes written to the disk", len);
        E.dirty = 0;
    }

}
/*** GO TO LINE ***/

void editorGoToLineCallback(char* query, i32 key){
    if(key == '\r' || key == '\x1b' || query[0] == '\0'){
        return;
    }
    i32 line_num = 0;
    for(u32 i = 0; query[i] != '\0'; i++){
        char d = query[i];
        if(d >= '0' && d <= '9'){
            line_num = line_num * 10 + (d-'0');
        }
        else{
            return;
        }
    }
    if(line_num > E.numrows || line_num < 1) return;
    E.cy = line_num - 1;
    E.cx = 0;
}

void editorGoToLine(void){
    i32 saved_cy = E.cy;
    i32 saved_cx = E.cx;
    i32 saved_rowoff = E.rowoff;
    i32 saved_coloff = E.coloff;

    char* line_num = editorPrompt("Enter the line number you want to go to: %s", editorGoToLineCallback);
    if(line_num){
        E.cx  = 0;
        free(line_num);
    }
    else{
        E.cy = saved_cy;
        E.cx = saved_cx;
        E.rowoff = saved_rowoff;
        E.coloff = saved_coloff;
    }
}

/*** FIND ***/

void editorFindCallback(char* query, i32 key){
    static int last_match = -1;
    static int direction = 1;

    static i32 saved_hl_line = 0;
    static char* saved_hl = NULL;
    if(saved_hl){
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = NULL;
    }
    if(key == '\r' || key == '\x1b'){
        last_match = -1;
        direction = 1;
        return;
    }
    else if(key == ARROW_UP || key == ARROW_LEFT){
        direction = -1;
    }
    else if(key == ARROW_DOWN || key == ARROW_RIGHT){
        direction = 1;
    }
    else{
        last_match = -1;
        direction = 1;
    }

    if(last_match == -1) direction = 1;
    i32 current = last_match;

    i32 i;
    for(i = 0; i < E.numrows; i++){
        current+=direction;
        if(current == -1) current = E.numrows - 1;
        else if(current == E.numrows) current = 0;

        erow* row = &E.row[current];
        char* match = strstr(row->render, query);
        if(match){
            last_match = current;
            E.cy = current;
            // match - row-> render is the index of start of the found string
            E.cx = editorRxToCx(row, match - row->render);
            E.rowoff = E.numrows;
            saved_hl_line = current;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);
            memset(&row->hl[match-row->render], HL_MATCH, strlen(query));
            break;
        }
        
    }

}

void editorFind(void){
    i32 saved_cx = E.cx;
    i32 saved_cy = E.cy;
    i32 saved_rowoff = E.rowoff;
    i32 saved_coloff = E.coloff;

    char* query = editorPrompt("Search: %s (Use ESC | Arrows | Enter)", editorFindCallback);
    if(query){
        free(query);
    }
    else{
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.rowoff = saved_rowoff;
        E.coloff = saved_coloff;
    }
}

/*** OUTPUT ***/

i32 editorCxToRx(erow* row, i32 cx){
    i32 rx = 0;
    for(i32 j = 0; j < cx; j++){
        if(row->chars[j] == '\t'){
            rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
        }
        rx++;
    }
    return rx;
}

i32 editorRxToCx(erow* row, i32 rx){
    i32 cx;
    i32 cur_rx = 0;
    for(cx = 0; cx < row->size; cx++){
        if(row->chars[cx] == '\t'){
            cur_rx+=(KILO_TAB_STOP - 1) - (cur_rx % KILO_TAB_STOP);
        }
        cur_rx++;
        if(cur_rx > rx) return cx;
    }
    return cx;
}

void editorScroll(void){
    E.rx = 0;
    if(E.cy < E.numrows){
        E.rx = editorCxToRx(&E.row[E.cy], E.cx);
    }
    if(E.cy < E.rowoff){
        E.rowoff = E.cy;
    }
    if(E.cy >= E.rowoff + E.screen_rows){
        E.rowoff = E.cy - E.screen_rows + 1;
    }
    if(E.rx < E.coloff){
        E.coloff = E.rx;
    }
    if(E.rx >= E.coloff + E.screen_cols){
        E.coloff = E.rx - E.screen_cols + 1;
    }
}

// draw tildes like vim does

void editorInsertChar(i32 c){
    if(E.cy == E.numrows){
        editorInsertRow(E.numrows, "", 0);
    }
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++;
}

void editorInsertNewline(void){
    if(E.cx == 0){
        editorInsertRow(E.cy, "", 0);
    }
    else{
        erow* row = &E.row[E.cy];
        editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    E.cy++;
    E.cx = 0;
}

void editorDelChar(void){
    if(E.cy == E.numrows) return;
    if(E.cx == 0 && E.cy == 0) return;
    erow* row = &E.row[E.cy];
    if(E.cx > 0){
        editorRowDelChar(row, --E.cx);
    }
    else{
        E.cx = E.row[E.cy-1].size;
        editorRowAppendString(&E.row[E.cy-1], row->chars, row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
}

void editorDrawStatusBar(struct abuf* ab){
    abAppend(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    i32 len = snprintf(status, sizeof(status), "%.20s - %d lines %s", (E.filename == NULL ? "[No name]" : E.filename), E.numrows, (E.dirty ? "(modified)" : ""));
    i32 rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d", (E.syntax ? E.syntax->filetype : "no ft"), E.cy + 1, E.numrows);
    if(len > E.screen_cols) len = E.screen_cols;
    abAppend(ab, status, len);
    while(len < E.screen_cols){
        if(E.screen_cols - len == rlen){
            abAppend(ab, rstatus, rlen);
            break;
        }
        else{
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);


}
void editorSetStatusMessage(const char* fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}
void editorDrawMessageBar(struct abuf* ab){
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if(msglen > E.screen_cols) msglen = E.screen_cols;
    if(msglen && time(NULL) - E.statusmsg_time < 5){
        abAppend(ab, E.statusmsg, msglen);
    }
}

void editorDrawRows(struct abuf* ab){
    for(i32 i = 0; i < E.screen_rows; i++){
        i32 filerow = i + E.rowoff;
        if(filerow >= E.numrows){
            if(E.numrows == 0 && i == E.screen_rows / 3){
                char welcome[80];
                i32 welcomeLen = snprintf(welcome, sizeof(welcome), "Kilo version --- %s", VERSION);
                if(welcomeLen > E.screen_cols) welcomeLen = E.screen_cols;
                i32 padding = (E.screen_cols - welcomeLen) / 2;
                if(padding){
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while(padding--){
                    abAppend(ab, " ", 1);
                }
                abAppend(ab, welcome, welcomeLen);
            }
            else{
                abAppend(ab, "~", 1);
            }
        }
        else{
            char* c = &E.row[filerow].render[E.coloff];
            i32 len = E.row[filerow].rsize - E.coloff;
            if(len < 0) len = 0;
            if(len > E.screen_cols) len = E.screen_cols;
            unsigned char* hl = &E.row[filerow].hl[E.coloff];
            i32 cur_color = -1;
            for(i32 j = 0; j < len; j++){
                if(iscntrl(c[j])){
                    char sym = c[j] <= 26 ? c[j] + '@' : '?';
                    abAppend(ab, "\x1b[7m", 4);
                    abAppend(ab, &sym, 1);
                    abAppend(ab, "\x1b[m", 3);
                    if(cur_color != -1){
                        char buf[16];
                        i32 buf_len = snprintf(buf, sizeof(buf), "\x1b[%dm", cur_color);
                        abAppend(ab, buf, buf_len);
                    }
                }
                else if(hl[j] == HL_NORMAL){
                    if(cur_color != -1){
                        cur_color = -1;
                        abAppend(ab, "\x1b[39m", 5);
                    }
                    abAppend(ab, &c[j], 1);
                }
                else{
                    i32 color = editorSyntaxToColor(hl[j]);
                    if(color != cur_color){
                        cur_color = color;
                        char buf[16];
                        i32 clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        abAppend(ab, buf, clen);
                    }
                    abAppend(ab, &c[j], 1);
                }
            }
            abAppend(ab, "\x1b[39m", 5);
        }
        // clear each line before the cursor
        abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2);
    }
}

void editorClearScreen(void){
    // write escape sequences
    // \x1b is 27 in decimal followed by [ is a escape sequence
    // clears the whole screen up and under the cursor 
    write(STDOUT_FILENO, "\x1b[2J", 4);
    // repositions the cursor to the start
    write(STDOUT_FILENO, "\x1b[H", 3);
}
void editorRefreshSize(void){
    if(getWindowSize(&E.screen_rows, &E.screen_cols) == -1) die("getWindowsSize");
    E.screen_rows-=2;
}
void editorRefreshScreen(void){
    editorScroll();
    struct abuf ab = ABUF_INIT;
    //turn off the cursor
    abAppend(&ab, "\x1b[?25l", 6);
    //reposition the cursor to the top
    abAppend(&ab, "\x1b[H", 3);
    
    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);
    char buf[32];
    // position the cursor to its old position
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy-E.rowoff+1, E.rx-E.coloff+1);
    abAppend(&ab, buf, strlen(buf));
    // turn on the cursor
    abAppend(&ab, "\x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}


/*** INPUT***/

void editorMoveCursor(i32 key){
    erow* row = (E.cy >= E.numrows ? NULL : &E.row[E.cy]);
    switch(key){
        case ARROW_UP:
            if(E.cy > 0) E.cy--;
            break;
        case ARROW_LEFT:
            if(E.cx > 0) E.cx--;
            else if(E.cy > 0){
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_DOWN:
            if(E.cy < E.numrows) E.cy++;
            break;
        case ARROW_RIGHT:
            if(row && E.cx < row->size){
                E.cx++;
            }
            else if(row && E.cx == row->size){
                E.cy++;
                E.cx = 0;
            }
            break;
    }
    row = (E.cy >= E.numrows ? NULL : &E.row[E.cy]);
    i32 rowlen = row ? row->size : 0;
    if(E.cx > rowlen){
        E.cx = rowlen;
    }
}

void editorProcessPress(void){
    static i32 quit_times = KILO_QUIT_TIMES;
    i32 c = editorReadKey();
    kiloLog("Key %d was pressed\n", c);
    switch (c) {
        // enter key
        case '\r':
            editorInsertNewline();
            break;
        case CTRL_KEY('q'):
            if(E.dirty && quit_times){
                editorSetStatusMessage("You have unsaved changes! Press Ctrl-Q %d more times to quit.", quit_times--);
                return;
            }
            editorClearScreen();
            editorFree();
            exit(0);
            break;
        case CTRL_KEY('d'):
            editorRefreshScreen();
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            {
                if(c == PAGE_DOWN){
                    E.cy = E.rowoff;
                }
                else if(c == PAGE_UP){
                    E.cy = E.rowoff + E.screen_rows - 1;
                    if(E.cy > E.numrows) E.cy = E.numrows;
                }
                i32 times = E.screen_rows;
                while(times--){
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }
            break;
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if(E.cy < E.numrows){
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
        case CTRL_KEY('g'):
            editorGoToLine();
            break;
        case CTRL_KEY('f'):
            editorFind();
            break;
        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if(c == DEL_KEY){
                editorMoveCursor(ARROW_RIGHT);
            }
            editorDelChar();
            break;
        case CTRL_KEY('s'):
            editorSave();
            break;
        case CTRL_KEY('l'):
        case '\x1b':
            //todo
            break;
        default:
            editorInsertChar(c);
            break;
    }
    quit_times = KILO_QUIT_TIMES;
}



/*** INIT ***/


void initEditor(void){
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.numrows = 0;
    E.rowoff = 0;
    E.row = NULL;
    E.coloff = 0;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    E.filename = NULL;
    E.dirty = 0;
    E.syntax = NULL;

    // configuration of signals for the os
    struct sigaction sa;
    
    // setting how the os should handle the signal that we receive
    sa.sa_handler = signalHandler;
    //telling the os not to block any signals by setting the whole mask to 0
    sigemptyset(&sa.sa_mask);
    //turning off the SA_RESTART
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa,NULL);

    editorRefreshSize();
}


i32 main(i32 argc, char** argv){
    enableRawMode();
    initEditor();
    if(argc >= 2){
        const char* filename = argv[1];
        editorOpen(filename);
    }
    editorSetStatusMessage("Ctrl-S to save | Ctrl-Q to quit | Ctrl-F to search | Ctrl-G to go to line");
    while(1){
        editorRefreshScreen();
        editorProcessPress();
    }
    
    return 0;
}
