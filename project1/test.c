#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[])
{  
    for(int i=0;i<argc;i++){
        printf("%d\t%s\n",i,argv[i]);
    }
    const char *delim = " ";
    char *token = strtok(argv[2], " ");
    char **args = NULL;
        // OFFSET of the pointer array 'args'
    int OFFSET = 0;
    *(args + OFFSET) = token;
    printf("%s\n", *(args+OFFSET));    
    while(token != NULL)
    {
        token = strtok(NULL, delim);
        OFFSET++;
        *(args + OFFSET) = token;
        printf("%p\n", *(args+OFFSET));
    }
}