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

// Base64 编码/解码函数
// ... (Base64 encode and decode functions here)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const unsigned char base64_inv[256] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, // 0-15
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, // 16-31
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, // 32-47 ('+', '/')
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64, // 48-63 ('0'-'9')
    64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,           // 64-79 ('A'-'O')
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,  // 80-95 ('P'-'Z')
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,  // 96-111 ('a'-'o')
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,  // 112-127 ('p'-'z')
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,  // 128-143
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,  // 144-159
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,  // 160-175
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,  // 176-191
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,  // 192-207
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,  // 208-223
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,  // 224-239
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64   // 240-255
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
    if (len % 4 != 0) {
        fprintf(stderr, "Input length is not a multiple of 4\n");
        return NULL;
    }
    *output_len = len / 4 * 3;
    if (input[len - 1] == '=') (*output_len)--;
    if (input[len - 2] == '=') (*output_len)--;
    unsigned char *decoded_data = (unsigned char *)malloc(*output_len + 1);
    if (decoded_data == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }
    for (int i = 0, j = 0; i < len;) {
        uint32_t sextet_a = input[i] == '=' ? 0 & i++ : base64_inv[(unsigned char)input[i++]];
        uint32_t sextet_b = input[i] == '=' ? 0 & i++ : base64_inv[(unsigned char)input[i++]];
        uint32_t sextet_c = input[i] == '=' ? 0 & i++ : base64_inv[(unsigned char)input[i++]];
        uint32_t sextet_d = input[i] == '=' ? 0 & i++ : base64_inv[(unsigned char)input[i++]];
        
        if (sextet_a == 64 || sextet_b == 64 || sextet_c == 64 || sextet_d == 64) {
            fprintf(stderr, "Invalid Base64 character detected\n");
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

// 检查版本
void check_version(int client_sock) {
    char buf[BUF_SIZE];
    FILE *fp = fopen("v.txt", "r");
    if (fp == NULL) {
        error_handling("Failed to open version file");
    }
    fgets(buf, BUF_SIZE, fp);
    fclose(fp);
    printf("Sending server version: %s\n", buf);
    write(client_sock, buf, strlen(buf));
}

// 发送新客户端程序
void send_new_client(int client_sock) {
    char buf[BUF_SIZE];
    FILE *fp = fopen("client_new", "rb");
    if (fp == NULL) {
        error_handling("Failed to open new client file");
    }
    int str_len;
    while ((str_len = fread(buf, 1, BUF_SIZE, fp)) > 0) {
        if (write(client_sock, buf, str_len) == -1) {
            error_handling("write() error");
        }
    }
    printf("Sent new client to client.\n");
    fclose(fp);
}

// 处理客户端请求的线程函数
void *handle_client(void *arg) {
    int client_sock = *((int *)arg);
    free(arg);
    char buf[BUF_SIZE];
    int str_len;

    while ((str_len = read(client_sock, buf, BUF_SIZE)) != 0) {
        buf[str_len] = '\0';  // Null-terminate the received data
        printf("Received encoded data: %s\n", buf);

        if (strncmp(buf, "CHECK_VERSION", 13) == 0) {
            check_version(client_sock);
        } else if (strncmp(buf, "GET_NEW_CLIENT", 14) == 0) {
            send_new_client(client_sock);
        } else {
            // 数据接收与处理逻辑
            int decoded_len;
            unsigned char *decoded_data = base64_decode(buf, &decoded_len);
            if (decoded_data == NULL) {
                fprintf(stderr, "Base64 decode error for data: %s\n", buf);
                continue;  // 出错时继续处理下一个数据包
            }
            unsigned long crc = crc32(0L, Z_NULL, 0);
            crc = crc32(crc, decoded_data, decoded_len);

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

    printf("Server is running and waiting for connections...\n");

    while (1) {
        client_addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_sock == -1) {
            error_handling("accept() error");
        }

        printf("Client connected.\n");

        int *pclient = malloc(sizeof(int));
        *pclient = client_sock;
        pthread_create(&t_id, NULL, handle_client, pclient);
        pthread_detach(t_id);
    }

    close(server_sock);
    sqlite3_close(db);
    return 0;
}
