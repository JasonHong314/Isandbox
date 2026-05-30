#ifndef LSANDBOX_CGROUP_H
#define LSANDBOX_CGROUP_H

#include <sys/types.h>

#include "sandbox.h"

int lsandbox_cgroup_apply(const sandbox_config_t *cfg, pid_t pid);
int lsandbox_cgroup_cleanup(const sandbox_config_t *cfg);

#endif