#ifndef LSANDBOX_SANDBOX_H
#define LSANDBOX_SANDBOX_H

#include <sys/types.h>

#define LSANDBOX_NAME_MAX 128
#define LSANDBOX_PATH_MAX 4096

typedef enum lsandbox_seccomp_mode {
    LSANDBOX_SECCOMP_OFF = 0,
    LSANDBOX_SECCOMP_BASIC = 1,
    LSANDBOX_SECCOMP_STRICT = 2
} lsandbox_seccomp_mode_t;

typedef struct sandbox_config {
    char name[LSANDBOX_NAME_MAX];

    char sandbox_dir[LSANDBOX_PATH_MAX];

    char upper_root_dir[LSANDBOX_PATH_MAX];
    char work_root_dir[LSANDBOX_PATH_MAX];
    char merged_root_dir[LSANDBOX_PATH_MAX];

    char upper_tmp_dir[LSANDBOX_PATH_MAX];
    char work_tmp_dir[LSANDBOX_PATH_MAX];
    char merged_tmp_dir[LSANDBOX_PATH_MAX];
    char sandbox_tmp_dir[LSANDBOX_PATH_MAX];

    char target_work_dir[LSANDBOX_PATH_MAX];
    char work_state_dir[LSANDBOX_PATH_MAX];
    char upper_work_dir[LSANDBOX_PATH_MAX];
    char work_work_dir[LSANDBOX_PATH_MAX];
    char merged_work_dir[LSANDBOX_PATH_MAX];

    char cgroup_dir[LSANDBOX_PATH_MAX];

    int enable_net;
    int enable_pid_ns;
    int enable_mount_ns;
    int enable_uts_ns;
    int enable_ipc_ns;
    int enable_user_ns;

    lsandbox_seccomp_mode_t seccomp_mode;

    int enable_root_overlay;
    int enable_tmp_overlay;
    int enable_workdir_overlay;
    int remove_after_exit;

    int enable_cgroup;
    int keep_cgroup;
    char memory_limit[64];
    int pids_limit;
    int cpu_percent;

    int enable_drop_privs;
    uid_t run_uid;
    gid_t run_gid;
    char run_user[128];
    char run_home[LSANDBOX_PATH_MAX];

    char **cmd_argv;
} sandbox_config_t;

void sandbox_config_init(sandbox_config_t *cfg);
int sandbox_run(sandbox_config_t *cfg);

#endif