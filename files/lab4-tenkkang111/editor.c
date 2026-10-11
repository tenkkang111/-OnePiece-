#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_LINES 100
#define MAX_LINE_LEN 256
#define AUTOSAVE_FILE "autosave.txt"

char *lines[MAX_LINES];
int line_count = 0;

void handle_signal(int sig) {
    int fd = open(AUTOSAVE_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd != -1) {
        for (int i = 0; i < line_count; i++) {
            write(fd, lines[i], strlen(lines[i]));
        }
        close(fd);
    }

    if (sig == SIGALRM) {
        alarm(5);
    } else if (sig == SIGINT) {
        for (int i = 0; i < line_count; i++) {
            free(lines[i]);
        }
        exit(0);
    }
}

int main() {
    signal(SIGALRM, handle_signal);
    signal(SIGINT, handle_signal);
    
    alarm(5);

    printf("Enter text (Ctrl+C to quit):\n");

    char buffer[MAX_LINE_LEN];
    
    while (1) {
        if (fgets(buffer, MAX_LINE_LEN, stdin) != NULL) {
            if (line_count < MAX_LINES) {
                lines[line_count] = strdup(buffer);
                line_count++;
            } else {
                printf("Reached maximum number of lines.\n");
            }
        } else {
            clearerr(stdin);
        }
    }

    return 0;
}