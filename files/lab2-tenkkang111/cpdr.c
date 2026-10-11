#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

void copy_file(const char *src, const char *dest) {
    int fd_src, fd_dest;
    struct stat st;
    char buffer[4096];
    ssize_t bytes_read, bytes_written;

    fd_src = open(src, O_RDONLY);
    if (fd_src < 0) {
        perror("open src");
        return;
    }

    if (fstat(fd_src, &st) < 0) {
        perror("fstat src");
        close(fd_src);
        return;
    }

    fd_dest = open(dest, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (fd_dest < 0) {
        perror("open dest");
        close(fd_src);
        return;
    }

    while ((bytes_read = read(fd_src, buffer, sizeof(buffer))) > 0) {
        ssize_t written_total = 0;
        while (written_total < bytes_read) {
            bytes_written = write(fd_dest, buffer + written_total, bytes_read - written_total);
            if (bytes_written < 0) {
                perror("write error");
                close(fd_src);
                close(fd_dest);
                return;
            }
            written_total += bytes_written;
        }
    }

    if (bytes_read < 0) {
        perror("read error");
    }

    if (fchmod(fd_dest, st.st_mode) < 0) {
        perror("fchmod dest");
    }

    close(fd_src);
    close(fd_dest);
}

void copy_dir(const char *src, const char *dest) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char src_path[1024];
    char dest_path[1024];

    if (stat(src, &st) < 0) {
        perror("stat src dir");
        return;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s is not a directory\n", src);
        return;
    }

    if (mkdir(dest, st.st_mode) < 0) {
        if (errno != EEXIST) {
            perror("mkdir dest");
            return;
        }
    }
    
    if (chmod(dest, st.st_mode) < 0) {
        perror("chmod dest");
    }

    dir = opendir(src);
    if (!dir) {
        perror("opendir src");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name) >= sizeof(src_path) ||
            snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, entry->d_name) >= sizeof(dest_path)) {
            fprintf(stderr, "Warning: Path truncated for %s\n", entry->d_name);
            continue;
        }

        if (lstat(src_path, &st) < 0) {
            perror("lstat entry");
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            copy_dir(src_path, dest_path);
        } else if (S_ISREG(st.st_mode)) {
            copy_file(src_path, dest_path);
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source_dir> <destination_dir>\n", argv[0]);
        return EXIT_FAILURE;
    }

    copy_dir(argv[1], argv[2]);

    return EXIT_SUCCESS;
}