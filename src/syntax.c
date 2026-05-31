#include "../include/syntax.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>


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