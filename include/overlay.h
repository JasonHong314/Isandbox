#ifndef LSANDBOX_OVERLAY_H
#define LSANDBOX_OVERLAY_H

#include "sandbox.h"

int lsandbox_prepare_overlay_dirs(sandbox_config_t *cfg);
int lsandbox_mount_tmp_overlay(const sandbox_config_t *cfg);

int lsandbox_prepare_workdir_overlay_dirs(sandbox_config_t *cfg);
int lsandbox_mount_workdir_overlay(const sandbox_config_t *cfg);

int lsandbox_cleanup_overlay_dirs(const sandbox_config_t *cfg);

#endif