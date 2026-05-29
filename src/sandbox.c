#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sandbox.h"

void sandbox_config_init(sandbox_config_t *cfg) {
    if (cfg == NULL) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));

    snprintf(cfg->name, sizeof(cfg->name), "default");

    cfg->enable_net = 0;
    cfg->enable_pid_ns = 0;
    cfg->enable_mount_ns = 0;
    cfg->enable_uts_ns = 0;
    cfg->enable_ipc_ns = 0;
    cfg->enable_user_ns = 0;
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

    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Error: fork failed: %s\n", strerror(errno));
        return 1;
    }

    if (pid == 0) {
        /*
         * 子进程：
         * 使用 execvp 执行用户指定的命令。
         *
         * execvp 成功后不会返回；
         * 如果返回，说明执行失败。
         */
        execvp(cfg->cmd_argv[0], cfg->cmd_argv);

        fprintf(stderr, "Error: execvp failed for '%s': %s\n",
                cfg->cmd_argv[0], strerror(errno));

        _exit(127);
    }

    /*
     * 父进程：
     * 等待子进程退出。
     */
    int status = 0;

    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "Error: waitpid failed: %s\n", strerror(errno));
        return 1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        fprintf(stderr, "Process killed by signal %d\n", sig);
        return 128 + sig;
    }

    return 1;
}