/*
 * Name: Chong Meng
 * CPSC3220 Intro to Operating System
 * Detect memory leak of a process
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[])
{
    // Set the LD_PRELOAD environment variable
    char *const env[2] = {"LD_PRELOAD=./memory_shim.so", NULL};
    // Set the arguments vector. end with NULL
    char *args[argc];
    args[argc-1] = NULL;
    // copy arguments into args
    memcpy(args, argv+1, (argc-1)*sizeof(char *));
    
    // fork + execvpe
    if (fork() == 0)
    {
        int i = execvpe((const char *)args[0],(char *const *)args, env);
        if (i < 0)
            perror("execvpe");
    }


}
