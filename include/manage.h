#ifndef LSANDBOX_MANAGE_H
#define LSANDBOX_MANAGE_H

int lsandbox_manage_list(void);
int lsandbox_manage_inspect(const char *name);
int lsandbox_manage_clean(const char *name);
int lsandbox_manage_delete(const char *name);
int lsandbox_manage_clean_cgroups(void);

#endif