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

/*
 * This file is the hand-off from lsandbox to the real sandbox process.
 * clone flags, mount namespace setup, uid drop and seccomp must happen in
 * this order or some tests become hard to explain.
 */


static int
onebash(char **argv)
{
  if(argv == 0 || argv[0] == 0){
    return 0;
  }

  if(argv[1] != 0){
    return 0;
  }

  return strcmp(argv[0], "bash") == 0 || strcmp(argv[0], "/bin/bash") == 0;
}

static void
boxenv(void)
{
  setenv("LSANDBOX", "1", 1);
  setenv("LSANDBOX_USER", "sandbox", 1);
  setenv("LSANDBOX_HOST", "lsandbox", 1);
  setenv("PS1", "sandbox@lsandbox:\\w\\$ ", 1);
}

static int
touser(const sandbox_config_t *cfg)
{
  if(cfg == 0 || !cfg->enable_drop_privs){
    return 0;
  }

  if(geteuid() != 0){
    return 0;
  }

  if(setgroups(0, 0) < 0){
    fprintf(stderr, "lsandbox: setgroups failed: %s\n", strerror(errno));
    return -1;
  }

  if(setgid(cfg->run_gid) < 0){
    fprintf(stderr, "lsandbox: setgid(%d) failed: %s\n",
        (int)cfg->run_gid, strerror(errno));
    return -1;
  }

  if(setuid(cfg->run_uid) < 0){
    fprintf(stderr, "lsandbox: setuid(%d) failed: %s\n",
        (int)cfg->run_uid, strerror(errno));
    return -1;
  }

  if(cfg->run_user[0] != '\0'){
    setenv("USER", cfg->run_user, 1);
    setenv("LOGNAME", cfg->run_user, 1);
  }

  if(cfg->run_home[0] != '\0'){
    setenv("HOME", cfg->run_home, 1);
  }

  return 0;
}

static int
mounts(sandbox_config_t *cfg)
{
  if(mount(0, "/", 0, MS_REC | MS_PRIVATE, 0) < 0){
    fprintf(stderr, "lsandbox: make / private failed: %s\n", strerror(errno));
    return -1;
  }

  if(cfg->enable_root_overlay){
    if(lsandbox_enter_rootfs(cfg) < 0){
      return -1;
    }

    return 0;
  }

  if(cfg->enable_tmp_overlay){
    if(lsandbox_mount_tmp_overlay(cfg) < 0){
      return -1;
    }
  }

  if(cfg->enable_workdir_overlay){
    if(lsandbox_mount_workdir_overlay(cfg) < 0){
      return -1;
    }

    if(chdir(cfg->target_work_dir) < 0){
      fprintf(stderr, "lsandbox: chdir to workdir overlay target failed: %s\n",
          strerror(errno));
      return -1;
    }
  }

  if(mount("proc", "/proc", "proc",
        MS_NOSUID | MS_NOEXEC | MS_NODEV, 0) < 0){
    fprintf(stderr, "lsandbox: mount /proc failed: %s\n", strerror(errno));
    return -1;
  }

  return 0;
}

static int
child1(void *arg)
{
  sandbox_config_t *cfg = (sandbox_config_t *)arg;

  if(cfg->enable_uts_ns){
    if(sethostname("lsandbox", strlen("lsandbox")) < 0){
      fprintf(stderr, "lsandbox: sethostname failed: %s\n", strerror(errno));
      return 1;
    }
  }

  if(cfg->enable_mount_ns){
    if(mounts(cfg) < 0){
      return 1;
    }
  }

  boxenv();

  if(touser(cfg) < 0){
    return 1;
  }

  if(cfg->seccomp_mode != LSANDBOX_SECCOMP_OFF){
    if(lsandbox_install_seccomp_filter(cfg->seccomp_mode) < 0){
      fprintf(stderr, "lsandbox: cannot install seccomp filter\n");
      return 1;
    }
  }

  if(onebash(cfg->cmd_argv)){
    char *bash_argv[] = {
      "bash",
      "--noprofile",
      "--norc",
      "-i",
      0
    };

    execvp("bash", bash_argv);
    fprintf(stderr, "lsandbox: execvp bash failed: %s\n", strerror(errno));
    return 127;
  }

  execvp(cfg->cmd_argv[0], cfg->cmd_argv);

  fprintf(stderr, "lsandbox: execvp failed for '%s': %s\n",
      cfg->cmd_argv[0], strerror(errno));

  return 127;
}

static int
build_clone_flags(const sandbox_config_t *cfg)
{
  int clone_flags = SIGCHLD;

  if(cfg->enable_mount_ns){
    clone_flags |= CLONE_NEWNS;
  }

  if(cfg->enable_pid_ns){
    clone_flags |= CLONE_NEWPID;
  }

  if(cfg->enable_uts_ns){
    clone_flags |= CLONE_NEWUTS;
  }

  if(cfg->enable_ipc_ns){
    clone_flags |= CLONE_NEWIPC;
  }

  if(cfg->enable_net == 0){
    clone_flags |= CLONE_NEWNET;
  }

  return clone_flags;
}

int
lsandbox_clone_run(sandbox_config_t *cfg)
{
  if(cfg == 0 || cfg->cmd_argv == 0 || cfg->cmd_argv[0] == 0){
    fprintf(stderr, "lsandbox: invalid sandbox config\n");
    return 1;
  }

  void *stack = malloc(LSANDBOX_STACK_SIZE);
  if(stack == 0){
    fprintf(stderr, "lsandbox: malloc stack failed: %s\n", strerror(errno));
    return 1;
  }

  void *stack_top = (char *)stack + LSANDBOX_STACK_SIZE;

  int clone_flags = build_clone_flags(cfg);

  pid_t pid = clone(child1, stack_top, clone_flags, cfg);
  if(pid < 0){
    fprintf(stderr, "lsandbox: clone failed: %s\n", strerror(errno));
    fprintf(stderr, "hint: try running with sudo.\n");
    free(stack);
    return 1;
  }

  lsandbox_log_start(cfg, pid);

  if(cfg->enable_cgroup){
    if(lsandbox_cgroup_apply(cfg, pid) < 0){
      fprintf(stderr, "lsandbox: warning: cannot apply cgroup limits\n");
    }
  }

  int status = 0;
  if(waitpid(pid, &status, 0) < 0){
    fprintf(stderr, "lsandbox: waitpid failed: %s\n", strerror(errno));
    free(stack);
    return 1;
  }

  if(cfg->enable_cgroup){
    if(lsandbox_cgroup_cleanup(cfg) < 0){
      fprintf(stderr, "lsandbox: warning: cgroup cleanup failed\n");
    }
  }

  free(stack);

  if(WIFEXITED(status)){
    int code = WEXITSTATUS(status);
    lsandbox_log_exit(cfg, pid, code);
    return code;
  }

  if(WIFSIGNALED(status)){
    int sig = WTERMSIG(status);
    lsandbox_log_signal(cfg, pid, sig);
    fprintf(stderr, "Process killed by signal %d\n", sig);
    return 128 + sig;
  }

  return 1;
}
