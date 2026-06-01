#ifndef LSANDBOX_ROOTFS_H
#define LSANDBOX_ROOTFS_H

#include "sandbox.h"

int lsandbox_prepare_rootfs_dirs(sandbox_config_t *cfg);
int lsandbox_enter_rootfs(const sandbox_config_t *cfg);

#endif
