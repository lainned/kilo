/*** INCLUDES ***/
#include <asm-generic/errno-base.h>
#include <asm-generic/ioctls.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <errno.h>

/*** DEFINES ***/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** DATA ***/

struct editorConfig{
    int screen_rows;
    int screen_cols;
    struct termios orig_termios;
};

struct editorConfig E;


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

char editorReadKey(){
    char c = '\0';
    int nread;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if(nread == -1 && errno != EAGAIN) die("read");
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

/*** OUTPUT ***/

// draw tildes like vim does
void editorDrawRows(void){
    for(int i = 0; i < E.screen_rows; i++){
        write(STDOUT_FILENO, "~\r\n", 3);
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
    editorClearScreen();
    editorDrawRows();
    write(STDOUT_FILENO, "\x1b[H", 3);
}


/*** INPUT***/

void editorProcessPress(){
    char c = editorReadKey();
    switch (c) {
        case CTRL_KEY('q'):
            editorClearScreen();
            exit(0);
            break;
        case CTRL_KEY('d'):
            editorRefreshScreen();
            break;
    }
}

/*** INIT ***/
void initEditor(void){
    if(getWindowSize(&E.screen_rows, &E.screen_cols) == -1) die("getWindowsSize");
}


int main(void){
    enableRawMode();
    initEditor();
    while(1){
        editorRefreshScreen();
        editorProcessPress();
    }
    
    return 0;
}