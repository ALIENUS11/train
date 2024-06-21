#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <zlib.h>

#define PORT 12345
#define BUF_SIZE 1024
#define VERSION_FILE "version.txt"
#define NEW_CLIENT_FILE "client_new"

// 错误处理函数
void error_handling(char *message) {
    perror(message);
    exit(1);
}

// Base64编码表和解码表
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const int mod_table[] = {0, 2, 1};

// Base64编码函数
char *base64_encode(const unsigned char *input, int len) {
    int output_len = 4 * ((len + 2) / 3);
    char *encoded_data = (char *)malloc(output_len + 1);
    if (encoded_data == NULL) return NULL;

    for (int i = 0, j = 0; i < len;) {
        uint32_t octet_a = i < len ? input[i++] : 0;
        uint32_t octet_b = i < len ? input[i++] : 0;
        uint32_t octet_c = i < len ? input[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = base64_chars[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 0 * 6) & 0x3F];
    }

    for (int i = 0; i < mod_table[len % 3]; i++)
        encoded_data[output_len - 1 - i] = '=';

    encoded_data[output_len] = '\0';
    return encoded_data;
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
