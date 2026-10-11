#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <termios.h>

#define MAX_INPUT 1024
#define RED "\033[31m"
#define RESET "\033[0m"

int main(int argc, char *argv[]){
    if(argc != 2) {
        fprintf(stderr, "Usage: %s \"target sentence\"\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *target = argv[1];
    char input[MAX_INPUT];
    int target_len = strlen(target);
    int index = 0;
    char ch;

    printf("Type the following sentence:\n%s\n", target);
    printf("\nStart typing: ");
    fflush(stdout);

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    struct timeval start, end;
    gettimeofday(&start, NULL);


    while(1){
        ch = getchar();
        if (ch == '\n' || ch == '\r'){
            break;
        } else if (ch == 127 || ch == 8){
            if(index > 0){
                index--;
                printf("\b \b");
                fflush(stdout);
            }
        } else {
            if(ch == target[index]){
                printf("%c", ch);
                fflush(stdout);
            } else {
                printf(RED "%c" RESET, ch);
                fflush(stdout);
            }
            input[index++] = ch;
        }
        if (index == target_len){
            break;
        }
    }

    gettimeofday(&end, NULL);

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    double elapsed = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_usec - start.tv_usec) / 1e6;

    int correct = 0;
    for (int i = 0; i < index && i < target_len; i++){
        if (input[i] == target[i]) correct++;
    }

    double accuracy = 100.0 * correct / target_len;
    double speed = index / elapsed;

    printf("\n\n=== Result ===\n");
    printf("Time taken: %.2f seconds\n", elapsed);
    printf("Typing speed: %.2f chars/sec\n", speed);
    printf("Accuracy: %.2f%%\n", accuracy);

    
    return 0;
}
