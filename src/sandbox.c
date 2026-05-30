#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

    if (safe_snprintf(cfg->work_state_dir,
                      sizeof(cfg->work_state_dir),
                      "/var/tmp/lsandbox/%s",
                      cfg->name) < 0) {
        return -1;
    }

    if (safe_snprintf(cfg->upper_work_dir,
                      sizeof(cfg->upper_work_dir),
                      "%s/upper_work",
                      cfg->work_state_dir) < 0) {
        return -1;
    }

    if (safe_snprintf(cfg->work_work_dir,
                      sizeof(cfg->work_work_dir),
                      "%s/work_work",
                      cfg->work_state_dir) < 0) {
        return -1;
    }

    if (safe_snprintf(cfg->merged_work_dir,
                      sizeof(cfg->merged_work_dir),
                      "%s/merged_work",
                      cfg->work_state_dir) < 0) {
        return -1;
    }

    if (safe_snprintf(cfg->cgroup_dir,
                      sizeof(cfg->cgroup_dir),
                      "/sys/fs/cgroup/lsandbox_%s",
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

    /*
     * Namespace 默认配置。
     *
     * 默认使用主机网络，方便 pip、curl、apt 等联网操作。
     * 如果用户需要禁网，可以显式传：
     *
     *   --net off
     */
    cfg->enable_net = 1;

    cfg->enable_pid_ns = 1;
    cfg->enable_mount_ns = 1;
    cfg->enable_uts_ns = 1;
    cfg->enable_ipc_ns = 1;
    cfg->enable_user_ns = 0;

    /*
     * 默认启用 basic seccomp。
     * basic 模式兼容性较好，同时禁止 mount、ptrace、reboot 等高危 syscall。
     */
    cfg->seccomp_mode = LSANDBOX_SECCOMP_BASIC;

    /*
     * 默认隔离 /tmp。
     * workdir overlay 默认关闭，避免用户误以为写入文件会保存在真实目录。
     */
    cfg->enable_tmp_overlay = 1;
    cfg->enable_workdir_overlay = 0;
    cfg->remove_after_exit = 0;

    /*
     * 默认启用 cgroup，给出适中的资源限制。
     *
     * --mem 1G：
     *   适合 Python、小型 C/C++ 程序、小规模 pip 安装。
     *
     * --pids 128：
     *   防止 fork bomb，同时不影响普通程序。
     *
     * --cpu 100：
     *   约等于最多使用 1 个 CPU 核心。
     */
    cfg->enable_cgroup = 1;
    cfg->keep_cgroup = 0;
    snprintf(cfg->memory_limit, sizeof(cfg->memory_limit), "1G");
    cfg->pids_limit = 128;
    cfg->cpu_percent = 100;

    cfg->cmd_argv = NULL;

    if (getcwd(cfg->target_work_dir, sizeof(cfg->target_work_dir)) == NULL) {
        perror("getcwd");
        cfg->target_work_dir[0] = '\0';
    }

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

    if (cfg->target_work_dir[0] == '\0') {
        if (getcwd(cfg->target_work_dir, sizeof(cfg->target_work_dir)) == NULL) {
            perror("getcwd");
            return 1;
        }
    }

    if (build_sandbox_paths(cfg) < 0) {
        return 1;
    }

    if (cfg->enable_tmp_overlay) {
        if (lsandbox_prepare_overlay_dirs(cfg) < 0) {
            return 1;
        }
    }

    if (cfg->enable_workdir_overlay) {
        if (lsandbox_prepare_workdir_overlay_dirs(cfg) < 0) {
            return 1;
        }
    }

    rc = lsandbox_clone_run(cfg);

    if (cfg->remove_after_exit) {
        if (lsandbox_cleanup_overlay_dirs(cfg) < 0) {
            fprintf(stderr, "Warning: failed to cleanup sandbox state\n");
        }
    }

    return rc;
}