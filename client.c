#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define PORT 33333
#define VERSION_URL "http://test.sh.fangk.top/server_version.txt"
#define UPDATE_URL "http://test.sh.fangk.top/client_new"
#define LOCAL_VERSION "version.txt"
#define CLIENT_PATH "./client"
#define TEMP_CLIENT_PATH "./client_new_temp"
#define UPDATE_FLAG "update_complete.flag"

// 写入回调函数，用于下载文件
size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

// 检查并更新客户端
void check_and_update() {
    CURL *curl;
    FILE *fp;
    CURLcode res;
    char server_version[64];
    char local_version[64];

    // 初始化 libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        // 下载服务器版本号文件并保存为本地的 version.txt
        fp = fopen(LOCAL_VERSION, "w");
        curl_easy_setopt(curl, CURLOPT_URL, VERSION_URL);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        res = curl_easy_perform(curl);
        fclose(fp);

        if (res == CURLE_OK) {
            // 读取本地版本号
            fp = fopen(LOCAL_VERSION, "r");
            if (fp != NULL) {
                fscanf(fp, "%s", local_version);
                fclose(fp);

                // 比较版本号
                if (access("server_version.txt", F_OK) != -1) {
                    // 删除旧的 server_version.txt 文件
                    remove("server_version.txt");
                }

                // 重命名下载的版本号文件为 server_version.txt
                if (rename(LOCAL_VERSION, "server_version.txt") != 0) {
                    perror("Failed to rename version file");
                    exit(EXIT_FAILURE);
                }

                // 读取服务器版本号
                fp = fopen("server_version.txt", "r");
                fscanf(fp, "%s", server_version);
                fclose(fp);

                // 比较本地和服务器版本号
                if (strcmp(server_version, local_version) != 0) {
                    // 版本不同，下载新版本
                    printf("Downloading new version...\n");
                    fp = fopen(TEMP_CLIENT_PATH, "wb");
                    curl_easy_setopt(curl, CURLOPT_URL, UPDATE_URL);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                    res = curl_easy_perform(curl);
                    fclose(fp);

                    if (res == CURLE_OK) {
                        printf("Update downloaded.\n");

                        // 验证下载文件的大小
                        struct stat st;
                        if (stat(TEMP_CLIENT_PATH, &st) == 0 && st.st_size > 0) {
                            // 创建标志文件
                            int fd = open(UPDATE_FLAG, O_CREAT | O_WRONLY, 0644);
                            if (fd != -1) {
                                close(fd);
                            }

                            // 替换旧版本并重启
                            if (rename(TEMP_CLIENT_PATH, CLIENT_PATH) == 0) {
                                printf("Update applied successfully.\n");
                                execl(CLIENT_PATH, CLIENT_PATH, NULL);
                            } else {
                                perror("Failed to apply update");
                            }
                        } else {
                            printf("Downloaded file is invalid.\n");
                        }
                    } else {
                        printf("Failed to download update.\n");
                    }

                    // 退出当前客户端
                    exit(EXIT_FAILURE);
                } else {
                    printf("Client is up to date.\n");
                }
            } else {
                // 如果本地版本文件不存在，创建并写入服务器版本号
                printf("Local version not found, creating new one.\n");
                fp = fopen(LOCAL_VERSION, "w");
                fprintf(fp, "%s", server_version);
                fclose(fp);
            }
        } else {
            printf("Failed to fetch server version.\n");
        }

        curl_easy_cleanup(curl);
    } else {
        printf("Failed to initialize libcurl.\n");
    }

    curl_global_cleanup();
}

struct message {
    int action;
    char fromname[20];
    char toname[20];
    char msg[1024];
};

void *recv_message(void *arg) {
    time_t t;
    char buf[1024];
    time(&t);
    ctime_r(&t, buf);

    int ret;
    int cfd = *((int *)arg);

    struct message *msg = (struct message *)malloc(sizeof(struct message));

    while (1) {
        memset(msg, 0, sizeof(struct message));

        if ((ret = recv(cfd, msg, sizeof(struct message), 0)) < 0) {
            perror("recv error!");
            exit(EXIT_FAILURE);
        }

        if (ret == 0) {
            printf("%d is close!\n", cfd);
            pthread_exit(NULL);
        }

        switch (msg->action) {
            case 1:
                printf("reg success!\n");
                break;
            case 2:
                printf("time:%s recv:%s\n", buf, msg->msg);
                break;
            case 3:
                printf("time:%s all recv:%s\n", buf, msg->msg);
                break;
        }
        usleep(3);
    }

    pthread_exit(NULL);
}

// 信号处理函数，用于捕获 Ctrl+C 中断信号
volatile sig_atomic_t update_in_progress = 0;

void sig_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        update_in_progress = 1;
    }
}

int main() {
    // 注册信号处理函数
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    // 检查并更新
    printf("Checking for updates...\n");
    check_and_update();

    int sockfd;
    pthread_t id;
    struct sockaddr_in s_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket error!");
        exit(EXIT_FAILURE);
    }

    printf("client socket success!\n");

    bzero(&s_addr, sizeof(struct sockaddr_in));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(PORT);
    s_addr.sin_addr.s_addr = inet_addr("101.133.168.114");

    if (connect(sockfd, (struct sockaddr *)(&s_addr), sizeof(struct sockaddr_in)) < 0) {
        perror("connect error!");
        exit(EXIT_FAILURE);
    }

    printf("connect success!\n");

    if (pthread_create(&id, NULL, recv_message, (void *)(&sockfd)) != 0) {
        perror("pthread create error!");
        exit(EXIT_FAILURE);
    }

    char cmd[20];
    char name[20];
    char toname[20];
    char message[1024];

    struct message *msg = (struct message *)malloc(sizeof(struct message));

    while (1) {
        printf("Please input cmd:\n");
        scanf("%s", cmd);

        if (strcmp(cmd, "reg") == 0) {
            printf("Please input reg name:\n");
            scanf("%s", name);

            msg->action = 1;
            strcpy(msg->fromname, name);

            if (send(sockfd, msg, sizeof(struct message), 0) < 0) {
                perror("send error reg!\n");
                exit(EXIT_FAILURE);
            }
        }

        if (strcmp(cmd, "send") == 0) {
            printf("Please input send to name:\n");
            scanf("%s", toname);

            printf("Please input send message:\n");
            scanf("%s", message);

            msg->action = 2;
            strcpy(msg->toname, toname);
            strcpy(msg->msg, message);

            if (send(sockfd, msg, sizeof(struct message), 0) < 0) {
                perror("send error send!\n");
                exit(EXIT_FAILURE);
            }
        }

        if (strcmp(cmd, "all") == 0) {
            printf("Please input all message:\n");
            scanf("%s", message);

            msg->action = 3;
            strcpy(msg->msg, message);

            if (send(sockfd, msg, sizeof(struct message), 0) < 0) {
                perror("send error all!\n");
                exit(EXIT_FAILURE);
            }
        }

        // 如果接收到中断信号，则退出循环
        if (update_in_progress) {
            printf("Interrupt signal received. Exiting...\n");
            break;
        }
    }

    // 关闭套接字
    shutdown(sockfd, SHUT_RDWR);

    return 0;
}
