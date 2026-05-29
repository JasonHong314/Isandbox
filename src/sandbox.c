#include <stdio.h>
#include <string.h>

#include "sandbox.h"
#include "namespace.h"

void sandbox_config_init(sandbox_config_t *cfg) {
    if (cfg == NULL) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));

    snprintf(cfg->name, sizeof(cfg->name), "default");

    /*
     * 第二阶段默认开启这些 namespace。
     */
    cfg->enable_net = 0;
    cfg->enable_pid_ns = 1;
    cfg->enable_mount_ns = 1;
    cfg->enable_uts_ns = 1;
    cfg->enable_ipc_ns = 1;

    /*
     * user namespace 先不开。
     * 它需要额外写 uid_map/gid_map，第三四阶段以后再做。
     */
    cfg->enable_user_ns = 0;

    /*
     * seccomp 后面再做。
     */
    cfg->enable_seccomp = 0;

    cfg->memory_limit[0] = '\0';
    cfg->pids_limit = 0;
    cfg->cpu_percent = 0;

    cfg->cmd_argv = NULL;
}

int sandbox_run(sandbox_config_t *cfg) {
    if (cfg == NULL || cfg->cmd_argv == NULL || cfg->cmd_argv[0] == NULL) {
        fprintf(stderr, "Error: invalid sandbox config or empty command\n");
        return 1;
    }

    return lsandbox_clone_run(cfg);
}