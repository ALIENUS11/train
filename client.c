#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <zlib.h>
#include <curl/curl.h>

// Base64 编码/解码函数
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const unsigned char base64_inv[256] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64,
    64, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 0,
    64, 64, 64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64,
    64, 64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
};

char *base64_encode(const unsigned char *input, int len) {
    int output_len = 4 * ((len + 2) / 3);
    char *encoded_data = (char *)malloc(output_len + 1);
    if (encoded_data == NULL) return NULL;
    for (int i = 0, j = 0; i < len;) {
        uint32_t octet_a = i < len ? input[i++] : 0;
        uint32_t octet_b = i < len ? input[i++] : 0;
        uint32_t octet_c = i < len ? input[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        encoded_data[j++] = base64_chars[(triple >> 18) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 12) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 6) & 0x3F];
        encoded_data[j++] = base64_chars[triple & 0x3F];
    }
    for (int i = 0; i < 3 - (len % 3) && len % 3 != 0; i++)
        encoded_data[output_len - 1 - i] = '=';
    encoded_data[output_len] = '\0';
    return encoded_data;
}

unsigned char *base64_decode(const char *input, int *output_len) {
    int len = strlen(input);
    if (len % 4 != 0) return NULL;
    *output_len = len / 4 * 3;
    if (input[len - 1] == '=') (*output_len)--;
    if (input[len - 2] == '=') (*output_len)--;
    unsigned char *decoded_data = (unsigned char *)malloc(*output_len + 1);
    if (decoded_data == NULL) return NULL;
    for (int i = 0, j = 0; i < len;) {
        uint32_t sextet_a = input[i] == '=' ? 0 & i++ : base64_inv[(unsigned char)input[i++]];
        uint32_t sextet_b = input[i] == '=' ? 0 & i++ : base64_inv[(unsigned char)input[i++]];
        uint32_t sextet_c = input[i] == '=' ? 0 & i++ : base64_inv[(unsigned char)input[i++]];
        uint32_t sextet_d = input[i] == '=' ? 0 & i++ : base64_inv[(unsigned char)input[i++]];
        if (sextet_a == 64 || sextet_b == 64 || sextet_c == 64 || sextet_d == 64) {
            free(decoded_data);
            return NULL;
        }
        uint32_t triple = (sextet_a << 18) + (sextet_b << 12) + (sextet_c << 6) + sextet_d;
        if (j < *output_len) decoded_data[j++] = (triple >> 16) & 0xFF;
        if (j < *output_len) decoded_data[j++] = (triple >> 8) & 0xFF;
        if (j < *output_len) decoded_data[j++] = triple & 0xFF;
    }
    decoded_data[*output_len] = '\0';
    return decoded_data;
}

#define PORT 12345
#define BUF_SIZE 1024
#define VERSION_FILE "version.txt"
#define NEW_CLIENT_FILE "client_new"

// 错误处理函数
void error_handling(char *message) {
    perror(message);
    exit(1);
}

// 检查并下载更新的客户端程序
void check_for_update() {
    int sock;
    struct sockaddr_in serv_addr;
    char message[BUF_SIZE];
    FILE *fp;
    char server_version[BUF_SIZE];

    // 创建套接字
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        error_handling("socket() error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(PORT);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        error_handling("connect() error");
    }

    // 发送版本检查请求
    write(sock, "CHECK_VERSION", strlen("CHECK_VERSION"));
    int str_len = read(sock, server_version, BUF_SIZE - 1);
    server_version[str_len] = 0;

    // 打开本地版本文件
    fp = fopen(VERSION_FILE, "r");
    if (fp == NULL) {
        error_handling("Failed to open version file");
    }
    char local_version[BUF_SIZE];
    fgets(local_version, BUF_SIZE, fp);
    fclose(fp);

    // 检查版本是否一致
    if (strcmp(server_version, local_version) != 0) {
        printf("New version available. Downloading...\n");
        write(sock, "GET_NEW_CLIENT", strlen("GET_NEW_CLIENT"));
        fp = fopen(NEW_CLIENT_FILE, "wb");
        if (fp == NULL) {
            error_handling("Failed to open new client file for writing");
        }
        while ((str_len = read(sock, message, BUF_SIZE)) > 0) {
            fwrite(message, 1, str_len, fp);
        }
        fclose(fp);
        // 更新版本文件
        fp = fopen(VERSION_FILE, "w");
        if (fp == NULL) {
            error_handling("Failed to open version file for writing");
        }
        fputs(server_version, fp);
        fclose(fp);
        printf("Update downloaded. Please restart the client.\n");
        close(sock);
        exit(0);
    }
    printf("Client is up to date.\n");
    close(sock);
}

// 发送数据的线程函数
void *send_data(void *arg) {
    int sock;
    struct sockaddr_in serv_addr;
    char message[BUF_SIZE];
    unsigned long crc;

    // 创建套接字
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        error_handling("socket() error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(PORT);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        error_handling("connect() error");
    }

    while (1) {
        printf("Enter data to send: ");
        fgets(message, BUF_SIZE, stdin);
        message[strlen(message) - 1] = 0;

        crc = crc32(0L, Z_NULL, 0);
        crc = crc32(crc, (const unsigned char *)message, strlen(message));
        printf("Sending data: %s, CRC32: %lu\n", message, crc);

        // Base64编码
        char *encoded_data = base64_encode((const unsigned char *)message, strlen(message));
        if (encoded_data == NULL) {
            error_handling("Base64 encode error");
        }

        write(sock, encoded_data, strlen(encoded_data));
        free(encoded_data);
    }
    close(sock);
    return NULL;
}

int main() {
    pthread_t send_thread;

    // 检查更新
    check_for_update();

    // 创建数据发送线程
    if (pthread_create(&send_thread, NULL, send_data, NULL) != 0) {
        error_handling("pthread_create() error");
    }
    pthread_join(send_thread, NULL);

    return 0;
}
