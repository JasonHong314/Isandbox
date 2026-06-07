#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "manage.h"
#include "sandbox.h"
#include "utils.h"

#define BOXD "sandboxes"
#define CGD  "/sys/fs/cgroup"

/*
 * manage.c is not in the sandbox hot path.  It is only the small
 * command line part I used while checking boxes left by earlier runs.
 * The real isolation work is in namespace/rootfs/cgroup/seccomp.
 */

static int
badbox(const char *s)
{
  char c;

  if(s == 0 || *s == 0)
    return 1;

  while((c = *s++) != 0){
    if(c >= 'a' && c <= 'z')
      continue;
    if(c >= 'A' && c <= 'Z')
      continue;
    if(c >= '0' && c <= '9')
      continue;
    if(c == '_' || c == '-')
      continue;
    return 1;
  }
  return 0;
}

static int
path(char *b, int n, const char *a, const char *c)
{
  int r;

  r = snprintf(b, n, "%s/%s", a, c);
  if(r < 0 || r >= n){
    fprintf(stderr, "lsandbox: name is too long: %s/%s\n", a, c);
    return -1;
  }
  return 0;
}

static int
boxpath(char *b, int n, const char *name)
{
  if(badbox(name)){
    fprintf(stderr, "lsandbox: bad box name %s\n", name ? name : "(nil)");
    fprintf(stderr, "lsandbox: use letters, digits, '_' or '-'\n");
    return -1;
  }
  return path(b, n, BOXD, name);
}

static int
dirp(const char *p)
{
  struct stat st;

  if(stat(p, &st) < 0)
    return 0;
  return S_ISDIR(st.st_mode);
}

/* Count only for inspect.  Errors are ignored; sandboxes may contain
 * root-owned overlay work dirs after an interrupted sudo run. */
static unsigned long long
nentry(const char *p)
{
  DIR *d;
  struct dirent *e;
  unsigned long long n;

  d = opendir(p);
  if(d == 0)
    return 0;

  n = 0;
  while((e = readdir(d)) != 0){
    char q[LSANDBOX_PATH_MAX];
    struct stat st;

    if(strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
      continue;
    if(path(q, sizeof q, p, e->d_name) < 0)
      continue;
    if(lstat(q, &st) < 0)
      continue;

    n++;
    if(S_ISDIR(st.st_mode))
      n += nentry(q);
  }
  closedir(d);
  return n;
}

static void
showdir(const char *tag, const char *p)
{
  printf("  %-11s %s  %s\n", tag, dirp(p) ? "ok " : "-- ", p);
}

int
lsandbox_manage_list(void)
{
  DIR *d;
  struct dirent *e;
  int n;

  if(!lsandbox_path_exists(BOXD)){
    printf("no sandboxes yet (%s not created)\n", BOXD);
    return 0;
  }

  d = opendir(BOXD);
  if(d == 0){
    fprintf(stderr, "lsandbox: cannot read %s: %s\n", BOXD, strerror(errno));
    return 1;
  }

  n = 0;
  while((e = readdir(d)) != 0){
    char p[LSANDBOX_PATH_MAX];

    if(e->d_name[0] == '.')
      continue;
    if(path(p, sizeof p, BOXD, e->d_name) < 0)
      continue;
    if(!dirp(p))
      continue;

    if(n++ == 0)
      printf("boxes under %s:\n", BOXD);
    printf("  %s\n", e->d_name);
  }

  closedir(d);
  if(n == 0)
    printf("boxes under %s: none\n", BOXD);
  return 0;
}

int
lsandbox_manage_inspect(const char *name)
{
  char b[LSANDBOX_PATH_MAX];
  char up[LSANDBOX_PATH_MAX], wk[LSANDBOX_PATH_MAX], mg[LSANDBOX_PATH_MAX];
  char rup[LSANDBOX_PATH_MAX], rwk[LSANDBOX_PATH_MAX], rmg[LSANDBOX_PATH_MAX];
  char cg[LSANDBOX_PATH_MAX];

  if(boxpath(b, sizeof b, name) < 0)
    return 1;

  if(path(up,  sizeof up,  b, "upper_tmp") < 0) return 1;
  if(path(wk,  sizeof wk,  b, "work_tmp") < 0) return 1;
  if(path(mg,  sizeof mg,  b, "merged_tmp") < 0) return 1;
  if(path(rup, sizeof rup, b, "upper_root") < 0) return 1;
  if(path(rwk, sizeof rwk, b, "work_root") < 0) return 1;
  if(path(rmg, sizeof rmg, b, "merged_root") < 0) return 1;

  if(snprintf(cg, sizeof cg, "%s/lsandbox_%s", CGD, name) >= (int)sizeof cg){
    fprintf(stderr, "lsandbox: cgroup name too long\n");
    return 1;
  }

  printf("box %s\n", name);
  printf("  dir         %s  %s\n", dirp(b) ? "ok " : "-- ", b);
  printf("\ntmp overlay\n");
  showdir("upper", up);
  showdir("work", wk);
  showdir("merged", mg);
  printf("\nroot overlay\n");
  showdir("upper", rup);
  showdir("work", rwk);
  showdir("merged", rmg);

  if(dirp(up))
    printf("\nupper_tmp file count: %llu\n", nentry(up));
  if(dirp(rup))
    printf("upper_root file count: %llu\n", nentry(rup));

  printf("cgroup: %s  %s\n", dirp(cg) ? "ok " : "-- ", cg);
  return 0;
}

int
lsandbox_manage_clean(const char *name)
{
  char b[LSANDBOX_PATH_MAX];
  char p[3][LSANDBOX_PATH_MAX];
  int i;

  if(boxpath(b, sizeof b, name) < 0)
    return 1;
  if(!dirp(b)){
    fprintf(stderr, "lsandbox: no such box: %s\n", name);
    return 1;
  }

  if(path(p[0], sizeof p[0], b, "upper_tmp") < 0) return 1;
  if(path(p[1], sizeof p[1], b, "work_tmp") < 0) return 1;
  if(path(p[2], sizeof p[2], b, "merged_tmp") < 0) return 1;

  for(i = 0; i < 3; i++)
    lsandbox_remove_recursive(p[i]);
  if(lsandbox_mkdir_p(p[0], 0755) < 0) return 1;
  if(lsandbox_mkdir_p(p[1], 0755) < 0) return 1;
  if(lsandbox_mkdir_p(p[2], 0755) < 0) return 1;

  printf("cleaned %s tmp overlay\n", name);
  return 0;
}

int
lsandbox_manage_delete(const char *name)
{
  char b[LSANDBOX_PATH_MAX];

  if(boxpath(b, sizeof b, name) < 0)
    return 1;
  if(!dirp(b)){
    fprintf(stderr, "lsandbox: no such box: %s\n", name);
    return 1;
  }
  if(lsandbox_remove_recursive(b) < 0)
    return 1;

  printf("removed %s\n", name);
  return 0;
}

int
lsandbox_manage_clean_cgroups(void)
{
  DIR *d;
  struct dirent *e;
  int n;

  d = opendir(CGD);
  if(d == 0){
    fprintf(stderr, "lsandbox: cannot read %s: %s\n", CGD, strerror(errno));
    return 1;
  }

  n = 0;
  while((e = readdir(d)) != 0){
    char p[LSANDBOX_PATH_MAX];

    if(strncmp(e->d_name, "lsandbox_", 9) != 0)
      continue;
    if(path(p, sizeof p, CGD, e->d_name) < 0)
      continue;

    if(rmdir(p) == 0){
      printf("removed cgroup %s\n", p);
      n++;
    } else {
      fprintf(stderr, "lsandbox: left %s: %s\n", p, strerror(errno));
    }
  }

  closedir(d);
  if(n == 0)
    printf("no empty lsandbox cgroup found\n");
  return 0;
}
