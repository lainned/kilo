#include "../include/input.h"
#include "../include/terminal.h"
#include "../include/output.h"
#include "../include/row.h"
#include <stdlib.h>

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
