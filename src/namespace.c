#define _GNU_SOURCE

#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>

#include "namespace.h"
#include "sandbox.h"

static int is_plain_bash(char **argv) {
    if (argv == NULL || argv[0] == NULL) {
        return 0;
    }

    /*
     * 只处理：
     *   ./lsandbox run -- bash
     *   ./lsandbox run -- /bin/bash
     *
     * 如果用户执行 bash -c xxx，则不修改参数。
     */
    if (argv[1] != NULL) {
        return 0;
    }

    return strcmp(argv[0], "bash") == 0 || strcmp(argv[0], "/bin/bash") == 0;
}

static void setup_fake_sandbox_prompt(void) {
    /*
     * 这里只是让提示符看起来像沙盒用户。
     * 真实用户身份还没有改变。
     */
    setenv("LSANDBOX", "1", 1);
    setenv("LSANDBOX_USER", "sandbox", 1);
    setenv("LSANDBOX_HOST", "lsandbox", 1);
    setenv("PS1", "sandbox@lsandbox:\\w\\$ ", 1);
}

static int setup_mount_namespace(void) {
    /*
     * 关键点：
     * 进入新的 mount namespace 后，要先把挂载传播设置为 private。
     * 否则沙盒内的挂载行为可能传播到外部。
     */
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) {
        fprintf(stderr, "Error: make / private failed: %s\n", strerror(errno));
        return -1;
    }

    /*
     * PID namespace 里必须重新挂载 /proc。
     * ps、top 等工具读取 /proc 来显示进程。
     *
     * 这里直接在新的 mount namespace 内覆盖挂载 proc。
     * 不会影响主机的 /proc。
     */
    if (mount("proc", "/proc", "proc",
              MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) < 0) {
        fprintf(stderr, "Error: mount /proc failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static int child_func(void *arg) {
    sandbox_config_t *cfg = (sandbox_config_t *)arg;

    /*
     * UTS namespace：设置沙盒 hostname。
     * 如果 UTS namespace 生效，这不会影响主机。
     */
    if (cfg->enable_uts_ns) {
        if (sethostname("lsandbox", strlen("lsandbox")) < 0) {
            fprintf(stderr, "Error: sethostname failed: %s\n", strerror(errno));
            return 1;
        }
    }

    /*
     * Mount namespace：设置挂载隔离，并重新挂载 /proc。
     */
    if (cfg->enable_mount_ns) {
        if (setup_mount_namespace() < 0) {
            return 1;
        }
    }

    setup_fake_sandbox_prompt();

    /*
     * 如果用户直接运行 bash，就改成不读取 ~/.bashrc 的交互 bash。
     * 这样可以避免 conda 的 (base) 提示符覆盖我们的 PS1。
     */
    if (is_plain_bash(cfg->cmd_argv)) {
        char *bash_argv[] = {
            "bash",
            "--noprofile",
            "--norc",
            "-i",
            NULL
        };

        execvp("bash", bash_argv);
        fprintf(stderr, "Error: execvp bash failed: %s\n", strerror(errno));
        return 127;
    }

    execvp(cfg->cmd_argv[0], cfg->cmd_argv);

    fprintf(stderr, "Error: execvp failed for '%s': %s\n",
            cfg->cmd_argv[0], strerror(errno));

    return 127;
}

static int build_clone_flags(const sandbox_config_t *cfg) {
    int flags = SIGCHLD;

    if (cfg->enable_mount_ns) {
        flags |= CLONE_NEWNS;
    }

    if (cfg->enable_pid_ns) {
        flags |= CLONE_NEWPID;
    }

    if (cfg->enable_uts_ns) {
        flags |= CLONE_NEWUTS;
    }

    if (cfg->enable_ipc_ns) {
        flags |= CLONE_NEWIPC;
    }

    if (cfg->enable_net == 0) {
        /*
         * 第二阶段先默认禁网。
         * 创建新的 network namespace 后，沙盒内不会直接拥有主机网卡。
         *
         * 如果你暂时不想禁网，可以把这一段注释掉。
         */
        flags |= CLONE_NEWNET;
    }

    /*
     * user namespace 暂时不启用。
     * 后续需要 UID/GID 映射。
     */
    return flags;
}

int lsandbox_clone_run(sandbox_config_t *cfg) {
    if (cfg == NULL || cfg->cmd_argv == NULL || cfg->cmd_argv[0] == NULL) {
        fprintf(stderr, "Error: invalid sandbox config\n");
        return 1;
    }

    void *stack = malloc(LSANDBOX_STACK_SIZE);
    if (stack == NULL) {
        fprintf(stderr, "Error: malloc stack failed: %s\n", strerror(errno));
        return 1;
    }

    /*
     * clone 的栈从高地址向低地址增长。
     */
    void *stack_top = (char *)stack + LSANDBOX_STACK_SIZE;

    int flags = build_clone_flags(cfg);

    pid_t pid = clone(child_func, stack_top, flags, cfg);
    if (pid < 0) {
        fprintf(stderr, "Error: clone failed: %s\n", strerror(errno));
        fprintf(stderr, "Hint: try running with sudo.\n");
        free(stack);
        return 1;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "Error: waitpid failed: %s\n", strerror(errno));
        free(stack);
        return 1;
    }

    free(stack);

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