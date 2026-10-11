#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <ctype.h>

#define MAX_THREADS 16

typedef struct {
    char *buffer;
    long start;
    long end;
    int count;
} ThreadArg;

int is_word_char(char c) {
    return isalnum(c);
}

void* count_words(void *arg) {
    ThreadArg *targ = (ThreadArg*)arg;
    int count = 0;
    int in_word = 0;

    for (long i = targ->start; i < targ->end; i++) {
        if (is_word_char(targ->buffer[i])) {
            if (!in_word) { count++; in_word = 1; }
        } else {
            in_word = 0;
        }
    }
    targ->count = count;
    return NULL;
}

double time_diff_ms(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 + 
           (end.tv_usec - start.tv_usec) / 1000.0;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <filename> <num_threads>\n", argv[0]);
        return 1;
    }
    
    struct timeval total_start, total_end;
    gettimeofday(&total_start, NULL);

    char* filename = argv[1];
    int num_threads = atoi(argv[2]);

    if (num_threads <= 0 || num_threads > MAX_THREADS) {
        printf("Thread count must be between 1 and %d\n", MAX_THREADS);
        return 1;
    }

    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return 1;
    }
    

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);


    struct timeval io_start, io_end;
    gettimeofday(&io_start, NULL);

    char* buffer = malloc(size + 1);
    size_t read_size = fread(buffer, 1, size, file);
    if (read_size < (size_t)size) {
        if (ferror(file)) {
            perror("Error reading file");
        }
    }
    buffer[read_size] = '\0';
    buffer[size] = '\0';

    gettimeofday(&io_end, NULL);
    fclose(file);

    struct timeval wc_start, wc_end;
    gettimeofday(&wc_start, NULL);

    pthread_t threads[MAX_THREADS];
    ThreadArg args[MAX_THREADS];
    long block = size / num_threads;

    for(int i = 0; i < num_threads; i++) {
        args[i].buffer = buffer;
        args[i].start = i * block;
        args[i].end = (i == num_threads - 1) ? size : (i + 1) * block;
        if(i != 0) {
            while(args[i].start < size && is_word_char(buffer[args[i].start])) {
                args[i].start++;
            }
        }

        if(i != num_threads - 1) {
            while(args[i].end < size && is_word_char(buffer[args[i].end])) {
                args[i].end++;
            }
        }

        pthread_create(&threads[i], NULL, count_words, &args[i]);
    }

    int total = 0;
    for(int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total += args[i].count;
    }

    gettimeofday(&wc_end, NULL);

    free(buffer);

    gettimeofday(&total_end, NULL);

    double io_time = time_diff_ms(io_start, io_end);
    double wc_time = time_diff_ms(wc_start, wc_end);
    double total_time = time_diff_ms(total_start, total_end);

    printf("Total words: %d\n", total);
    printf("Elapsed time (total): %.2f ms\n", total_time);
    printf("  I/O time:           %.2f ms\n", io_time);
    printf("  Word count time:    %.2f ms\n", wc_time);

    return 0;
}