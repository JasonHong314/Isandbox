#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#include "log.h"
#include "sandbox.h"
#include "utils.h"

/*
 * The log is only for checking my own runs.  It is not a full audit log;
 * I mostly used it to see which box name and limit set produced a failure.
 */

#define LOG_DIR "logs"
#define LOG_FILE "logs/lsandbox.log"

static const char *
secname(lsandbox_seccomp_mode_t mode)
{
  switch(mode){
    case LSANDBOX_SECCOMP_OFF:
      return "off";
    case LSANDBOX_SECCOMP_BASIC:
      return "basic";
    case LSANDBOX_SECCOMP_STRICT:
      return "strict";
    default:
      return "unknown";
  }
}

static const char *
netmode(const sandbox_config_t *cfg)
{
  return cfg->enable_net ? "host" : "off";
}

static void
timestr(char *buf, size_t size)
{
  time_t now = time(0);
  struct tm tm_now;

  localtime_r(&now, &tm_now);
  strftime(buf, size, "%Y-%m-%d %H:%M:%S", &tm_now);
}

static void
cmdstr(const sandbox_config_t *cfg, char *buf, size_t size)
{
  size_t used = 0;

  if(cfg == 0 || cfg->cmd_argv == 0 || cfg->cmd_argv[0] == 0){
    snprintf(buf, size, "(null)");
    return;
  }

  buf[0] = '\0';

  for(int i = 0; cfg->cmd_argv[i] != 0; i++){
    int n = snprintf(buf + used,
             size - used,
             "%s%s",
             i == 0 ? "" : " ",
             cfg->cmd_argv[i]);

    if(n < 0){
      break;
    }

    if((size_t)n >= size - used){
      break;
    }

    used += (size_t)n;
  }
}

static FILE *
logfp(void)
{
  if(lsandbox_mkdir_p(LOG_DIR, 0755) < 0){
    return 0;
  }

  FILE *fp = fopen(LOG_FILE, "a");
  if(fp == 0){
    fprintf(stderr, "lsandbox: warning: open log file '%s' failed: %s\n",
        LOG_FILE, strerror(errno));
    return 0;
  }

  return fp;
}

void
lsandbox_log_start(const sandbox_config_t *cfg, pid_t pid)
{
  char ts[64];
  char cmd[1024];
  FILE *fp;

  if(cfg == 0)
    return;

  timestr(ts, sizeof(ts));
  cmdstr(cfg, cmd, sizeof(cmd));

  fp = logfp();
  if(fp == 0){
    return;
  }

  fprintf(fp,
      "[%s] START name=%s pid=%d cmd=\"%s\" net=%s mem=%s pids=%d cpu=%d seccomp=%s tmp=%s workdir=%s rm=%d\n",
      ts,
      cfg->name,
      (int)pid,
      cmd,
      netmode(cfg),
      cfg->memory_limit[0] ? cfg->memory_limit : "none",
      cfg->pids_limit,
      cfg->cpu_percent,
      secname(cfg->seccomp_mode),
      cfg->enable_tmp_overlay ? "on" : "off",
      cfg->enable_workdir_overlay ? "on" : "off",
      cfg->remove_after_exit);

  fclose(fp);
}

void
lsandbox_log_exit(const sandbox_config_t *cfg, pid_t pid, int exit_code)
{
  char ts[64];
  FILE *fp;

  if(cfg == 0)
    return;

  timestr(ts, sizeof(ts));

  fp = logfp();
  if(fp == 0){
    return;
  }

  fprintf(fp,
      "[%s] EXIT  name=%s pid=%d code=%d\n",
      ts,
      cfg->name,
      (int)pid,
      exit_code);

  fclose(fp);
}

void
lsandbox_log_signal(const sandbox_config_t *cfg, pid_t pid, int signal_no)
{
  char ts[64];
  FILE *fp;

  if(cfg == 0)
    return;

  timestr(ts, sizeof(ts));

  fp = logfp();
  if(fp == 0){
    return;
  }

  fprintf(fp,
      "[%s] SIGNAL name=%s pid=%d signal=%d\n",
      ts,
      cfg->name,
      (int)pid,
      signal_no);

  fclose(fp);
}

int
lsandbox_log_show(void)
{
  FILE *fp = fopen(LOG_FILE, "r");
  char line[1024];

  if(fp == 0){
    printf("No log file found: %s\n", LOG_FILE);
    return 0;
  }

  while(fgets(line, sizeof(line), fp) != 0){
    fputs(line, stdout);
  }

  fclose(fp);
  return 0;
}
