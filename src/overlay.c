#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>

#include "overlay.h"
#include "sandbox.h"
#include "utils.h"

#define LSANDBOX_OVERLAY_OPT_MAX 8192

int lsandbox_prepare_overlay_dirs(sandbox_config_t *cfg) {
    if (cfg == NULL) {
        return -1;
    }

    if (lsandbox_mkdir_p("sandboxes", 0755) < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(cfg->sandbox_dir, 0755) < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(cfg->upper_tmp_dir, 0755) < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(cfg->work_tmp_dir, 0755) < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(cfg->merged_tmp_dir, 0755) < 0) {
        return -1;
    }

    return 0;
}

static int build_overlay_options(char *buf,
                                 size_t size,
                                 const sandbox_config_t *cfg) {
    int n;

    if (buf == NULL || cfg == NULL) {
        return -1;
    }

    n = snprintf(buf,
                 size,
                 "lowerdir=/tmp,upperdir=%s,workdir=%s",
                 cfg->upper_tmp_dir,
                 cfg->work_tmp_dir);

    if (n < 0) {
        fprintf(stderr, "Error: snprintf overlay options failed\n");
        return -1;
    }

    if ((size_t)n >= size) {
        fprintf(stderr, "Error: overlay mount options too long\n");
        return -1;
    }

    return 0;
}

int lsandbox_mount_tmp_overlay(const sandbox_config_t *cfg) {
    if (cfg == NULL) {
        return -1;
    }

    char options[LSANDBOX_OVERLAY_OPT_MAX];

    if (build_overlay_options(options, sizeof(options), cfg) < 0) {
        return -1;
    }

    if (mount("overlay",
              cfg->merged_tmp_dir,
              "overlay",
              0,
              options) < 0) {
        fprintf(stderr, "Error: mount overlay for /tmp failed: %s\n", strerror(errno));
        fprintf(stderr, "Overlay options: %s\n", options);
        return -1;
    }

    /*
     * 这里把 overlay 的 merged_tmp 绑定到沙盒内 /tmp。
     * 由于当前处于新的 mount namespace 中，这不会影响主机。
     */
    if (mount(cfg->merged_tmp_dir,
              "/tmp",
              NULL,
              MS_BIND | MS_REC,
              NULL) < 0) {
        fprintf(stderr, "Error: bind merged_tmp to /tmp failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int lsandbox_cleanup_overlay_dirs(const sandbox_config_t *cfg) {
    if (cfg == NULL) {
        return -1;
    }

    /*
     * 挂载发生在子进程的 mount namespace 中。
     * 子进程退出后，该 namespace 销毁，挂载通常已经随之消失。
     *
     * 父进程这里主要负责删除持久化目录。
     */
    if (cfg->sandbox_dir[0] == '\0') {
        return -1;
    }

    return lsandbox_remove_recursive(cfg->sandbox_dir);
}
