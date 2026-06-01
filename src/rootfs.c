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

static int create_char_device(const char *dev_dir,
                              const char *name,
                              int major_no,
                              int minor_no,
                              mode_t mode) {
    char path[LSANDBOX_PATH_MAX];

    if (path_join(path, sizeof(path), dev_dir, name) < 0) {
        return -1;
    }

    unlink(path);

    if (mknod(path, S_IFCHR | mode, makedev(major_no, minor_no)) < 0) {
        fprintf(stderr, "Error: mknod %s failed: %s\n",
                path, strerror(errno));
        return -1;
    }

    if (chmod(path, mode) < 0) {
        fprintf(stderr, "Error: chmod %s failed: %s\n",
                path, strerror(errno));
        return -1;
    }

    return 0;
}

static int create_symlink_force(const char *target, const char *link_path) {
    unlink(link_path);

    if (symlink(target, link_path) < 0) {
        fprintf(stderr, "Warning: symlink %s -> %s failed: %s\n",
                link_path, target, strerror(errno));
        return -1;
    }

    return 0;
}

static int setup_private_dev(const char *root) {
    char dev_dir[LSANDBOX_PATH_MAX];
    char path[LSANDBOX_PATH_MAX];

    if (path_join(dev_dir, sizeof(dev_dir), root, "dev") < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(dev_dir, 0755) < 0) {
        return -1;
    }

    if (mount("tmpfs", dev_dir, "tmpfs",
              MS_NOSUID | MS_NOEXEC,
              "mode=1777") < 0) {
        fprintf(stderr, "Error: mount private /dev tmpfs failed: %s\n",
                strerror(errno));
        return -1;
    }

    if (chmod(dev_dir, 01777) < 0) {
        fprintf(stderr, "Error: chmod private /dev failed: %s\n",
                strerror(errno));
        return -1;
    }

    if (create_char_device(dev_dir, "null", 1, 3, 0666) < 0) {
        return -1;
    }

    if (create_char_device(dev_dir, "zero", 1, 5, 0666) < 0) {
        return -1;
    }

    if (create_char_device(dev_dir, "full", 1, 7, 0666) < 0) {
        return -1;
    }

    if (create_char_device(dev_dir, "random", 1, 8, 0666) < 0) {
        return -1;
    }

    if (create_char_device(dev_dir, "urandom", 1, 9, 0666) < 0) {
        return -1;
    }

    if (create_char_device(dev_dir, "tty", 5, 0, 0666) < 0) {
        return -1;
    }

    if (path_join(path, sizeof(path), dev_dir, "fd") == 0) {
        create_symlink_force("/proc/self/fd", path);
    }

    if (path_join(path, sizeof(path), dev_dir, "stdin") == 0) {
        create_symlink_force("/proc/self/fd/0", path);
    }

    if (path_join(path, sizeof(path), dev_dir, "stdout") == 0) {
        create_symlink_force("/proc/self/fd/1", path);
    }

    if (path_join(path, sizeof(path), dev_dir, "stderr") == 0) {
        create_symlink_force("/proc/self/fd/2", path);
    }

    if (path_join(path, sizeof(path), dev_dir, "shm") < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(path, 01777) < 0) {
        return -1;
    }

    if (mount("tmpfs", path, "tmpfs",
              MS_NOSUID | MS_NODEV,
              "mode=1777") < 0) {
        fprintf(stderr, "Warning: mount /dev/shm failed: %s\n",
                strerror(errno));
    } else {
        chmod(path, 01777);
    }

    if (path_join(path, sizeof(path), dev_dir, "pts") < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(path, 0755) < 0) {
        return -1;
    }

    if (mount("devpts", path, "devpts",
              MS_NOSUID | MS_NOEXEC,
              "newinstance,ptmxmode=0666,mode=0620") < 0) {
        fprintf(stderr, "Warning: mount /dev/pts failed: %s\n",
                strerror(errno));
    }

    if (path_join(path, sizeof(path), dev_dir, "ptmx") == 0) {
        create_symlink_force("pts/ptmx", path);
    }

    return 0;
}

static int copy_file_content(const char *src, const char *dst) {
    FILE *in = fopen(src, "r");
    FILE *out;
    char buf[4096];
    size_t n;

    if (in == NULL) {
        return -1;
    }

    unlink(dst);

    out = fopen(dst, "w");
    if (out == NULL) {
        fclose(in);
        return -1;
    }

    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

static int setup_resolv_conf(const sandbox_config_t *cfg) {
    char etc_dir[LSANDBOX_PATH_MAX];
    char resolv_path[LSANDBOX_PATH_MAX];
    int n;

    if (cfg == NULL) {
        return -1;
    }

    n = snprintf(etc_dir, sizeof(etc_dir), "%s/etc", cfg->merged_root_dir);
    if (n < 0 || (size_t)n >= sizeof(etc_dir)) {
        fprintf(stderr, "Error: /etc path too long\n");
        return -1;
    }

    if (lsandbox_mkdir_p(etc_dir, 0755) < 0) {
        return -1;
    }

    n = snprintf(resolv_path,
                 sizeof(resolv_path),
                 "%s/etc/resolv.conf",
                 cfg->merged_root_dir);
    if (n < 0 || (size_t)n >= sizeof(resolv_path)) {
        fprintf(stderr, "Error: resolv.conf path too long\n");
        return -1;
    }

    if (copy_file_content("/etc/resolv.conf", resolv_path) == 0) {
        chmod(resolv_path, 0644);
        return 0;
    }

    unlink(resolv_path);

    FILE *fp = fopen(resolv_path, "w");
    if (fp == NULL) {
        fprintf(stderr, "Error: create sandbox resolv.conf failed: %s\n",
                strerror(errno));
        return -1;
    }

    fprintf(fp, "nameserver 1.1.1.1\n");
    fprintf(fp, "nameserver 8.8.8.8\n");

    fclose(fp);
    chmod(resolv_path, 0644);

    fprintf(stderr,
            "Warning: host /etc/resolv.conf not available, using fallback DNS\n");

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

    if (setup_private_dev(cfg->merged_root_dir) < 0) {
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

    if (setup_resolv_conf(cfg) < 0) {
        fprintf(stderr, "Warning: setup resolv.conf failed\n");
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
