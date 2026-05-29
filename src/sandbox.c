#include <stdio.h>
#include <string.h>

#include "sandbox.h"
#include "namespace.h"
#include "overlay.h"

static int safe_snprintf(char *buf, size_t size, const char *fmt, const char *arg) {
    int n = snprintf(buf, size, fmt, arg);

    if (n < 0) {
        fprintf(stderr, "Error: snprintf failed\n");
        return -1;
    }

    if ((size_t)n >= size) {
        fprintf(stderr, "Error: path too long: ");
        fprintf(stderr, fmt, arg);
        fprintf(stderr, "\n");
        return -1;
    }

    return 0;
}

static int build_sandbox_paths(sandbox_config_t *cfg) {
    if (safe_snprintf(cfg->sandbox_dir,
                      sizeof(cfg->sandbox_dir),
                      "sandboxes/%s",
                      cfg->name) < 0) {
        return -1;
    }

    if (safe_snprintf(cfg->upper_tmp_dir,
                      sizeof(cfg->upper_tmp_dir),
                      "%s/upper_tmp",
                      cfg->sandbox_dir) < 0) {
        return -1;
    }

    if (safe_snprintf(cfg->work_tmp_dir,
                      sizeof(cfg->work_tmp_dir),
                      "%s/work_tmp",
                      cfg->sandbox_dir) < 0) {
        return -1;
    }

    if (safe_snprintf(cfg->merged_tmp_dir,
                      sizeof(cfg->merged_tmp_dir),
                      "%s/merged_tmp",
                      cfg->sandbox_dir) < 0) {
        return -1;
    }

    if (safe_snprintf(cfg->cgroup_dir,
                      sizeof(cfg->cgroup_dir),
                      "/sys/fs/cgroup/lsandbox/%s",
                      cfg->name) < 0) {
        return -1;
    }

    return 0;
}

void sandbox_config_init(sandbox_config_t *cfg) {
    if (cfg == NULL) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));

    snprintf(cfg->name, sizeof(cfg->name), "default");

    cfg->enable_net = 0;
    cfg->enable_pid_ns = 1;
    cfg->enable_mount_ns = 1;
    cfg->enable_uts_ns = 1;
    cfg->enable_ipc_ns = 1;

    cfg->enable_user_ns = 0;
    cfg->enable_seccomp = 0;

    cfg->enable_tmp_overlay = 1;
    cfg->remove_after_exit = 0;

    cfg->enable_cgroup = 0;
    cfg->memory_limit[0] = '\0';
    cfg->pids_limit = 0;
    cfg->cpu_percent = 0;

    cfg->cmd_argv = NULL;

    if (build_sandbox_paths(cfg) < 0) {
        fprintf(stderr, "Error: failed to build default sandbox paths\n");
    }
}

int sandbox_run(sandbox_config_t *cfg) {
    int rc;

    if (cfg == NULL || cfg->cmd_argv == NULL || cfg->cmd_argv[0] == NULL) {
        fprintf(stderr, "Error: invalid sandbox config or empty command\n");
        return 1;
    }

    if (build_sandbox_paths(cfg) < 0) {
        return 1;
    }

    if (cfg->enable_tmp_overlay) {
        if (lsandbox_prepare_overlay_dirs(cfg) < 0) {
            return 1;
        }
    }

    rc = lsandbox_clone_run(cfg);

    if (cfg->remove_after_exit) {
        if (lsandbox_cleanup_overlay_dirs(cfg) < 0) {
            fprintf(stderr, "Warning: failed to cleanup sandbox directory '%s'\n",
                    cfg->sandbox_dir);
        }
    }

    return rc;
}
