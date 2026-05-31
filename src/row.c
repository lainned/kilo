#include "../include/row.h"
#include "../include/syntax.h"
#include <stdlib.h>
#include <string.h>

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