#define _GNU_SOURCE

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sandbox.h"
#include "namespace.h"
#include "overlay.h"
#include "rootfs.h"

/*
 * Defaults live here.  I changed these several times while testing, so the
 * code is intentionally boring: fill cfg, derive paths, then call clone side.
 */


static int
strpath(char *buf, size_t size, const char *fmt, const char *arg)
{
  int n = snprintf(buf, size, fmt, arg);

  if(n < 0){
    fprintf(stderr, "lsandbox: snprintf failed\n");
    return -1;
  }

  if((size_t)n >= size){
    fprintf(stderr, "lsandbox: path too long: ");
    fprintf(stderr, fmt, arg);
    fprintf(stderr, "\n");
    return -1;
  }

  return 0;
}

static int
setpaths(sandbox_config_t *cfg)
{
  if(strpath(cfg->sandbox_dir,
            sizeof(cfg->sandbox_dir),
            "sandboxes/%s",
            cfg->name) < 0){
    return -1;
  }

  if(strpath(cfg->upper_root_dir,
            sizeof(cfg->upper_root_dir),
            "%s/upper_root",
            cfg->sandbox_dir) < 0){
    return -1;
  }

  if(strpath(cfg->work_root_dir,
            sizeof(cfg->work_root_dir),
            "%s/work_root",
            cfg->sandbox_dir) < 0){
    return -1;
  }

  if(strpath(cfg->merged_root_dir,
            sizeof(cfg->merged_root_dir),
            "%s/merged_root",
            cfg->sandbox_dir) < 0){
    return -1;
  }

  if(strpath(cfg->upper_tmp_dir,
            sizeof(cfg->upper_tmp_dir),
            "%s/upper_tmp",
            cfg->sandbox_dir) < 0){
    return -1;
  }

  if(strpath(cfg->work_tmp_dir,
            sizeof(cfg->work_tmp_dir),
            "%s/work_tmp",
            cfg->sandbox_dir) < 0){
    return -1;
  }

  if(strpath(cfg->merged_tmp_dir,
            sizeof(cfg->merged_tmp_dir),
            "%s/merged_tmp",
            cfg->sandbox_dir) < 0){
    return -1;
  }

  if(strpath(cfg->sandbox_tmp_dir,
            sizeof(cfg->sandbox_tmp_dir),
            "%s/tmp",
            cfg->sandbox_dir) < 0){
    return -1;
  }

  if(strpath(cfg->work_state_dir,
            sizeof(cfg->work_state_dir),
            "/var/tmp/lsandbox/%s",
            cfg->name) < 0){
    return -1;
  }

  if(strpath(cfg->upper_work_dir,
            sizeof(cfg->upper_work_dir),
            "%s/upper_work",
            cfg->work_state_dir) < 0){
    return -1;
  }

  if(strpath(cfg->work_work_dir,
            sizeof(cfg->work_work_dir),
            "%s/work_work",
            cfg->work_state_dir) < 0){
    return -1;
  }

  if(strpath(cfg->merged_work_dir,
            sizeof(cfg->merged_work_dir),
            "%s/merged_work",
            cfg->work_state_dir) < 0){
    return -1;
  }

  if(strpath(cfg->cgroup_dir,
            sizeof(cfg->cgroup_dir),
            "/sys/fs/cgroup/lsandbox_%s",
            cfg->name) < 0){
    return -1;
  }

  return 0;
}

void
sandbox_config_init(sandbox_config_t *cfg)
{
  if(cfg == 0)
    return;

  memset(cfg, 0, sizeof(*cfg));

  snprintf(cfg->name, sizeof(cfg->name), "default");

  cfg->enable_net = 1;

  cfg->enable_pid_ns = 1;
  cfg->enable_mount_ns = 1;
  cfg->enable_uts_ns = 1;
  cfg->enable_ipc_ns = 1;
  cfg->enable_user_ns = 0;

  cfg->seccomp_mode = LSANDBOX_SECCOMP_BASIC;

  cfg->enable_root_overlay = 1;
  cfg->enable_tmp_overlay = 0;
  cfg->enable_workdir_overlay = 0;
  cfg->remove_after_exit = 0;

  cfg->enable_cgroup = 1;
  cfg->keep_cgroup = 0;
  snprintf(cfg->memory_limit, sizeof(cfg->memory_limit), "1G");
  cfg->pids_limit = 128;
  cfg->cpu_percent = 100;

  cfg->enable_drop_privs = 1;
  cfg->run_uid = getuid();
  cfg->run_gid = getgid();
  snprintf(cfg->run_user, sizeof(cfg->run_user), "unknown");
  cfg->run_home[0] = '\0';

  const char *sudo_uid = getenv("SUDO_UID");
  const char *sudo_gid = getenv("SUDO_GID");

  if(sudo_uid != 0 && sudo_gid != 0){
    cfg->run_uid = (uid_t)atoi(sudo_uid);
    cfg->run_gid = (gid_t)atoi(sudo_gid);
  }

  struct passwd *pw = getpwuid(cfg->run_uid);
  if(pw != 0){
    snprintf(cfg->run_user, sizeof(cfg->run_user), "%s", pw->pw_name);
    snprintf(cfg->run_home, sizeof(cfg->run_home), "%s", pw->pw_dir);
  }

  cfg->cmd_argv = 0;

  if(getcwd(cfg->target_work_dir, sizeof(cfg->target_work_dir)) == 0){
    perror("getcwd");
    cfg->target_work_dir[0] = '\0';
  }

  if(setpaths(cfg) < 0){
    fprintf(stderr, "lsandbox: cannot build default sandbox paths\n");
  }
}

int
sandbox_run(sandbox_config_t *cfg)
{
  int rc;

  if(cfg == 0 || cfg->cmd_argv == 0 || cfg->cmd_argv[0] == 0){
    fprintf(stderr, "lsandbox: invalid sandbox config or empty command\n");
    return 1;
  }

  if(cfg->target_work_dir[0] == '\0'){
    if(getcwd(cfg->target_work_dir, sizeof(cfg->target_work_dir)) == 0){
      perror("getcwd");
      return 1;
    }
  }

  if(setpaths(cfg) < 0){
    return 1;
  }

  if(cfg->enable_root_overlay){
    if(lsandbox_prepare_rootfs_dirs(cfg) < 0){
      return 1;
    }
  }

  if(!cfg->enable_root_overlay && cfg->enable_tmp_overlay){
    if(lsandbox_prepare_overlay_dirs(cfg) < 0){
      return 1;
    }
  }

  if(!cfg->enable_root_overlay && cfg->enable_workdir_overlay){
    if(lsandbox_prepare_workdir_overlay_dirs(cfg) < 0){
      return 1;
    }
  }

  rc = lsandbox_clone_run(cfg);

  if(cfg->remove_after_exit){
    if(lsandbox_cleanup_overlay_dirs(cfg) < 0){
      fprintf(stderr, "lsandbox: warning: cannot cleanup sandbox state\n");
    }
  }

  return rc;
}
