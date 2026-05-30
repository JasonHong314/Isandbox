#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

static int build_tmp_overlay_options(char *buf,
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
        fprintf(stderr, "Error: snprintf tmp overlay options failed\n");
        return -1;
    }

    if ((size_t)n >= size) {
        fprintf(stderr, "Error: tmp overlay mount options too long\n");
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

    char options[LSANDBOX_OVERLAY_OPT_MAX];

    if (build_tmp_overlay_options(options, sizeof(options), cfg) < 0) {
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