#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sqlite3.h>
#include <zlib.h>

#define PORT 12345
#define BUF_SIZE 1024

sqlite3 *db;

void error_handling(char *message) {
    perror(message);
    exit(1);
}

// Base64编码表和解码表
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const unsigned char base64_inv[256] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
    64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
};

// Base64解码函数
unsigned char *base64_decode(const char *input, int *output_len) {
    int len = strlen(input);
    int i, j;
    if (len % 4 != 0) return NULL;

    *output_len = len / 4 * 3;
    if (input[len - 1] == '=') (*output_len)--;
    if (input[len - 2] == '=') (*output_len)--;

    unsigned char *decoded_data = (unsigned char *)malloc(*output_len);
    if (decoded_data == NULL) return NULL;

    for (i = 0, j = 0; i < len;) {
        uint32_t sextet_a = input[i] == '=' ? 0 & i++ : base64_inv[(int)input[i++]];
        uint32_t sextet_b = input[i] == '=' ? 0 & i++ : base64_inv[(int)input[i++]];
        uint32_t sextet_c = input[i] == '=' ? 0 & i++ : base64_inv[(int)input[i++]];
        uint32_t sextet_d = input[i] == '=' ? 0 & i++ : base64_inv[(int)input[i++]];

        uint32_t triple = (sextet_a << 3 * 6)
                        + (sextet_b << 2 * 6)
                        + (sextet_c << 1 * 6)
                        + (sextet_d << 0 * 6);

        if (j < *output_len) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < *output_len) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < *output_len) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
    }

    return decoded_data;
}

// 处理客户端请求的线程函数
void *handle_client(void *arg) {
    int client_sock = *((int *)arg);
    free(arg);
    char buf[BUF_SIZE];
    int str_len;

    while ((str_len = read(client_sock, buf, BUF_SIZE)) != 0) {
        if (strncmp(buf, "CHECK_VERSION", 13) == 0) {
            // 版本检查逻辑
            FILE *fp = fopen("v.txt", "r");
            if (fp == NULL) {
                error_handling("Failed to open version file");
            }
            fgets(buf, BUF_SIZE, fp);
            fclose(fp);
            write(client_sock, buf, strlen(buf));
        } else if (strncmp(buf, "GET_NEW_CLIENT", 14) == 0) {
            // 发送新的客户端程序
            FILE *fp = fopen("client_new", "rb");
            if (fp == NULL) {
                error_handling("Failed to open new client file");
            }
            while ((str_len = fread(buf, 1, BUF_SIZE, fp)) > 0) {
                if (write(client_sock, buf, str_len) == -1) {
                    error_handling("write() error");
                }
            }
            fclose(fp);
        } else {
            // 数据接收与处理逻辑
            int decoded_len;
            unsigned char *decoded_data = base64_decode(buf, &decoded_len);
            if (decoded_data == NULL) {
                error_handling("Base64 decode error");
            }
            unsigned long crc = crc32(0L, Z_NULL, 0);
            crc = crc32(crc, (const unsigned char *)buf, str_len);

            printf("Received data: %s, CRC32: %lu\n", decoded_data, crc);

            // 存储到SQLite数据库
            sqlite3_stmt *stmt;
            sqlite3_prepare_v2(db, "INSERT INTO data (field_data) VALUES (?)", -1, &stmt, NULL);
            sqlite3_bind_text(stmt, 1, (const char *)decoded_data, -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            free(decoded_data);
        }
    }
    close(client_sock);
    return NULL;
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size;
    pthread_t t_id;

    // 打开SQLite数据库
    if (sqlite3_open("server_data.db", &db) != SQLITE_OK) {
        error_handling("Can't open database");
    }
    // 创建数据表
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS data (id INTEGER PRIMARY KEY, field_data TEXT)", 0, 0, 0);

    // 创建套接字
    server_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        error_handling("socket() error");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    // 绑定套接字
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        error_handling("bind() error");
    }

    // 监听连接
    if (listen(server_sock, 5) == -1) {
        error_handling("listen() error");
    }

    while (1) {
        client_addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_sock == -1) {
            error_handling("accept() error");
        }

        int *pclient = malloc(sizeof(int));
        *pclient = client_sock;
        pthread_create(&t_id, NULL, handle_client, pclient);
        pthread_detach(t_id);
    }

    close(server_sock);
    sqlite3_close(db);
    return 0;
}
