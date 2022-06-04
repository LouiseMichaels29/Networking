#include <stdio.h>
#include <string.h>
#include "proj.h"

//This method simply works as an fgets wrapped function. 
int read_stdin(char *buf, int buf_len, int *more) {

    bzero(buf, buf_len);
    if (fgets(buf, buf_len, stdin) != NULL) {
        
        *more = strchr(buf, '\n') == NULL;
        return strlen(buf) > 0;
    }
    
    return 0;
}


