#define _GNU_SOURCE

#include <errno.h>
#include <grp.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cgroup.h"
#include "log.h"
#include "namespace.h"
#include "overlay.h"
#include "rootfs.h"
#include "sandbox.h"
#include "seccomp_filter.h"

static int is_plain_bash(char **argv) {
    if (argv == NULL || argv[0] == NULL) {
        return 0;
    }

    if (argv[1] != NULL) {
        return 0;
    }

    return strcmp(argv[0], "bash") == 0 || strcmp(argv[0], "/bin/bash") == 0;
}

static void setup_fake_sandbox_prompt(void) {
    setenv("LSANDBOX", "1", 1);
    setenv("LSANDBOX_USER", "sandbox", 1);
    setenv("LSANDBOX_HOST", "lsandbox", 1);
    setenv("PS1", "sandbox@lsandbox:\\w\\$ ", 1);
}

static int drop_privileges(const sandbox_config_t *cfg) {
    if (cfg == NULL || !cfg->enable_drop_privs) {
        return 0;
    }

    if (geteuid() != 0) {
        return 0;
    }

    if (setgroups(0, NULL) < 0) {
        fprintf(stderr, "Error: setgroups failed: %s\n", strerror(errno));
        return -1;
    }

    if (setgid(cfg->run_gid) < 0) {
        fprintf(stderr, "Error: setgid(%d) failed: %s\n",
                (int)cfg->run_gid, strerror(errno));
        return -1;
    }

    if (setuid(cfg->run_uid) < 0) {
        fprintf(stderr, "Error: setuid(%d) failed: %s\n",
                (int)cfg->run_uid, strerror(errno));
        return -1;
    }

    if (cfg->run_user[0] != '\0') {
        setenv("USER", cfg->run_user, 1);
        setenv("LOGNAME", cfg->run_user, 1);
    }

    if (cfg->run_home[0] != '\0') {
        setenv("HOME", cfg->run_home, 1);
    }

    return 0;
}

static int setup_mount_namespace(sandbox_config_t *cfg) {
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) {
        fprintf(stderr, "Error: make / private failed: %s\n", strerror(errno));
        return -1;
    }

    if (cfg->enable_root_overlay) {
        if (lsandbox_enter_rootfs(cfg) < 0) {
            return -1;
        }

        if (lsandbox_setup_sandbox_resolv_conf() < 0) {
            fprintf(stderr, "Warning: failed to setup sandbox resolv.conf\n");
        }

        return 0;
    }

    if (cfg->enable_tmp_overlay) {
        if (lsandbox_mount_tmp_overlay(cfg) < 0) {
            return -1;
        }
    }

    if (cfg->enable_workdir_overlay) {
        if (lsandbox_mount_workdir_overlay(cfg) < 0) {
            return -1;
        }

        if (chdir(cfg->target_work_dir) < 0) {
            fprintf(stderr, "Error: chdir to workdir overlay target failed: %s\n",
                    strerror(errno));
            return -1;
        }
    }

    if (mount("proc", "/proc", "proc",
              MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) < 0) {
        fprintf(stderr, "Error: mount /proc failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static int child_func(void *arg) {
    sandbox_config_t *cfg = (sandbox_config_t *)arg;

    if (cfg->enable_uts_ns) {
        if (sethostname("lsandbox", strlen("lsandbox")) < 0) {
            fprintf(stderr, "Error: sethostname failed: %s\n", strerror(errno));
            return 1;
        }
    }

    if (cfg->enable_mount_ns) {
        if (setup_mount_namespace(cfg) < 0) {
            return 1;
        }
    }

    setup_fake_sandbox_prompt();

    if (drop_privileges(cfg) < 0) {
        return 1;
    }

    if (cfg->seccomp_mode != LSANDBOX_SECCOMP_OFF) {
        if (lsandbox_install_seccomp_filter(cfg->seccomp_mode) < 0) {
            fprintf(stderr, "Error: failed to install seccomp filter\n");
            return 1;
        }
    }

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
        flags |= CLONE_NEWNET;
    }

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

    void *stack_top = (char *)stack + LSANDBOX_STACK_SIZE;

    int flags = build_clone_flags(cfg);

    pid_t pid = clone(child_func, stack_top, flags, cfg);
    if (pid < 0) {
        fprintf(stderr, "Error: clone failed: %s\n", strerror(errno));
        fprintf(stderr, "Hint: try running with sudo.\n");
        free(stack);
        return 1;
    }

    lsandbox_log_start(cfg, pid);

    if (cfg->enable_cgroup) {
        if (lsandbox_cgroup_apply(cfg, pid) < 0) {
            fprintf(stderr, "Warning: failed to apply cgroup limits\n");
        }
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "Error: waitpid failed: %s\n", strerror(errno));
        free(stack);
        return 1;
    }

    if (cfg->enable_cgroup) {
        if (lsandbox_cgroup_cleanup(cfg) < 0) {
            fprintf(stderr, "Warning: cgroup cleanup failed\n");
        }
    }

    free(stack);

    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        lsandbox_log_exit(cfg, pid, code);
        return code;
    }

    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        lsandbox_log_signal(cfg, pid, sig);
        fprintf(stderr, "Process killed by signal %d\n", sig);
        return 128 + sig;
    }

    return 1;
}