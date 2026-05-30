#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cgroup.h"
#include "sandbox.h"
#include "utils.h"

#define CGROUP_ROOT "/sys/fs/cgroup"

static int write_text_file(const char *path, const char *text) {
    FILE *fp = fopen(path, "w");

    if (fp == NULL) {
        fprintf(stderr, "Error: open '%s' failed: %s\n", path, strerror(errno));
        return -1;
    }

    if (fputs(text, fp) == EOF) {
        fprintf(stderr, "Error: write '%s' failed: %s\n", path, strerror(errno));
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

static int read_text_file(const char *path, char *buf, size_t size) {
    FILE *fp = fopen(path, "r");

    if (fp == NULL) {
        return -1;
    }

    if (fgets(buf, size, fp) == NULL) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

static int path_join(char *buf, size_t size, const char *dir, const char *name) {
    int n = snprintf(buf, size, "%s/%s", dir, name);

    if (n < 0 || (size_t)n >= size) {
        fprintf(stderr, "Error: path too long: %s/%s\n", dir, name);
        return -1;
    }

    return 0;
}

static int parse_size_to_bytes(const char *text, unsigned long long *out) {
    char *end = NULL;
    unsigned long long value;

    if (text == NULL || text[0] == '\0' || out == NULL) {
        return -1;
    }

    errno = 0;
    value = strtoull(text, &end, 10);

    if (errno != 0 || end == text) {
        return -1;
    }

    if (*end == '\0') {
        *out = value;
        return 0;
    }

    if ((end[0] == 'K' || end[0] == 'k') && end[1] == '\0') {
        *out = value * 1024ULL;
        return 0;
    }

    if ((end[0] == 'M' || end[0] == 'm') && end[1] == '\0') {
        *out = value * 1024ULL * 1024ULL;
        return 0;
    }

    if ((end[0] == 'G' || end[0] == 'g') && end[1] == '\0') {
        *out = value * 1024ULL * 1024ULL * 1024ULL;
        return 0;
    }

    return -1;
}

static int ensure_cgroup_v2(void) {
    if (!lsandbox_path_exists(CGROUP_ROOT "/cgroup.controllers")) {
        fprintf(stderr, "Error: cgroups v2 not detected at %s\n", CGROUP_ROOT);
        return -1;
    }

    return 0;
}

static int ensure_root_controllers(void) {
    char buf[1024];

    if (read_text_file(CGROUP_ROOT "/cgroup.subtree_control",
                       buf,
                       sizeof(buf)) < 0) {
        fprintf(stderr, "Error: cannot read cgroup.subtree_control: %s\n",
                strerror(errno));
        return -1;
    }

    if (strstr(buf, "memory") == NULL ||
        strstr(buf, "pids") == NULL ||
        strstr(buf, "cpu") == NULL) {
        fprintf(stderr,
                "Error: root cgroup controllers not enabled. Current: %s\n",
                buf);
        fprintf(stderr,
                "Hint: run: sudo sh -c 'echo \"+memory +pids +cpu\" > /sys/fs/cgroup/cgroup.subtree_control'\n");
        return -1;
    }

    return 0;
}

static int safe_cgroup_path(const char *path) {
    const char *prefix = "/sys/fs/cgroup/lsandbox_";

    if (path == NULL) {
        return 0;
    }

    return strncmp(path, prefix, strlen(prefix)) == 0;
}

static int cgroup_has_processes(const sandbox_config_t *cfg) {
    char path[LSANDBOX_PATH_MAX];
    char buf[64];

    if (path_join(path, sizeof(path), cfg->cgroup_dir, "cgroup.procs") < 0) {
        return 1;
    }

    if (read_text_file(path, buf, sizeof(buf)) < 0) {
        return 0;
    }

    return 1;
}

static int write_memory_limit(const sandbox_config_t *cfg) {
    char path[LSANDBOX_PATH_MAX];
    char value[128];
    unsigned long long bytes;

    if (cfg->memory_limit[0] == '\0') {
        return 0;
    }

    if (parse_size_to_bytes(cfg->memory_limit, &bytes) < 0) {
        fprintf(stderr, "Error: invalid memory limit '%s'\n", cfg->memory_limit);
        return -1;
    }

    snprintf(value, sizeof(value), "%llu", bytes);

    if (path_join(path, sizeof(path), cfg->cgroup_dir, "memory.max") < 0) {
        return -1;
    }

    if (!lsandbox_path_exists(path)) {
        fprintf(stderr, "Error: '%s' does not exist\n", path);
        return -1;
    }

    return write_text_file(path, value);
}

static int write_memory_swap_limit(const sandbox_config_t *cfg) {
    char path[LSANDBOX_PATH_MAX];

    if (cfg->memory_limit[0] == '\0') {
        return 0;
    }

    if (path_join(path, sizeof(path), cfg->cgroup_dir, "memory.swap.max") < 0) {
        return -1;
    }

    if (!lsandbox_path_exists(path)) {
        fprintf(stderr, "Warning: '%s' does not exist, skip swap limit\n", path);
        return 0;
    }

    return write_text_file(path, "0");
}

static int write_memory_oom_group(const sandbox_config_t *cfg) {
    char path[LSANDBOX_PATH_MAX];

    if (cfg->memory_limit[0] == '\0') {
        return 0;
    }

    if (path_join(path, sizeof(path), cfg->cgroup_dir, "memory.oom.group") < 0) {
        return -1;
    }

    if (!lsandbox_path_exists(path)) {
        fprintf(stderr, "Warning: '%s' does not exist, skip oom group\n", path);
        return 0;
    }

    return write_text_file(path, "1");
}

static int write_pids_limit(const sandbox_config_t *cfg) {
    char path[LSANDBOX_PATH_MAX];
    char value[64];

    if (cfg->pids_limit <= 0) {
        return 0;
    }

    snprintf(value, sizeof(value), "%d", cfg->pids_limit);

    if (path_join(path, sizeof(path), cfg->cgroup_dir, "pids.max") < 0) {
        return -1;
    }

    if (!lsandbox_path_exists(path)) {
        fprintf(stderr, "Error: '%s' does not exist\n", path);
        return -1;
    }

    return write_text_file(path, value);
}

static int write_cpu_limit(const sandbox_config_t *cfg) {
    char path[LSANDBOX_PATH_MAX];
    char value[128];

    if (cfg->cpu_percent <= 0) {
        return 0;
    }

    if (cfg->cpu_percent >= 100) {
        snprintf(value, sizeof(value), "max 100000");
    } else {
        int period = 100000;
        int quota = period * cfg->cpu_percent / 100;
        snprintf(value, sizeof(value), "%d %d", quota, period);
    }

    if (path_join(path, sizeof(path), cfg->cgroup_dir, "cpu.max") < 0) {
        return -1;
    }

    if (!lsandbox_path_exists(path)) {
        fprintf(stderr, "Error: '%s' does not exist\n", path);
        return -1;
    }

    return write_text_file(path, value);
}

static int add_pid_to_cgroup(const sandbox_config_t *cfg, pid_t pid) {
    char path[LSANDBOX_PATH_MAX];
    char value[64];

    snprintf(value, sizeof(value), "%d", pid);

    if (path_join(path, sizeof(path), cfg->cgroup_dir, "cgroup.procs") < 0) {
        return -1;
    }

    return write_text_file(path, value);
}

int lsandbox_cgroup_apply(const sandbox_config_t *cfg, pid_t pid) {
    if (cfg == NULL) {
        return -1;
    }

    if (!safe_cgroup_path(cfg->cgroup_dir)) {
        fprintf(stderr, "Error: unsafe cgroup path '%s'\n", cfg->cgroup_dir);
        return -1;
    }

    if (ensure_cgroup_v2() < 0) {
        return -1;
    }

    if (ensure_root_controllers() < 0) {
        return -1;
    }

    if (lsandbox_path_exists(cfg->cgroup_dir)) {
        if (rmdir(cfg->cgroup_dir) < 0) {
            fprintf(stderr,
                    "Warning: old cgroup '%s' exists and cannot be removed: %s\n",
                    cfg->cgroup_dir,
                    strerror(errno));
        }
    }

    if (lsandbox_mkdir_p(cfg->cgroup_dir, 0755) < 0) {
        return -1;
    }

    if (write_memory_limit(cfg) < 0) {
        lsandbox_cgroup_cleanup(cfg);
        return -1;
    }

    if (write_memory_swap_limit(cfg) < 0) {
        lsandbox_cgroup_cleanup(cfg);
        return -1;
    }

    if (write_memory_oom_group(cfg) < 0) {
        lsandbox_cgroup_cleanup(cfg);
        return -1;
    }

    if (write_pids_limit(cfg) < 0) {
        lsandbox_cgroup_cleanup(cfg);
        return -1;
    }

    if (write_cpu_limit(cfg) < 0) {
        lsandbox_cgroup_cleanup(cfg);
        return -1;
    }

    if (add_pid_to_cgroup(cfg, pid) < 0) {
        lsandbox_cgroup_cleanup(cfg);
        return -1;
    }

    return 0;
}

int lsandbox_cgroup_cleanup(const sandbox_config_t *cfg) {
    if (cfg == NULL) {
        return -1;
    }

    if (!safe_cgroup_path(cfg->cgroup_dir)) {
        fprintf(stderr, "Error: refuse to cleanup unsafe cgroup path '%s'\n",
                cfg->cgroup_dir);
        return -1;
    }

    if (!lsandbox_path_exists(cfg->cgroup_dir)) {
        return 0;
    }

    if (cfg->keep_cgroup) {
        return 0;
    }

    if (cgroup_has_processes(cfg)) {
        fprintf(stderr,
                "Warning: cgroup '%s' still has processes, skip cleanup\n",
                cfg->cgroup_dir);
        return -1;
    }

    if (rmdir(cfg->cgroup_dir) < 0) {
        fprintf(stderr,
                "Warning: failed to remove cgroup '%s': %s\n",
                cfg->cgroup_dir,
                strerror(errno));
        return -1;
    }

    return 0;
}