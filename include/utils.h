#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
 * 后续阶段会在这里放通用工具函数：
 *
 * 1. mkdir_p()
 * 2. write_file()
 * 3. path_exists()
 * 4. remove_recursive()
 * 5. join_path()
 *
 * 第一阶段暂时不需要具体工具函数。
 */

int lsandbox_path_exists(const char *path) {
    struct stat st;

    if (path == NULL) {
        return 0;
    }

    return stat(path, &st) == 0;
}

int lsandbox_mkdir_if_not_exists(const char *path, mode_t mode) {
    if (path == NULL) {
        return -1;
    }

    if (mkdir(path, mode) == 0) {
        return 0;
    }

    if (errno == EEXIST) {
        return 0;
    }

    fprintf(stderr, "Error: mkdir '%s' failed: %s\n", path, strerror(errno));
    return -1;
}