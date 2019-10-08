/*
 * Name: Chong Meng
 * CPSC3220 Intro to Operating System
 * system call tracer
 */

// Number of system calls in Linux : https://syscalls.kernelgrok.com/ is 338
// Get a new number of system call via ausyscall --dump is 333 (kernel linux 4.20)
// Some other resources give me different numbers, so I just use a large number 1024
#define NUMSYSCALLS 1024
// Maximim number of command line arguments.
#define NUMARGUMENTS 1024

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/reg.h>



int main(int argc, char *argv[])
{  
    // count the amount of every system call
    int counts[NUMSYSCALLS];
    // Initialize the array to 0
    memset(counts,0,sizeof(int)*NUMSYSCALLS);

    pid_t child = fork();
    if(child == 0)
    {
        // Some commandline arguments may be the combination of several commands, so use strtok to process it.
        
        // args : store the processed commandline arguments
        char *args[NUMARGUMENTS];
        // OFFSET of the pointer array 'args'
        int OFFSET = 0;
        // Set delimmer
        const char *delim = " ";
        // should start with argv[1].
        for(int i = 1;i < argc - 1; i++)
        {
            
            char *token = strtok(argv[i], delim);
            while(token != NULL)
            {
                *(args + OFFSET) = token;
                token = strtok(NULL, delim);
                OFFSET++;
            }
        }
        // args should end with a NULL
        *(args + OFFSET) = NULL;

        // Indicate this process is to be traced by its parent
        ptrace(PTRACE_TRACEME);
        // Stop myself --allow the parent to get ready to trace me 
        kill(getpid(), SIGSTOP);
         
        execvp((const char *)args[0], (char *const *)args);

    }
    else
    {
        int status, syscall_num;
        // wait for the child to stop itself
        waitpid(child, &status, 0);
        // PTRACE_O_TRACESYSGOOD : set the tracer distinguish normal traps 
        // from those caused by a system call.
        ptrace(PTRACE_SETOPTIONS, child, 0, PTRACE_O_TRACESYSGOOD);

        while(1)
        {
	        // retrieve signal at entry of syscall
            // Request: I want to wait for a system call
            ptrace(PTRACE_SYSCALL, child, 0, 0);

            // wait for child status to change
            waitpid(child, &status, 0);

            // if the child terminate normally
            if (WIFEXITED(status))
            {
                // write results to output file named argv[argc-1]
                FILE *fp = fopen(argv[argc - 1], "w");

                for (int i = 0; i < NUMSYSCALLS; i++)
                {
                    if (counts[i] != 0)
                    {
                        fprintf(fp,"%d\t%d\n", i, counts[i]);
                    }
                }
                fclose(fp);
                // exit or return from  main
                return 0;
            }
            
            // if the child was stopped by delivery of 
            // signal (system call)
            if ((WIFSTOPPED(status)&&WSTOPSIG(status) & 0x80))
            {
                // read out the saved value of the RAX register, which is the system call number
                syscall_num = ptrace(PTRACE_PEEKUSER, child, sizeof(long)*ORIG_RAX, NULL);
                counts[syscall_num]++;
                // retrieve syscall at the exit of system call but do not count it
                ptrace(PTRACE_SYSCALL, child, 0, 0);
                waitpid(child, &status, 0);
            }

        }

    }
    
}
