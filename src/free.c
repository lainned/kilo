#include "../include/common.h"
#include <stdlib.h>
#include "../include/free.h"

void editorFree(void){
    for(i32 i = 0; i < E.numrows; i++){
        free(E.row[i].chars);
        free(E.row[i].render);
        free(E.row[i].hl);
    }
    free(E.row);
    free(E.filename);
}