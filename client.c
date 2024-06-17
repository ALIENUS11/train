#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 33333
#define SERVER_ADDRESS "127.0.0.1" // 修改为实际服务器IP地址

// Base64编码函数，简化了实现，可以根据需要替换为更完善的版本
char *base64_encode(const unsigned char *data, size_t input_length, size_t *output_length) {
    static const char encoding_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    static const char padding_char = '=';
    *output_length = 4 * ((input_length + 2) / 3);

    char *encoded_data = malloc(*output_length + 1);
    if (encoded_data == NULL) return NULL;

    for (size_t i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        encoded_data[j++] = encoding_table[(triple >> 18) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 12) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 6) & 0x3F];
        encoded_data[j++] = encoding_table[triple & 0x3F];
    }

    for (size_t i = 0; i < (input_length % 3); i++) {
        encoded_data[*output_length - 1 - i] = padding_char;
    }

    encoded_data[*output_length] = '\0';
    return encoded_data;
}

// 发送消息到服务器
void send_message(const char *fromname, const char *toname, const char *message) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[4096];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to server failed");
        close(sock);
        return;
    }

    size_t encoded_length;
    char *encoded_message = base64_encode((const unsigned char *)message, strlen(message), &encoded_length);

    snprintf(buffer, sizeof(buffer), "%s|%s|%s", fromname, toname, encoded_message);

    send(sock, buffer, strlen(buffer), 0);

    printf("Message sent\n");

    free(encoded_message);
    close(sock);
}

// 主函数
int main() {
    char fromname[256];
    char toname[256];
    char message[1024];

    printf("Enter your name: ");
    fgets(fromname, sizeof(fromname), stdin);
    fromname[strcspn(fromname, "\n")] = '\0'; // Remove newline character

    printf("Enter recipient name: ");
    fgets(toname, sizeof(toname), stdin);
    toname[strcspn(toname, "\n")] = '\0'; // Remove newline character

    while (1) {
        printf("Enter message (or 'exit' to quit): ");
        fgets(message, sizeof(message), stdin);

        if (strncmp(message, "exit", 4) == 0) {
            break;
        }

        message[strcspn(message, "\n")] = '\0'; // Remove newline character
        send_message(fromname, toname, message);
    }

    return 0;
}