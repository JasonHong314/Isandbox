#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cgroup.h"
#include "sandbox.h"
#include "utils.h"

/*
 * cgroup code used by the fork bomb and memory bomb tests.
 * I kept it flat on purpose: write a few v2 files, put the child pid in,
 * and leave cleanup conservative when the kernel says the group is still busy.
 */

#define CGROUP_ROOT "/sys/fs/cgroup"

static int
cgw(const char *path, const char *text)
{
  FILE *fp = fopen(path, "w");

  if(fp == 0){
    fprintf(stderr, "lsandbox: open '%s' failed: %s\n", path, strerror(errno));
    return -1;
  }

  if(fputs(text, fp) == EOF){
    fprintf(stderr, "lsandbox: write '%s' failed: %s\n", path, strerror(errno));
    fclose(fp);
    return -1;
  }

  fclose(fp);
  return 0;
}

static int
cgr(const char *path, char *buf, size_t size)
{
  FILE *fp = fopen(path, "r");

  if(fp == 0){
    return -1;
  }

  if(fgets(buf, size, fp) == 0){
    fclose(fp);
    return -1;
  }

  fclose(fp);
  return 0;
}

static int
cgpath(char *buf, size_t size, const char *dir, const char *name)
{
  int n = snprintf(buf, size, "%s/%s", dir, name);

  if(n < 0 || (size_t)n >= size){
    fprintf(stderr, "lsandbox: path too long: %s/%s\n", dir, name);
    return -1;
  }

  return 0;
}

static int
sizeto(const char *text, unsigned long long *out)
{
  char *end = 0;
  unsigned long long value;

  if(text == 0 || text[0] == '\0' || out == 0){
    return -1;
  }

  errno = 0;
  value = strtoull(text, &end, 10);

  if(errno != 0 || end == text){
    return -1;
  }

  if(*end == '\0'){
    *out = value;
    return 0;
  }

  if((end[0] == 'K' || end[0] == 'k') && end[1] == '\0'){
    *out = value * 1024ULL;
    return 0;
  }

  if((end[0] == 'M' || end[0] == 'm') && end[1] == '\0'){
    *out = value * 1024ULL * 1024ULL;
    return 0;
  }

  if((end[0] == 'G' || end[0] == 'g') && end[1] == '\0'){
    *out = value * 1024ULL * 1024ULL * 1024ULL;
    return 0;
  }

  return -1;
}

static int
iscgv2(void)
{
  if(!lsandbox_path_exists(CGROUP_ROOT "/cgroup.controllers")){
    fprintf(stderr, "lsandbox: cgroups v2 not detected at %s\n", CGROUP_ROOT);
    return -1;
  }

  return 0;
}

static int
rootctl(void)
{
  char buf[1024];

  if(cgr(CGROUP_ROOT "/cgroup.subtree_control",
             buf,
             sizeof(buf)) < 0){
    fprintf(stderr, "lsandbox: cannot read cgroup.subtree_control: %s\n",
        strerror(errno));
    return -1;
  }

  if(strstr(buf, "memory") == 0 ||
    strstr(buf, "pids") == 0 ||
    strstr(buf, "cpu") == 0){
    fprintf(stderr,
        "lsandbox: root cgroup controllers not enabled. Current: %s\n",
        buf);
    fprintf(stderr,
        "hint: run: sudo sh -c 'echo \"+memory +pids +cpu\" > /sys/fs/cgroup/cgroup.subtree_control'\n");
    return -1;
  }

  return 0;
}

static int
okcg(const char *path)
{
  const char *prefix = "/sys/fs/cgroup/lsandbox_";

  if(path == 0)
    return 0;

  return strncmp(path, prefix, strlen(prefix)) == 0;
}

static int
haspid(const sandbox_config_t *cfg)
{
  char path[LSANDBOX_PATH_MAX];
  char buf[64];

  if(cgpath(path, sizeof(path), cfg->cgroup_dir, "cgroup.procs") < 0){
    return 1;
  }

  if(cgr(path, buf, sizeof(buf)) < 0){
    return 0;
  }

  return 1;
}

static int
limmem(const sandbox_config_t *cfg)
{
  char path[LSANDBOX_PATH_MAX];
  char value[128];
  unsigned long long bytes;

  if(cfg->memory_limit[0] == '\0'){
    return 0;
  }

  if(sizeto(cfg->memory_limit, &bytes) < 0){
    fprintf(stderr, "lsandbox: invalid memory limit '%s'\n", cfg->memory_limit);
    return -1;
  }

  snprintf(value, sizeof(value), "%llu", bytes);

  if(cgpath(path, sizeof(path), cfg->cgroup_dir, "memory.max") < 0){
    return -1;
  }

  if(!lsandbox_path_exists(path)){
    fprintf(stderr, "lsandbox: '%s' does not exist\n", path);
    return -1;
  }

  return cgw(path, value);
}

static int
noswap(const sandbox_config_t *cfg)
{
  char path[LSANDBOX_PATH_MAX];

  if(cfg->memory_limit[0] == '\0'){
    return 0;
  }

  if(cgpath(path, sizeof(path), cfg->cgroup_dir, "memory.swap.max") < 0){
    return -1;
  }

  if(!lsandbox_path_exists(path)){
    fprintf(stderr, "lsandbox: warning: '%s' does not exist, skip swap limit\n", path);
    return 0;
  }

  return cgw(path, "0");
}

static int
oomgrp(const sandbox_config_t *cfg)
{
  char path[LSANDBOX_PATH_MAX];

  if(cfg->memory_limit[0] == '\0'){
    return 0;
  }

  if(cgpath(path, sizeof(path), cfg->cgroup_dir, "memory.oom.group") < 0){
    return -1;
  }

  if(!lsandbox_path_exists(path)){
    fprintf(stderr, "lsandbox: warning: '%s' does not exist, skip oom group\n", path);
    return 0;
  }

  return cgw(path, "1");
}

static int
limpids(const sandbox_config_t *cfg)
{
  char path[LSANDBOX_PATH_MAX];
  char value[64];

  if(cfg->pids_limit <= 0){
    return 0;
  }

  snprintf(value, sizeof(value), "%d", cfg->pids_limit);

  if(cgpath(path, sizeof(path), cfg->cgroup_dir, "pids.max") < 0){
    return -1;
  }

  if(!lsandbox_path_exists(path)){
    fprintf(stderr, "lsandbox: '%s' does not exist\n", path);
    return -1;
  }

  return cgw(path, value);
}

static int
limcpu(const sandbox_config_t *cfg)
{
  char path[LSANDBOX_PATH_MAX];
  char value[128];

  if(cfg->cpu_percent <= 0){
    return 0;
  }

  if(cfg->cpu_percent >= 100){
    snprintf(value, sizeof(value), "max 100000");
  } else {
    int period = 100000;
    int quota = period * cfg->cpu_percent / 100;
    snprintf(value, sizeof(value), "%d %d", quota, period);
  }

  if(cgpath(path, sizeof(path), cfg->cgroup_dir, "cpu.max") < 0){
    return -1;
  }

  if(!lsandbox_path_exists(path)){
    fprintf(stderr, "lsandbox: '%s' does not exist\n", path);
    return -1;
  }

  return cgw(path, value);
}

static int
putproc(const sandbox_config_t *cfg, pid_t pid)
{
  char path[LSANDBOX_PATH_MAX];
  char value[64];

  snprintf(value, sizeof(value), "%d", pid);

  if(cgpath(path, sizeof(path), cfg->cgroup_dir, "cgroup.procs") < 0){
    return -1;
  }

  return cgw(path, value);
}

int
lsandbox_cgroup_apply(const sandbox_config_t *cfg, pid_t pid)
{
  if(cfg == 0)
    return -1;

  if(!okcg(cfg->cgroup_dir)){
    fprintf(stderr, "lsandbox: unsafe cgroup path '%s'\n", cfg->cgroup_dir);
    return -1;
  }

  if(iscgv2() < 0){
    return -1;
  }

  if(rootctl() < 0){
    return -1;
  }

  if(lsandbox_path_exists(cfg->cgroup_dir)){
    if(rmdir(cfg->cgroup_dir) < 0){
      fprintf(stderr,
          "lsandbox: warning: old cgroup '%s' exists and cannot be removed: %s\n",
          cfg->cgroup_dir,
          strerror(errno));
    }
  }

  if(lsandbox_mkdir_p(cfg->cgroup_dir, 0755) < 0){
    return -1;
  }

  if(limmem(cfg) < 0){
    lsandbox_cgroup_cleanup(cfg);
    return -1;
  }

  if(noswap(cfg) < 0){
    lsandbox_cgroup_cleanup(cfg);
    return -1;
  }

  if(oomgrp(cfg) < 0){
    lsandbox_cgroup_cleanup(cfg);
    return -1;
  }

  if(limpids(cfg) < 0){
    lsandbox_cgroup_cleanup(cfg);
    return -1;
  }

  if(limcpu(cfg) < 0){
    lsandbox_cgroup_cleanup(cfg);
    return -1;
  }

  if(putproc(cfg, pid) < 0){
    lsandbox_cgroup_cleanup(cfg);
    return -1;
  }

  return 0;
}

int
lsandbox_cgroup_cleanup(const sandbox_config_t *cfg)
{
  if(cfg == 0)
    return -1;

  if(!okcg(cfg->cgroup_dir)){
    fprintf(stderr, "lsandbox: refuse to cleanup unsafe cgroup path '%s'\n",
        cfg->cgroup_dir);
    return -1;
  }

  if(!lsandbox_path_exists(cfg->cgroup_dir)){
    return 0;
  }

  if(cfg->keep_cgroup){
    return 0;
  }

  if(haspid(cfg)){
    fprintf(stderr,
        "lsandbox: warning: cgroup '%s' still has processes, skip cleanup\n",
        cfg->cgroup_dir);
    return -1;
  }

  if(rmdir(cfg->cgroup_dir) < 0){
    fprintf(stderr,
        "lsandbox: warning: cannot remove cgroup '%s': %s\n",
        cfg->cgroup_dir,
        strerror(errno));
    return -1;
  }

  return 0;
}
