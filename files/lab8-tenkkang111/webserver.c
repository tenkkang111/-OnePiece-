#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 4096        // fallback
#endif

#define PORT     8080
#define BUF_SIZE 4096

// Handle one HTTP GET request
void handle_request(int client_fd) {
    char buffer[BUF_SIZE];
    int bytes = read(client_fd, buffer, BUF_SIZE - 1); // [Fix 1] BUF_SIZE 1 → BUF_SIZE - 1
    if (bytes <= 0) {
        close(client_fd);
        return;
    }
    buffer[bytes] = '\0';
    printf("Received request:\n%s\n", buffer);

    // Parse the GET line (method and path)
    char method[8], path[256];
    sscanf(buffer, "%s %s", method, path);

    // Only allow GET requests
    if (strcmp(method, "GET") != 0) {
        const char *error = "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
        write(client_fd, error, strlen(error));
        close(client_fd);
        return;
    }

    // Default to index.html if root path is requested
    if (strcmp(path, "/") == 0)
        strcpy(path, "/index.html");

    // Construct full path to file
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "./www%s", path);

    char base_real[PATH_MAX];
    char file_real[PATH_MAX];
 
    // (1) ./www 자체의 정규 경로 획득 — 실패하면 서비스 불가 → 404
    if (realpath("./www", base_real) == NULL) {
        const char *not_found =
            "HTTP/1.1 404 Not Found\r\n\r\n"
            "<h1>404 Not Found</h1>\n";
        write(client_fd, not_found, strlen(not_found));
        close(client_fd);
        return;
    }
 
    // (2) 요청 파일의 정규 경로 획득 — 파일이 없으면 NULL → 404
    if (realpath(full_path, file_real) == NULL) {
        const char *not_found =
            "HTTP/1.1 404 Not Found\r\n\r\n"
            "<h1>404 Not Found</h1>\n";
        write(client_fd, not_found, strlen(not_found));
        close(client_fd);
        return;
    }
 
    // (3) file_real 이 base_real 로 시작하지 않으면 → 403
    //     경계 문자 검사('/'\0')로 "/www2/…" 같은 prefix 오탐도 방지
    size_t base_len = strlen(base_real);
    if (strncmp(file_real, base_real, base_len) != 0 ||
        (file_real[base_len] != '/' && file_real[base_len] != '\0')) {
        const char *forbidden =
            "HTTP/1.1 403 Forbidden\r\n\r\n"
            "<h1>403 Forbidden</h1>\n";
        write(client_fd, forbidden, strlen(forbidden));
        close(client_fd);
        return;
    }

    int file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0) {
        // Send simple 404 error response
        const char *not_found =
            "HTTP/1.1 404 Not Found\r\n\r\n"
            "<h1>404 Not Found</h1>\n";
        write(client_fd, not_found, strlen(not_found));
        close(client_fd);
        return;
    }

    // Send 200 OK response
    const char *header = "HTTP/1.1 200 OK\r\n\r\n";
    write(client_fd, header, strlen(header));

    // Send file content
    char file_buf[BUF_SIZE];
    int n;
    while ((n = read(file_fd, file_buf, BUF_SIZE)) > 0) { // [Fix 2] ) - → ) {
        write(client_fd, file_buf, n);
    }

    close(file_fd);
    close(client_fd);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind socket to port
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    // Listen for incoming connections
    listen(server_fd, 5);
    printf("Simple Web Server running at http://localhost:%d\n", PORT);

    // Accept and handle connections one by one
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        handle_request(client_fd); // [Fix 3] while 블록 안으로 이동
    }

    close(server_fd);
    return 0;
}