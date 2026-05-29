#ifndef LSANDBOX_SANDBOX_H
#define LSANDBOX_SANDBOX_H

#define LSANDBOX_NAME_MAX 128

typedef struct sandbox_config {
    char name[LSANDBOX_NAME_MAX];

    /*
     * 第二阶段开始使用 namespace 配置。
     */
    int enable_net;
    int enable_pid_ns;
    int enable_mount_ns;
    int enable_uts_ns;
    int enable_ipc_ns;
    int enable_user_ns;
    int enable_seccomp;

    char memory_limit[64];
    int pids_limit;
    int cpu_percent;

    char **cmd_argv;
} sandbox_config_t;

void sandbox_config_init(sandbox_config_t *cfg);
int sandbox_run(sandbox_config_t *cfg);

#endif