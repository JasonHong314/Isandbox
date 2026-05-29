#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

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

int lsandbox_mkdir_p(const char *path, mode_t mode) {
    char tmp[1024];
    size_t len;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);

    if (len == 0) {
        return -1;
    }

    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (char *p = tmp + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';

            if (lsandbox_mkdir_if_not_exists(tmp, mode) < 0) {
                return -1;
            }

            *p = '/';
        }
    }

    if (lsandbox_mkdir_if_not_exists(tmp, mode) < 0) {
        return -1;
    }

    return 0;
}

static int remove_recursive_inner(const char *path) {
    struct stat st;

    if (lstat(path, &st) < 0) {
        if (errno == ENOENT) {
            return 0;
        }

        fprintf(stderr, "Error: lstat '%s' failed: %s\n", path, strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        if (unlink(path) < 0) {
            fprintf(stderr, "Error: unlink '%s' failed: %s\n", path, strerror(errno));
            return -1;
        }

        return 0;
    }

    DIR *dir = opendir(path);
    if (dir == NULL) {
        fprintf(stderr, "Error: opendir '%s' failed: %s\n", path, strerror(errno));
        return -1;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        char child[4096];

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        int n = snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(child)) {
            fprintf(stderr, "Error: path too long while deleting '%s'\n", path);
            closedir(dir);
            return -1;
        }

        if (remove_recursive_inner(child) < 0) {
            closedir(dir);
            return -1;
        }
    }

    closedir(dir);

    if (rmdir(path) < 0) {
        fprintf(stderr, "Error: rmdir '%s' failed: %s\n", path, strerror(errno));
        return -1;
    }

    return 0;
}

int lsandbox_remove_recursive(const char *path) {
    /*
     * 安全保护：
     * 只允许删除 sandboxes/ 开头的相对路径。
     * 防止误删 /、/tmp、/home 等重要目录。
     */
    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    if (strncmp(path, "sandboxes/", strlen("sandboxes/")) != 0) {
        fprintf(stderr, "Error: refuse to remove unsafe path '%s'\n", path);
        return -1;
    }

    return remove_recursive_inner(path);
}
