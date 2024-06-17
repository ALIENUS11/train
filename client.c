#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define PORT 33333

// Base64 解码函数
unsigned char *base64_decode(const char *data, size_t input_length, size_t *output_length) {
    static const unsigned char decoding_table[] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 0, 64, 64,
        64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
        64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
        40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64
    };

    if (input_length % 4 != 0) return NULL;

    *output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=') (*output_length)--;
    if (data[input_length - 2] == '=') (*output_length)--;

    unsigned char *decoded_data = malloc(*output_length);
    if (decoded_data == NULL) return NULL;

    for (size_t i = 0, j = 0; i < input_length;) {
        uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[(unsigned char)data[i++]];
        uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[(unsigned char)data[i++]];
        uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[(unsigned char)data[i++]];
        uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[(unsigned char)data[i++]];

        uint32_t triple = (sextet_a << 18) + (sextet_b << 12) + (sextet_c << 6) + sextet_d;

        if (j < *output_length) decoded_data[j++] = (triple >> 16) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> 8) & 0xFF;
        if (j < *output_length) decoded_data[j++] = triple & 0xFF;
    }

    return decoded_data;
}

// 获取当前时间字符串
void current_time_str(char *buffer, size_t size) {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", timeinfo);
}

// 将消息写入文件
void write_message_to_file(const char *fromname, const char *toname, const char *message) {
    FILE *file = fopen("messages.txt", "a");
    if (!file) {
        perror("Failed to open messages file");
        return;
    }

    char time_buffer[64];
    current_time_str(time_buffer, sizeof(time_buffer));
    fprintf(file, "Time: %s\nFrom: %s\nTo: %s\nMessage: %s\n\n", time_buffer, fromname, toname, message);
    fclose(file);
}

// 处理客户端连接的线程函数
void *handle_client(void *arg) {
    int client_sock = *((int *)arg);
    free(arg);
    char buffer[4096];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_sock, buffer, sizeof(buffer), 0);

        if (bytes_received <= 0) {
            printf("Client disconnected or error occurred.\n");
            break;
        }

        char *fromname = strtok(buffer, "|");
        char *toname = strtok(NULL, "|");
        char *encoded_message = strtok(NULL, "|");

        if (!fromname || !toname || !encoded_message) {
            printf("Received invalid message format.\n");
            continue;
        }

        size_t decoded_length;
        unsigned char *decoded_message = base64_decode(encoded_message, strlen(encoded_message), &decoded_length);

        if (!decoded_message) {
            printf("Failed to decode Base64 message.\n");
            continue;
        }

        write_message_to_file(fromname, toname, (char *)decoded_message);
        printf("Message from %s to %s: %s\n", fromname, toname, decoded_message);

        free(decoded_message);
    }

    close(client_sock);
    return NULL;
}

// 启动服务端的主函数
int main() {
    int server_sock, *new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Socket bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 3) < 0) {
        perror("Socket listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        client_addr_len = sizeof(client_addr);
        new_sock = malloc(sizeof(int));

        if ((*new_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
            perror("Server accept failed");
            free(new_sock);
            continue;
        }

        printf("New client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, (void *)new_sock) != 0) {
            perror("Could not create thread");
            free(new_sock);
        }
        pthread_detach(client_thread); // 自动回收线程资源
    }

    close(server_sock);
    return 0;
}