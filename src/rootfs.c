#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include "rootfs.h"
#include "sandbox.h"
#include "utils.h"

#define ROOTFS_OPT_MAX 8192

static int path_join(char *buf, size_t size, const char *a, const char *b) {
    int n = snprintf(buf, size, "%s/%s", a, b);
    if (n < 0 || (size_t)n >= size) {
        fprintf(stderr, "Error: path too long: %s/%s\n", a, b);
        return -1;
    }
    return 0;
}

static int ensure_dir_under_root(const char *root, const char *name, mode_t mode) {
    char path[LSANDBOX_PATH_MAX];

    if (path_join(path, sizeof(path), root, name) < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(path, mode) < 0) {
        return -1;
    }

    chmod(path, mode);
    return 0;
}

int lsandbox_prepare_rootfs_dirs(sandbox_config_t *cfg) {
    if (cfg == NULL) {
        return -1;
    }

    if (lsandbox_mkdir_p("sandboxes", 0755) < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(cfg->sandbox_dir, 0755) < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(cfg->upper_root_dir, 0755) < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(cfg->work_root_dir, 0755) < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(cfg->merged_root_dir, 0755) < 0) {
        return -1;
    }

    return 0;
}

static int mount_root_overlay(const sandbox_config_t *cfg) {
    char options[ROOTFS_OPT_MAX];
    int n;

    n = snprintf(options,
                 sizeof(options),
                 "lowerdir=/,upperdir=%s,workdir=%s",
                 cfg->upper_root_dir,
                 cfg->work_root_dir);

    if (n < 0 || (size_t)n >= sizeof(options)) {
        fprintf(stderr, "Error: root overlay options too long\n");
        return -1;
    }

    if (mount("overlay",
              cfg->merged_root_dir,
              "overlay",
              0,
              options) < 0) {
        fprintf(stderr, "Error: mount root overlay failed: %s\n", strerror(errno));
        fprintf(stderr, "Overlay options: %s\n", options);
        return -1;
    }

    return 0;
}

static int setup_minimal_dev(const char *root) {
    char dev_path[LSANDBOX_PATH_MAX];
    char node_path[LSANDBOX_PATH_MAX];

    if (path_join(dev_path, sizeof(dev_path), root, "dev") < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(dev_path, 0755) < 0) {
        return -1;
    }

    if (mount("tmpfs", dev_path, "tmpfs", MS_NOSUID, "mode=755") < 0) {
        fprintf(stderr, "Error: mount tmpfs /dev failed: %s\n", strerror(errno));
        return -1;
    }

    struct {
        const char *name;
        int major_no;
        int minor_no;
        mode_t mode;
    } nodes[] = {
        {"null",   1, 3, 0666},
        {"zero",   1, 5, 0666},
        {"random", 1, 8, 0666},
        {"urandom",1, 9, 0666},
        {"tty",    5, 0, 0666},
    };

    for (size_t i = 0; i < sizeof(nodes) / sizeof(nodes[0]); i++) {
        if (path_join(node_path, sizeof(node_path), dev_path, nodes[i].name) < 0) {
            return -1;
        }

        unlink(node_path);

        if (mknod(node_path,
                  S_IFCHR | nodes[i].mode,
                  makedev(nodes[i].major_no, nodes[i].minor_no)) < 0) {
            fprintf(stderr, "Warning: mknod %s failed: %s\n",
                    node_path, strerror(errno));
        }
    }

    path_join(node_path, sizeof(node_path), dev_path, "fd");
    symlink("/proc/self/fd", node_path);

    path_join(node_path, sizeof(node_path), dev_path, "stdin");
    symlink("/proc/self/fd/0", node_path);

    path_join(node_path, sizeof(node_path), dev_path, "stdout");
    symlink("/proc/self/fd/1", node_path);

    path_join(node_path, sizeof(node_path), dev_path, "stderr");
    symlink("/proc/self/fd/2", node_path);

    return 0;
}

static int mount_runtime_fs(const sandbox_config_t *cfg) {
    char path[LSANDBOX_PATH_MAX];

    if (ensure_dir_under_root(cfg->merged_root_dir, "proc", 0555) < 0) {
        return -1;
    }

    if (path_join(path, sizeof(path), cfg->merged_root_dir, "proc") < 0) {
        return -1;
    }

    if (mount("proc", path, "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) < 0) {
        fprintf(stderr, "Error: mount proc in rootfs failed: %s\n", strerror(errno));
        return -1;
    }

    if (setup_minimal_dev(cfg->merged_root_dir) < 0) {
        return -1;
    }

    if (ensure_dir_under_root(cfg->merged_root_dir, "run", 0755) < 0) {
        return -1;
    }

    if (path_join(path, sizeof(path), cfg->merged_root_dir, "run") < 0) {
        return -1;
    }

    if (mount("tmpfs", path, "tmpfs", MS_NOSUID | MS_NODEV, "mode=755") < 0) {
        fprintf(stderr, "Warning: mount tmpfs /run failed: %s\n", strerror(errno));
    }

    return 0;
}

int lsandbox_enter_rootfs(const sandbox_config_t *cfg) {
    if (cfg == NULL) {
        return -1;
    }

    if (mount_root_overlay(cfg) < 0) {
        return -1;
    }

    if (mount_runtime_fs(cfg) < 0) {
        return -1;
    }

    if (chroot(cfg->merged_root_dir) < 0) {
        fprintf(stderr, "Error: chroot failed: %s\n", strerror(errno));
        return -1;
    }

    if (chdir(cfg->target_work_dir) < 0) {
        if (chdir("/") < 0) {
            fprintf(stderr, "Error: chdir / failed: %s\n", strerror(errno));
            return -1;
        }
    }

    return 0;
}

int lsandbox_setup_sandbox_resolv_conf(void) {
    FILE *in = fopen("/etc/resolv.conf", "r");
    FILE *out;
    char lines[4096] = {0};
    char buf[512];
    int has_valid_nameserver = 0;

    if (in) {
        while (fgets(buf, sizeof(buf), in)) {
            if (strstr(buf, "nameserver 127.") ||
                strstr(buf, "nameserver ::1")) {
                continue;
            }

            if (strncmp(buf, "nameserver", 10) == 0) {
                has_valid_nameserver = 1;
            }

            strncat(lines, buf, sizeof(lines) - strlen(lines) - 1);
        }
        fclose(in);
    }

    out = fopen("/etc/resolv.conf", "w");
    if (!out) {
        perror("open sandbox /etc/resolv.conf");
        return -1;
    }

    if (has_valid_nameserver) {
        fputs(lines, out);
    } else {
        fprintf(out, "nameserver 223.5.5.5\n");
        fprintf(out, "nameserver 114.114.114.114\n");
        fprintf(out, "nameserver 8.8.8.8\n");
        fprintf(out, "options timeout:2 attempts:2\n");
    }

    fclose(out);
    return 0;
}