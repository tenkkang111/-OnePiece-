#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

#define MAX_LENG 1024
int main(){
    char password[MAX_LENG];
    int index = 0;
    char ch;
    struct termios oldt, newt;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    printf("Enter your password: ");
    while(index < MAX_LENG - 1){
        ch = getchar();
        if (ch == '\n' || ch == '\r'){
            break;
        }
        else if (ch == 127 || ch == 8){
            if(index > 0){
                index--;
                printf("\b \b");
                fflush(stdout);
            }
        }
        else{
            password[index++] = ch;
            printf("*");
            fflush(stdout);
        }
    }

    password[index] = '\0';
    printf("\n");


    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("Password entered: %s\n", password);
    return 0;
}

