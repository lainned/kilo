/*** INCLUDES ***/
#include <asm-generic/errno-base.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

/*** DEFINES ***/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** DATA ***/
//test
struct termios orig_termios;

/*** TERMINAL ***/

void die(const char* c){
    perror(c);
    exit(1);
}

void disableRawMode(void){
    if( tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) die("tcsetattr");
}

void enableRawMode(void){
    // reading the attributes to orig_termios struct
    if(tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);
    struct termios raw = orig_termios;
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
/*** OUTPUT ***/

void editorRefreshScreen(){
    write(STDOUT_FILENO, "\x1b[2J", 4);
}
/*** INPUT***/

void editorProcessPress(){
    char c = editorReadKey();
    switch (c) {
        case CTRL_KEY('q'):
            exit(0);
            break;
        case CTRL_KEY('d'):
            editorRefreshScreen();
            break;
    }
}

/*** INIT ***/

int main(void){
    enableRawMode();
    while(1){
        editorProcessPress();
    }

    return 0;
}