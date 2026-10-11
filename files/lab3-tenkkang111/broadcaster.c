#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#define PTS_PATH "/dev/pts/"
#define MAX_PATH 256

#define MAX_MSG 1024

int main(int argc, char *argv[]){
    if (argc < 2){
        fprintf(stderr, "Usage: %s <message>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *message = argv[1];
    char broadcast_msg[MAX_MSG];
    char broadcast_path[MAX_PATH];

    snprintf(broadcast_msg, sizeof(broadcast_msg), "[Broadcast] %s\n", message);

    DIR *dir = opendir(PTS_PATH);
    if (dir == NULL) {
        return 1;
    }

    struct dirent *entry;
    FILE * fp;

    while ((entry = readdir(dir)) != NULL){
        if(!isdigit(entry->d_name[0])){
            continue;
        }
        snprintf(broadcast_path, sizeof(broadcast_path), "%s%s", PTS_PATH, entry->d_name);
        fp = fopen(broadcast_path, "w");
        if (fp == NULL){
            continue;
        }
        else {
            fprintf(fp, "%s", broadcast_msg);
            fclose(fp);
        }
    }

    closedir(dir);
    return 0;
}