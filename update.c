#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

void replace_and_restart(const char *new_path, const char *old_path) {
    // 设置新文件的可执行权限
    if (chmod(new_path, S_IRWXU) != 0) {
        perror("Failed to set permissions for the new client");
        exit(EXIT_FAILURE);
    }

    // 移除旧版本文件
    if (remove(old_path) != 0) {
        perror("Failed to remove the old client");
        exit(EXIT_FAILURE);
    }

    // 将新版本文件重命名为旧文件路径
    if (rename(new_path, old_path) != 0) {
        perror("Failed to rename the new client to old client");
        exit(EXIT_FAILURE);
    }

    printf("Update completed successfully.\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <new_client_path> <old_client_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *new_path = argv[1];
    const char *old_path = argv[2];

    replace_and_restart(new_path, old_path);

    // 删除更新标志文件
    if (remove("update_complete.flag") != 0) {
        perror("Failed to remove update_complete.flag");
        exit(EXIT_FAILURE);
    }

    return 0;
}
