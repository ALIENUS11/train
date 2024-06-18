#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PORT 33333

struct message {
    int action;          // 消息类型：1 表示注册，2 表示发送消息，3 表示群发消息，4 表示文件下载请求
    char fromname[20];   // 发送消息的用户名
    char toname[20];     // 接收消息的用户名
    char msg[1024];      // 消息内容
};

struct online {
    int cfd;             // 客户端套接字文件描述符
    char name[20];       // 用户名
    struct online *next; // 指向下一个在线用户的指针
};

struct online *head = NULL;  // 在线用户链表头指针

// 插入用户到在线用户链表中
void insert_user(struct online *new) {
    if (head == NULL) {
        new->next = NULL;
        head = new;
    } else {
        new->next = head->next;
        head->next = new;
    }
}

// 查找用户名对应的客户端套接字文件描述符
int find_cfd(char *toname) {
    if (head == NULL) {
        return -1;
    }

    struct online *temp = head;

    while (temp != NULL) {
        if (strcmp(temp->name, toname) == 0) {
            return temp->cfd;
        }
        temp = temp->next;
    }
    return -1;
}

// 接收消息的线程函数
void *recv_message(void *arg) {
    int ret;
    int to_cfd;
    int cfd = *((int *)arg);

    struct online *new;
    struct message *msg = (struct message *)malloc(sizeof(struct message));

    while (1) {
        memset(msg, 0, sizeof(struct message));

        // 接收客户端发送的消息
        if ((ret = recv(cfd, msg, sizeof(struct message), 0)) < 0) {
            perror("recv error!");
            exit(1);
        }

        if (ret == 0) {
            printf("%d is close!\n", cfd);
            pthread_exit(NULL);
        }

        // 根据消息类型执行不同的操作
        switch (msg->action) {
            case 1: {  // 注册
                new = (struct online *)malloc(sizeof(struct online));
                new->cfd = cfd;
                strcpy(new->name, msg->fromname);
                insert_user(new);
                msg->action = 1;
                send(cfd, msg, sizeof(struct message), 0);  // 发送注册成功消息
                break;
            }
            case 2: {  // 发送消息
                to_cfd = find_cfd(msg->toname);
                msg->action = 2;
                send(to_cfd, msg, sizeof(struct message), 0);  // 转发消息到目标客户端

                // 记录消息到文件
                time_t timep;
                time(&timep);
                char buff[100];
                strcpy(buff, ctime(&timep));
                buff[strlen(buff) - 1] = 0;

                char record[1024];
                sprintf(record, "%s(%s->%s):%s", buff, msg->fromname, msg->toname, msg->msg);
                printf("one record is:%s \n", record);

                FILE *fp;
                fp = fopen("a.txt", "a+");
                if (fp == NULL) {
                    printf("file open error!");
                } else {
                    fprintf(fp, "%s\n", record);
                    printf("record have written into file. \n");
                }
                fclose(fp);
                break;
            }
            case 3: {  // 群发消息
                struct online *temp = head;

                while (temp != NULL) {
                    to_cfd = temp->cfd;
                    msg->action = 3;
                    send(to_cfd, msg, sizeof(struct message), 0);  // 发送群发消息到每个客户端
                    temp = temp->next;
                }
                break;
            }
            case 4: {  // 文件下载请求
                char filename[100];
                strcpy(filename, msg->msg);

                FILE *fp = fopen(filename, "rb");
                if (fp == NULL) {
                    perror("File open error");
                    msg->action = -1;  // 表示文件不存在
                    send(cfd, msg, sizeof(struct message), 0);
                } else {
                    msg->action = 4;  // 表示文件存在，开始发送
                    send(cfd, msg, sizeof(struct message), 0);

                    // 逐段读取文件内容并发送
                    ssize_t bytesRead = 0;
                    while ((bytesRead = fread(msg->msg, 1, sizeof(msg->msg), fp)) > 0) {
                        send(cfd, msg, bytesRead, 0);
                    }

                    fclose(fp);
                }
                break;
            }
        }

        usleep(3);
    }

    pthread_exit(NULL);
}

// 主函数
int main() {
    int cfd;
    int sockfd;
    int c_len;

    char buffer[1024];

    pthread_t id;

    struct sockaddr_in s_addr;
    struct sockaddr_in c_addr;

    // 创建一个 TCP 套接字
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket error!");
        exit(1);
    }

    printf("socket success!\n");

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));  // 设置套接字选项，允许地址重用

    bzero(&s_addr, sizeof(struct sockaddr_in));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(PORT);
    s_addr.sin_addr.s_addr = INADDR_ANY;

    // 绑定套接字到地址和端口
    if (bind(sockfd, (struct sockaddr *)(&s_addr), sizeof(struct sockaddr_in)) < 0) {
        perror("bind error!");
        exit(1);
    }

    printf("bind success!\n");

    // 监听端口
    if (listen(sockfd, 3) < 0) {
        perror("listen error!");
        exit(1);
    }

    printf("listen success!\n");

    while (1) {
        memset(buffer, 0, sizeof(buffer));

        bzero(&c_addr, sizeof(struct sockaddr_in));
        c_len = sizeof(struct sockaddr_in);

        printf("accepting........!\n");

        // 接受客户端连接
        if ((cfd = accept(sockfd, (struct sockaddr *)(&c_addr), &c_len)) < 0) {
            perror("accept error!");
            exit(1);
        }

        printf("port = %d ip = %s\n", ntohs(c_addr.sin_port), inet_ntoa(c_addr.sin_addr));

        // 创建接收消息的线程
        if (pthread_create(&id, NULL, recv_message, (void *)(&cfd)) != 0) {
            perror("pthread create error!");
            exit(1);
        }

        usleep(3);
    }

    return 0;
}
