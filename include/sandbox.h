#ifndef LSANDBOX_SANDBOX_H
#define LSANDBOX_SANDBOX_H

#define LSANDBOX_NAME_MAX 128
#define LSANDBOX_PATH_MAX 4096

typedef struct sandbox_config {
    char name[LSANDBOX_NAME_MAX];

    /*
     * 沙盒运行目录：
     *
     * sandboxes/<name>/
     * ├── upper_tmp/
     * ├── work_tmp/
     * └── merged_tmp/
     */
    char sandbox_dir[LSANDBOX_PATH_MAX];
    char upper_tmp_dir[LSANDBOX_PATH_MAX];
    char work_tmp_dir[LSANDBOX_PATH_MAX];
    char merged_tmp_dir[LSANDBOX_PATH_MAX];

    /*
     * cgroup 目录：
     * /sys/fs/cgroup/lsandbox/<name>
     */
    char cgroup_dir[LSANDBOX_PATH_MAX];

    /*
     * Namespace 配置。
     */
    int enable_net;
    int enable_pid_ns;
    int enable_mount_ns;
    int enable_uts_ns;
    int enable_ipc_ns;
    int enable_user_ns;
    int enable_seccomp;

    /*
     * 文件系统隔离。
     */
    int enable_tmp_overlay;

    /*
     * 是否在沙盒退出后删除写入层。
     *
     * 0：持久沙盒，保留 sandboxes/<name>
     * 1：临时沙盒，退出后删除 sandboxes/<name>
     */
    int remove_after_exit;

    /*
     * cgroups v2 资源限制。
     */
    int enable_cgroup;
    char memory_limit[64];
    int pids_limit;
    int cpu_percent;

    char **cmd_argv;
} sandbox_config_t;

void sandbox_config_init(sandbox_config_t *cfg);
int sandbox_run(sandbox_config_t *cfg);

#endif
