#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "manage.h"
#include "sandbox.h"
#include "utils.h"

#define SANDBOX_ROOT "sandboxes"
#define CGROUP_ROOT "/sys/fs/cgroup"

static int is_valid_sandbox_name(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    /*
     * 禁止路径穿越：
     * 只允许字母、数字、下划线、横杠。
     */
    for (const char *p = name; *p != '\0'; p++) {
        char c = *p;

        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_' ||
            c == '-') {
            continue;
        }

        return 0;
    }

    return 1;
}

static int build_sandbox_path(char *buf, size_t size, const char *name) {
    int n;

    if (!is_valid_sandbox_name(name)) {
        fprintf(stderr, "Error: invalid sandbox name '%s'\n", name ? name : "(null)");
        fprintf(stderr, "Allowed characters: letters, digits, underscore, hyphen\n");
        return -1;
    }

    n = snprintf(buf, size, "%s/%s", SANDBOX_ROOT, name);
    if (n < 0 || (size_t)n >= size) {
        fprintf(stderr, "Error: sandbox path too long\n");
        return -1;
    }

    return 0;
}

static int build_child_path(char *buf, size_t size, const char *base, const char *child) {
    int n = snprintf(buf, size, "%s/%s", base, child);

    if (n < 0 || (size_t)n >= size) {
        fprintf(stderr, "Error: path too long: %s/%s\n", base, child);
        return -1;
    }

    return 0;
}

static int is_directory(const char *path) {
    struct stat st;

    if (stat(path, &st) < 0) {
        return 0;
    }

    return S_ISDIR(st.st_mode);
}

static unsigned long long count_files_recursive(const char *path) {
    DIR *dir = opendir(path);
    struct dirent *entry;
    unsigned long long count = 0;

    if (dir == NULL) {
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        char child[LSANDBOX_PATH_MAX];
        struct stat st;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (build_child_path(child, sizeof(child), path, entry->d_name) < 0) {
            continue;
        }

        if (lstat(child, &st) < 0) {
            continue;
        }

        count++;

        if (S_ISDIR(st.st_mode)) {
            count += count_files_recursive(child);
        }
    }

    closedir(dir);
    return count;
}

int lsandbox_manage_list(void) {
    DIR *dir;
    struct dirent *entry;
    int found = 0;

    if (!lsandbox_path_exists(SANDBOX_ROOT)) {
        printf("No sandboxes found. Directory '%s' does not exist.\n", SANDBOX_ROOT);
        return 0;
    }

    dir = opendir(SANDBOX_ROOT);
    if (dir == NULL) {
        fprintf(stderr, "Error: opendir '%s' failed: %s\n",
                SANDBOX_ROOT, strerror(errno));
        return 1;
    }

    printf("Sandboxes:\n");

    while ((entry = readdir(dir)) != NULL) {
        char path[LSANDBOX_PATH_MAX];

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (build_child_path(path, sizeof(path), SANDBOX_ROOT, entry->d_name) < 0) {
            continue;
        }

        if (is_directory(path)) {
            printf("  %s\n", entry->d_name);
            found = 1;
        }
    }

    closedir(dir);

    if (!found) {
        printf("  (none)\n");
    }

    return 0;
}

int lsandbox_manage_inspect(const char *name) {
    char sandbox_dir[LSANDBOX_PATH_MAX];
    char upper_tmp[LSANDBOX_PATH_MAX];
    char work_tmp[LSANDBOX_PATH_MAX];
    char merged_tmp[LSANDBOX_PATH_MAX];

    if (build_sandbox_path(sandbox_dir, sizeof(sandbox_dir), name) < 0) {
        return 1;
    }

    if (build_child_path(upper_tmp, sizeof(upper_tmp), sandbox_dir, "upper_tmp") < 0) {
        return 1;
    }

    if (build_child_path(work_tmp, sizeof(work_tmp), sandbox_dir, "work_tmp") < 0) {
        return 1;
    }

    if (build_child_path(merged_tmp, sizeof(merged_tmp), sandbox_dir, "merged_tmp") < 0) {
        return 1;
    }

    printf("Sandbox: %s\n", name);
    printf("Directory: %s\n", sandbox_dir);
    printf("Exists: %s\n", is_directory(sandbox_dir) ? "yes" : "no");
    printf("\n");

    printf("Overlay directories:\n");
    printf("  upper_tmp : %s [%s]\n", upper_tmp, is_directory(upper_tmp) ? "exists" : "missing");
    printf("  work_tmp  : %s [%s]\n", work_tmp, is_directory(work_tmp) ? "exists" : "missing");
    printf("  merged_tmp: %s [%s]\n", merged_tmp, is_directory(merged_tmp) ? "exists" : "missing");
    printf("\n");

    if (is_directory(upper_tmp)) {
        printf("upper_tmp entries: %llu\n", count_files_recursive(upper_tmp));
    }

    printf("cgroup path: /sys/fs/cgroup/lsandbox_%s\n", name);

    return 0;
}

int lsandbox_manage_clean(const char *name) {
    char sandbox_dir[LSANDBOX_PATH_MAX];
    char upper_tmp[LSANDBOX_PATH_MAX];
    char work_tmp[LSANDBOX_PATH_MAX];
    char merged_tmp[LSANDBOX_PATH_MAX];

    if (build_sandbox_path(sandbox_dir, sizeof(sandbox_dir), name) < 0) {
        return 1;
    }

    if (!is_directory(sandbox_dir)) {
        fprintf(stderr, "Error: sandbox '%s' does not exist\n", name);
        return 1;
    }

    if (build_child_path(upper_tmp, sizeof(upper_tmp), sandbox_dir, "upper_tmp") < 0) {
        return 1;
    }

    if (build_child_path(work_tmp, sizeof(work_tmp), sandbox_dir, "work_tmp") < 0) {
        return 1;
    }

    if (build_child_path(merged_tmp, sizeof(merged_tmp), sandbox_dir, "merged_tmp") < 0) {
        return 1;
    }

    /*
     * clean：保留 sandboxes/<name>，只清空 overlay 工作目录。
     */
    lsandbox_remove_recursive(upper_tmp);
    lsandbox_remove_recursive(work_tmp);
    lsandbox_remove_recursive(merged_tmp);

    if (lsandbox_mkdir_p(upper_tmp, 0755) < 0) {
        return 1;
    }

    if (lsandbox_mkdir_p(work_tmp, 0755) < 0) {
        return 1;
    }

    if (lsandbox_mkdir_p(merged_tmp, 0755) < 0) {
        return 1;
    }

    printf("Cleaned sandbox '%s'\n", name);
    return 0;
}

int lsandbox_manage_delete(const char *name) {
    char sandbox_dir[LSANDBOX_PATH_MAX];

    if (build_sandbox_path(sandbox_dir, sizeof(sandbox_dir), name) < 0) {
        return 1;
    }

    if (!is_directory(sandbox_dir)) {
        fprintf(stderr, "Error: sandbox '%s' does not exist\n", name);
        return 1;
    }

    if (lsandbox_remove_recursive(sandbox_dir) < 0) {
        return 1;
    }

    printf("Deleted sandbox '%s'\n", name);
    return 0;
}

int lsandbox_manage_clean_cgroups(void) {
    DIR *dir;
    struct dirent *entry;
    int removed = 0;

    dir = opendir(CGROUP_ROOT);
    if (dir == NULL) {
        fprintf(stderr, "Error: opendir '%s' failed: %s\n",
                CGROUP_ROOT, strerror(errno));
        return 1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[LSANDBOX_PATH_MAX];

        if (strncmp(entry->d_name, "lsandbox_", strlen("lsandbox_")) != 0) {
            continue;
        }

        if (build_child_path(path, sizeof(path), CGROUP_ROOT, entry->d_name) < 0) {
            continue;
        }

        if (rmdir(path) == 0) {
            printf("Removed cgroup: %s\n", path);
            removed++;
        } else {
            fprintf(stderr, "Warning: failed to remove cgroup '%s': %s\n",
                    path, strerror(errno));
        }
    }

    closedir(dir);

    if (removed == 0) {
        printf("No removable lsandbox cgroups found.\n");
    }

    return 0;
}