#include "../include/io.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../include/terminal.h"
#include "../include/row.h"
#include "../include/syntax.h"

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
