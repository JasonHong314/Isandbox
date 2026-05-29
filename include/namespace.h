#ifndef LSANDBOX_NAMESPACE_H
#define LSANDBOX_NAMESPACE_H

#include "sandbox.h"

#define LSANDBOX_STACK_SIZE (1024 * 1024)

int lsandbox_clone_run(sandbox_config_t *cfg);

#endif