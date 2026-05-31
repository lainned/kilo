#include "../include/terminal.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

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
    i32 nread;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if(nread == -1 && errno != EAGAIN){
            if(errno == EINTR && window_resized){
                editorRefreshSize();
                editorRefreshScreen();
                window_resized = 0;
                continue;
            }
            die("read");
        }
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

i32 getCursorPosition(i32* rows, i32* cols){
    char buf[32];
    u32 i = 0;
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

i32 getWindowSize(i32* rows, i32* cols){
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
