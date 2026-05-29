#ifndef LSANDBOX_UTILS_H
#define LSANDBOX_UTILS_H

#include <sys/types.h>

int lsandbox_path_exists(const char *path);
int lsandbox_mkdir_if_not_exists(const char *path, mode_t mode);
int lsandbox_mkdir_p(const char *path, mode_t mode);
int lsandbox_remove_recursive(const char *path);

#endif
