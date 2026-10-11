#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAXARGS 20      // Maximum number of command line arguments
#define ARGLEN 100      // Maximum length of each argument

void execute(char *arglist[]);
char *makestring(char *buf);

int main() {
    char *arglist[MAXARGS + 1]; // Array of pointers to arguments
    int numargs = 0;            // Argument counter
    char argbuf[ARGLEN];        // Buffer for input

    signal(SIGINT, SIG_IGN);

    while (numargs < MAXARGS) {
        printf("Arg[%d]? ", numargs);

        if (fgets(argbuf, ARGLEN, stdin) && *argbuf != '\n') {
            argbuf[strcspn(argbuf, "\n")] = '\0'; // Remove newline safely

            if (numargs == 0 && strcmp(argbuf, "exit") == 0) {
                printf("Exiting shell.\n");
                exit(0);
            }

            arglist[numargs++] = makestring(argbuf);

        } else {
            if (numargs > 0) {  // If there are arguments
                arglist[numargs] = NULL; // Null-terminate the list
                
                execute(arglist); // Execute the command

                for (int i = 0; i < numargs; i++) {
                    free(arglist[i]);
                }
                numargs = 0; // Reset for next command
            }
        }
    }
    return 0;
}

void execute(char *arglist[]) {
    pid_t pid;
    int exitstatus;

    pid = fork(); 

    if (pid < 0) {
        perror("fork failed");
        exit(1);
    } else if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        execvp(arglist[0], arglist);
        perror("execvp failed");
        exit(1);
    } else {
        while (wait(&exitstatus) != pid)
            ; 
        
        printf("Child exited with status %d, signal %d\n", 
               (exitstatus >> 8) & 0xFF, exitstatus & 0x7F);
    }
}

char *makestring(char *buf) {
    char *cp = malloc(strlen(buf) + 1);
    
    if (cp == NULL) {
        fprintf(stderr, "no memory\n");
        exit(1);
    }
    
    strcpy(cp, buf);
    return cp;
}