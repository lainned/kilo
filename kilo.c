/*** INCLUDES ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <asm-generic/errno-base.h>
#include <asm-generic/ioctls.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <stdbool.h>
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

enum editorKey{
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



/*** DATA ***/

typedef struct erow{
    int size;
    char* chars;
} erow;

struct editorConfig{
    int cx, cy;
    int screen_rows;
    int screen_cols;
    int numrows;
    int rowoff;
    erow* row;
    struct termios orig_termios;
};

struct editorConfig E;


/*** append buffer ***/

struct abuf{
    char* b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf* ab, const char* s, int len){
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

/*** TERMINAL ***/

void die(const char* c){
    editorRefreshScreen();
    perror(c);
    exit(1);
}

void disableRawMode(void){
    if( tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcsetattr");
}

void enableRawMode(void){
    // reading the attributes to orig_termios struct
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);
    struct termios raw = E.orig_termios;
    // changing the bit local flag
    // turn off input ctrl-s, ctrl-q
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    // turn off echo, canonical mode and ctrl-c, ctrl-z
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    // turn off carriage return 
    raw.c_oflag &= ~(OPOST);
    // sets the character size to 8 bits per byte
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) die("tcsetattr");
}

i32 editorReadKey(void){
    char c = '\0';
    int nread;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if(nread == -1 && errno != EAGAIN) die("read");
    }
    if(c == '\x1b'){
        char seq[3];
        if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        if(seq[0] == '['){
            if(seq[1] >= '0' && seq[1] <= '9'){
                if(read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if(seq[2] == '~'){
                    switch(seq[1]){
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            }
            else{
                switch(seq[1]){
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        }
        else if(seq[0] == 'O'){
            switch(seq[1]){
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        return '\x1b';
    }
    return c;
}

int getCursorPosition(int* rows, int* cols){
    char buf[32];
    uint32_t i = 0;
    // reading the cursor position into string buf
    if(write(STDOUT_FILENO, "\x1b[6n",4) != 4) return -1;

    // 
    while(i < sizeof(buf) - 1){
        if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if(buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';
    if(buf[0] != '\x1b' || buf[1] != '[') return -1;
    if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    return 0;
}

int getWindowSize(int* rows, int* cols){
    struct winsize size;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1 || size.ws_col == 0 || size.ws_row == 0){
        // using a fallback method when ioctl fails
        // moving the cursor to the right bottom corner 
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[998B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    }
    else{
        *rows = size.ws_row;
        *cols = size.ws_col;
        return 0;
    }
}
/*** row operations***/

void editorAppendRow(char* s, size_t len){
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

    int atrow = E.numrows;
    E.row[atrow].size = len;
    E.row[atrow].chars = malloc(len+1);
    memcpy(E.row[atrow].chars, s, len);
    E.row[atrow].chars[len] = '\0';
    E.numrows++;
}

/*** file i/o***/

void editorOpen(const char* filename){
    FILE* fp = fopen(filename, "r");
    if(fp == NULL) die("fopen");
    
    char* line = NULL;
    ssize_t linelen = 0;
    size_t linecap = 0;
    while((linelen = getline(&line, &linecap, fp)) != -1){
        while(linelen > 0 && (line[linelen-1] == '\r' || line[linelen-1] == '\n')) linelen--;
        editorAppendRow(line, linelen);
    }
    free(line);
    fclose(fp);
}

/*** OUTPUT ***/

// draw tildes like vim does
void editorDrawRows(struct abuf* ab){
    for(int i = 0; i < E.screen_rows; i++){
        int filerow = i + E.rowoff;
        if(filerow >= E.numrows){
            if(E.numrows == 0 && i == E.screen_rows / 3){
                char welcome[80];
                int welcomeLen = snprintf(welcome, sizeof(welcome), "Kilo version --- %s", VERSION);
                if(welcomeLen > E.screen_cols) welcomeLen = E.screen_cols;
                int padding = (E.screen_cols - welcomeLen) / 2;
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
            int len = E.row[filerow].size;
            if(len >= E.screen_cols) len = E.screen_cols;
            abAppend(ab, E.row[filerow].chars, len);
        }
        // clear each line before the cursor
        abAppend(ab, "\x1b[K", 3);
        if(i < E.screen_rows - 1){
            abAppend(ab, "\r\n", 2);
        }
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

void editorRefreshScreen(void){
    struct abuf ab = ABUF_INIT;
    //turn off the cursor
    abAppend(&ab, "\x1b[?25l", 6);
    //reposition the cursor to the top
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    char buf[32];
    // position the cursor to its old position
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy+1, E.cx+1);
    abAppend(&ab, buf, strlen(buf));
    // turn on the cursor
    abAppend(&ab, "\x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}


/*** INPUT***/

void editorMoveCursor(i32 key){
    switch(key){
        case ARROW_UP:
            if(E.cy > 0) E.cy--;
            break;
        case ARROW_LEFT:
            if(E.cx > 0) E.cx--;
            break;
        case ARROW_DOWN:
            if(E.cy < E.screen_rows) E.cy++;
            break;
        case ARROW_RIGHT:
            if(E.cx < E.screen_cols) E.cx++;
            break;
    }
}

void editorProcessPress(void){
    i32 c = editorReadKey();
    switch (c) {
        case CTRL_KEY('q'):
            editorClearScreen();
            exit(0);
            break;
        case CTRL_KEY('d'):
            editorRefreshScreen();
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times = E.screen_rows;
                while(times--){
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }
            break;
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            E.cx = E.screen_cols - 1;
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
}

/*** INIT ***/
void initEditor(void){
    E.cx = 0;
    E.cy = 0;
    E.numrows = 0;
    E.rowoff = 0;
    E.row = NULL;
    if(getWindowSize(&E.screen_rows, &E.screen_cols) == -1) die("getWindowsSize");
}


int main(int argc, char** argv){
    enableRawMode();
    initEditor();
    if(argc >= 2){
        const char* filename = argv[1];
        editorOpen(filename);
    }
    while(1){
        editorRefreshScreen();
        editorProcessPress();
    }
    
    return 0;
}