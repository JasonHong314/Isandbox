#ifndef LSANDBOX_LOG_H
#define LSANDBOX_LOG_H

#include <sys/types.h>

#include "sandbox.h"

void lsandbox_log_start(const sandbox_config_t *cfg, pid_t pid);
void lsandbox_log_exit(const sandbox_config_t *cfg, pid_t pid, int exit_code);
void lsandbox_log_signal(const sandbox_config_t *cfg, pid_t pid, int signal_no);
int lsandbox_log_show(void);

#endif