#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>

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

    /*
     * 新设计：
     * /tmp 不再使用 host /tmp 作为 lowerdir。
     * 直接使用 sandboxes/<name>/tmp 作为沙盒私有临时目录。
     */
    if (lsandbox_mkdir_p(cfg->sandbox_tmp_dir, 0777) < 0) {
        return -1;
    }

    /*
     * /tmp 语义应为 1777：
     * 所有人可写，但只能删除自己的文件。
     */
    if (chmod(cfg->sandbox_tmp_dir, 01777) < 0) {
        fprintf(stderr, "Error: chmod '%s' failed: %s\n",
                cfg->sandbox_tmp_dir, strerror(errno));
        return -1;
    }

    return 0;
}

int lsandbox_prepare_workdir_overlay_dirs(sandbox_config_t *cfg) {
    if (cfg == NULL) {
        return -1;
    }

    if (cfg->target_work_dir[0] == '\0') {
        fprintf(stderr, "Error: empty target workdir\n");
        return -1;
    }

    if (lsandbox_mkdir_p("/var/tmp/lsandbox", 0755) < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(cfg->work_state_dir, 0755) < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(cfg->upper_work_dir, 0755) < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(cfg->work_work_dir, 0755) < 0) {
        return -1;
    }

    if (lsandbox_mkdir_p(cfg->merged_work_dir, 0755) < 0) {
        return -1;
    }

    return 0;
}

static int build_workdir_overlay_options(char *buf,
                                         size_t size,
                                         const sandbox_config_t *cfg) {
    int n;

    if (buf == NULL || cfg == NULL) {
        return -1;
    }

    n = snprintf(buf,
                 size,
                 "lowerdir=%s,upperdir=%s,workdir=%s",
                 cfg->target_work_dir,
                 cfg->upper_work_dir,
                 cfg->work_work_dir);

    if (n < 0) {
        fprintf(stderr, "Error: snprintf workdir overlay options failed\n");
        return -1;
    }

    if ((size_t)n >= size) {
        fprintf(stderr, "Error: workdir overlay mount options too long\n");
        return -1;
    }

    return 0;
}

int lsandbox_mount_tmp_overlay(const sandbox_config_t *cfg) {
    if (cfg == NULL) {
        return -1;
    }

    /*
     * 新设计：
     * 在新的 mount namespace 内，把沙盒私有 tmp 目录 bind 到 /tmp。
     *
     * 沙盒内看到的是 /tmp；
     * 实际写入位置是 sandboxes/<name>/tmp。
     *
     * 因为当前已经执行了：
     *   mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL)
     * 所以这个 bind mount 不会传播到主机。
     */
    if (mount(cfg->sandbox_tmp_dir,
              "/tmp",
              NULL,
              MS_BIND | MS_REC,
              NULL) < 0) {
        fprintf(stderr, "Error: bind sandbox tmp to /tmp failed: %s\n",
                strerror(errno));
        return -1;
    }

    return 0;
}

int lsandbox_mount_workdir_overlay(const sandbox_config_t *cfg) {
    if (cfg == NULL) {
        return -1;
    }

    char options[LSANDBOX_OVERLAY_OPT_MAX];

    if (build_workdir_overlay_options(options, sizeof(options), cfg) < 0) {
        return -1;
    }

    if (mount("overlay",
              cfg->merged_work_dir,
              "overlay",
              0,
              options) < 0) {
        fprintf(stderr, "Error: mount overlay for workdir failed: %s\n", strerror(errno));
        fprintf(stderr, "Overlay options: %s\n", options);
        return -1;
    }

    if (mount(cfg->merged_work_dir,
              cfg->target_work_dir,
              NULL,
              MS_BIND | MS_REC,
              NULL) < 0) {
        fprintf(stderr, "Error: bind merged_work to target workdir failed: %s\n",
                strerror(errno));
        return -1;
    }

    return 0;
}

static int remove_var_tmp_state(const char *path) {
    const char *prefix = "/var/tmp/lsandbox/";

    if (path == NULL || strncmp(path, prefix, strlen(prefix)) != 0) {
        fprintf(stderr, "Error: refuse to remove unsafe work state path '%s'\n",
                path ? path : "(null)");
        return -1;
    }

    char cmd[LSANDBOX_PATH_MAX + 32];
    int n = snprintf(cmd, sizeof(cmd), "rm -rf -- '%s'", path);

    if (n < 0 || (size_t)n >= sizeof(cmd)) {
        fprintf(stderr, "Error: cleanup command too long\n");
        return -1;
    }

    return system(cmd);
}

int lsandbox_cleanup_overlay_dirs(const sandbox_config_t *cfg) {
    int rc = 0;

    if (cfg == NULL) {
        return -1;
    }

    if (cfg->sandbox_dir[0] != '\0') {
        if (lsandbox_remove_recursive(cfg->sandbox_dir) < 0) {
            rc = -1;
        }
    }

    if (cfg->work_state_dir[0] != '\0') {
        if (remove_var_tmp_state(cfg->work_state_dir) != 0) {
            rc = -1;
        }
    }

    return rc;
}