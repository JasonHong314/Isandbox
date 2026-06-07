#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include "rootfs.h"
#include "sandbox.h"
#include "utils.h"

/*
 * Root overlay setup.  Most strange-looking bind mounts are from actual
 * tests: DNS broke without resolv.conf, venv/git broke when /dev/null was
 * wrong, and GUI apps need more of /run than simple shell commands.
 */

#define ROOTFS_OPT_MAX 8192

static int
mkpath(char *buf, size_t size, const char *a, const char *b)
{
  int n = snprintf(buf, size, "%s/%s", a, b);
  if(n < 0 || (size_t)n >= size){
    fprintf(stderr, "lsandbox: path too long: %s/%s\n", a, b);
    return -1;
  }
  return 0;
}

static int
mkdirat1(const char *root, const char *name, mode_t mode)
{
  char path[LSANDBOX_PATH_MAX];

  if(mkpath(path, sizeof(path), root, name) < 0){
    return -1;
  }

  if(lsandbox_mkdir_p(path, mode) < 0){
    return -1;
  }

  chmod(path, mode);
  return 0;
}

int
lsandbox_prepare_rootfs_dirs(sandbox_config_t *cfg)
{
  if(cfg == 0)
    return -1;

  if(lsandbox_mkdir_p("sandboxes", 0755) < 0){
    return -1;
  }

  if(lsandbox_mkdir_p(cfg->sandbox_dir, 0755) < 0){
    return -1;
  }

  if(lsandbox_mkdir_p(cfg->upper_root_dir, 0755) < 0){
    return -1;
  }

  if(lsandbox_mkdir_p(cfg->work_root_dir, 0755) < 0){
    return -1;
  }

  if(lsandbox_mkdir_p(cfg->merged_root_dir, 0755) < 0){
    return -1;
  }

  return 0;
}

static int
ovroot(const sandbox_config_t *cfg)
{
  char options[ROOTFS_OPT_MAX];
  int n;

  n = snprintf(options,
         sizeof(options),
         "lowerdir=/,upperdir=%s,workdir=%s",
         cfg->upper_root_dir,
         cfg->work_root_dir);

  if(n < 0 || (size_t)n >= sizeof(options)){
    fprintf(stderr, "lsandbox: root overlay options too long\n");
    return -1;
  }

  if(mount("overlay",
        cfg->merged_root_dir,
        "overlay",
        0,
        options) < 0){
    fprintf(stderr, "lsandbox: mount root overlay failed: %s\n", strerror(errno));
    fprintf(stderr, "Overlay options: %s\n", options);
    return -1;
  }

  return 0;
}

static int
mkdev(const char *dev_dir,
                const char *name,
                int major_no,
                int minor_no,
                mode_t mode)
{
  char path[LSANDBOX_PATH_MAX];

  if(mkpath(path, sizeof(path), dev_dir, name) < 0){
    return -1;
  }

  unlink(path);

  if(mknod(path, S_IFCHR | mode, makedev(major_no, minor_no)) < 0){
    fprintf(stderr, "lsandbox: mknod %s failed: %s\n",
        path, strerror(errno));
    return -1;
  }

  if(chmod(path, mode) < 0){
    fprintf(stderr, "lsandbox: chmod %s failed: %s\n",
        path, strerror(errno));
    return -1;
  }

  return 0;
}

static int
symlinkf(const char *target, const char *link_path)
{
  unlink(link_path);

  if(symlink(target, link_path) < 0){
    fprintf(stderr, "lsandbox: warning: symlink %s -> %s failed: %s\n",
        link_path, target, strerror(errno));
    return -1;
  }

  return 0;
}

static int
setup_private_dev(const char *root)
{
  char dev_dir[LSANDBOX_PATH_MAX];
  char path[LSANDBOX_PATH_MAX];

  if(mkpath(dev_dir, sizeof(dev_dir), root, "dev") < 0){
    return -1;
  }

  if(lsandbox_mkdir_p(dev_dir, 0755) < 0){
    return -1;
  }

  if(mount("tmpfs", dev_dir, "tmpfs",
        MS_NOSUID | MS_NOEXEC,
        "mode=1777") < 0){
    fprintf(stderr, "lsandbox: mount private /dev tmpfs failed: %s\n",
        strerror(errno));
    return -1;
  }

  if(chmod(dev_dir, 01777) < 0){
    fprintf(stderr, "lsandbox: chmod private /dev failed: %s\n",
        strerror(errno));
    return -1;
  }

  if(mkdev(dev_dir, "null", 1, 3, 0666) < 0){
    return -1;
  }

  if(mkdev(dev_dir, "zero", 1, 5, 0666) < 0){
    return -1;
  }

  if(mkdev(dev_dir, "full", 1, 7, 0666) < 0){
    return -1;
  }

  if(mkdev(dev_dir, "random", 1, 8, 0666) < 0){
    return -1;
  }

  if(mkdev(dev_dir, "urandom", 1, 9, 0666) < 0){
    return -1;
  }

  if(mkdev(dev_dir, "tty", 5, 0, 0666) < 0){
    return -1;
  }

  if(mkpath(path, sizeof(path), dev_dir, "fd") == 0){
    symlinkf("/proc/self/fd", path);
  }

  if(mkpath(path, sizeof(path), dev_dir, "stdin") == 0){
    symlinkf("/proc/self/fd/0", path);
  }

  if(mkpath(path, sizeof(path), dev_dir, "stdout") == 0){
    symlinkf("/proc/self/fd/1", path);
  }

  if(mkpath(path, sizeof(path), dev_dir, "stderr") == 0){
    symlinkf("/proc/self/fd/2", path);
  }

  if(mkpath(path, sizeof(path), dev_dir, "shm") < 0){
    return -1;
  }

  if(lsandbox_mkdir_p(path, 01777) < 0){
    return -1;
  }

  if(mount("tmpfs", path, "tmpfs",
        MS_NOSUID | MS_NODEV,
        "mode=1777") < 0){
    fprintf(stderr, "lsandbox: warning: mount /dev/shm failed: %s\n",
        strerror(errno));
  } else {
    chmod(path, 01777);
  }

  if(mkpath(path, sizeof(path), dev_dir, "pts") < 0){
    return -1;
  }

  if(lsandbox_mkdir_p(path, 0755) < 0){
    return -1;
  }

  if(mount("devpts", path, "devpts",
        MS_NOSUID | MS_NOEXEC,
        "newinstance,ptmxmode=0666,mode=0620") < 0){
    fprintf(stderr, "lsandbox: warning: mount /dev/pts failed: %s\n",
        strerror(errno));
  }

  if(mkpath(path, sizeof(path), dev_dir, "ptmx") == 0){
    symlinkf("pts/ptmx", path);
  }

  return 0;
}

static int
copy1(const char *src, const char *dst)
{
  FILE *in = fopen(src, "r");
  FILE *out;
  char buf[4096];
  size_t n;

  if(in == 0){
    return -1;
  }

  unlink(dst);

  out = fopen(dst, "w");
  if(out == 0){
    fclose(in);
    return -1;
  }

  while((n = fread(buf, 1, sizeof(buf), in)) > 0){
    if(fwrite(buf, 1, n, out) != n){
      fclose(in);
      fclose(out);
      return -1;
    }
  }

  fclose(in);
  fclose(out);
  return 0;
}

static int
dns1(const sandbox_config_t *cfg)
{
  char etc_dir[LSANDBOX_PATH_MAX];
  char resolv_path[LSANDBOX_PATH_MAX];
  int n;

  if(cfg == 0)
    return -1;

  n = snprintf(etc_dir, sizeof(etc_dir), "%s/etc", cfg->merged_root_dir);
  if(n < 0 || (size_t)n >= sizeof(etc_dir)){
    fprintf(stderr, "lsandbox: /etc path too long\n");
    return -1;
  }

  if(lsandbox_mkdir_p(etc_dir, 0755) < 0){
    return -1;
  }

  n = snprintf(resolv_path,
         sizeof(resolv_path),
         "%s/etc/resolv.conf",
         cfg->merged_root_dir);
  if(n < 0 || (size_t)n >= sizeof(resolv_path)){
    fprintf(stderr, "lsandbox: resolv.conf path too long\n");
    return -1;
  }

  if(copy1("/etc/resolv.conf", resolv_path) == 0){
    chmod(resolv_path, 0644);
    return 0;
  }

  unlink(resolv_path);

  FILE *fp = fopen(resolv_path, "w");
  if(fp == 0){
    fprintf(stderr, "lsandbox: create sandbox resolv.conf failed: %s\n",
        strerror(errno));
    return -1;
  }

  fprintf(fp, "nameserver 1.1.1.1\n");
  fprintf(fp, "nameserver 8.8.8.8\n");

  fclose(fp);
  chmod(resolv_path, 0644);

  fprintf(stderr,
      "lsandbox: warning: host /etc/resolv.conf not available, using fallback DNS\n");

  return 0;
}

static int
bindsys(const sandbox_config_t *cfg)
{
  char path[LSANDBOX_PATH_MAX];

  if(mkdirat1(cfg->merged_root_dir, "proc", 0555) < 0){
    return -1;
  }

  if(mkpath(path, sizeof(path), cfg->merged_root_dir, "proc") < 0){
    return -1;
  }

  if(mount("proc", path, "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, 0) < 0){
    fprintf(stderr, "lsandbox: mount proc in rootfs failed: %s\n", strerror(errno));
    return -1;
  }

  if(setup_private_dev(cfg->merged_root_dir) < 0){
    return -1;
  }

  if(mkdirat1(cfg->merged_root_dir, "run", 0755) < 0){
    return -1;
  }

  if(mkpath(path, sizeof(path), cfg->merged_root_dir, "run") < 0){
    return -1;
  }

  if(mount("tmpfs", path, "tmpfs", MS_NOSUID | MS_NODEV, "mode=755") < 0){
    fprintf(stderr, "lsandbox: warning: mount tmpfs /run failed: %s\n", strerror(errno));
  }

  if(dns1(cfg) < 0){
    fprintf(stderr, "lsandbox: warning: setup resolv.conf failed\n");
  }

  return 0;
}

int
lsandbox_enter_rootfs(const sandbox_config_t *cfg)
{
  if(cfg == 0)
    return -1;

  if(ovroot(cfg) < 0){
    return -1;
  }

  if(bindsys(cfg) < 0){
    return -1;
  }

  if(chroot(cfg->merged_root_dir) < 0){
    fprintf(stderr, "lsandbox: chroot failed: %s\n", strerror(errno));
    return -1;
  }

  if(chdir(cfg->target_work_dir) < 0){
    if(chdir("/") < 0){
      fprintf(stderr, "lsandbox: chdir / failed: %s\n", strerror(errno));
      return -1;
    }
  }

  return 0;
}
