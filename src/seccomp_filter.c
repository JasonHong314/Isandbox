#define _GNU_SOURCE

#include <errno.h>
#include <seccomp.h>
#include <stdio.h>
#include <string.h>

#include "seccomp_filter.h"
#include "sandbox.h"

static int add_kill_rule(scmp_filter_ctx ctx, int syscall_nr, const char *name) {
    int rc = seccomp_rule_add(ctx, SCMP_ACT_KILL_PROCESS, syscall_nr, 0);

    if (rc < 0) {
        fprintf(stderr,
                "Error: failed to add seccomp rule for %s: %s\n",
                name,
                strerror(-rc));
        return -1;
    }

    return 0;
}

static int add_basic_rules(scmp_filter_ctx ctx) {
    if (add_kill_rule(ctx, SCMP_SYS(mount), "mount") < 0) return -1;
    if (add_kill_rule(ctx, SCMP_SYS(ptrace), "ptrace") < 0) return -1;
    if (add_kill_rule(ctx, SCMP_SYS(reboot), "reboot") < 0) return -1;

#ifdef __NR_kexec_load
    if (add_kill_rule(ctx, SCMP_SYS(kexec_load), "kexec_load") < 0) return -1;
#endif

#ifdef __NR_init_module
    if (add_kill_rule(ctx, SCMP_SYS(init_module), "init_module") < 0) return -1;
#endif

#ifdef __NR_finit_module
    if (add_kill_rule(ctx, SCMP_SYS(finit_module), "finit_module") < 0) return -1;
#endif

#ifdef __NR_delete_module
    if (add_kill_rule(ctx, SCMP_SYS(delete_module), "delete_module") < 0) return -1;
#endif

#ifdef __NR_bpf
    if (add_kill_rule(ctx, SCMP_SYS(bpf), "bpf") < 0) return -1;
#endif

#ifdef __NR_perf_event_open
    if (add_kill_rule(ctx, SCMP_SYS(perf_event_open), "perf_event_open") < 0) return -1;
#endif

#ifdef __NR_swapon
    if (add_kill_rule(ctx, SCMP_SYS(swapon), "swapon") < 0) return -1;
#endif

#ifdef __NR_swapoff
    if (add_kill_rule(ctx, SCMP_SYS(swapoff), "swapoff") < 0) return -1;
#endif

    return 0;
}

static int add_strict_rules(scmp_filter_ctx ctx) {
#ifdef __NR_unshare
    if (add_kill_rule(ctx, SCMP_SYS(unshare), "unshare") < 0) return -1;
#endif

#ifdef __NR_setns
    if (add_kill_rule(ctx, SCMP_SYS(setns), "setns") < 0) return -1;
#endif

#ifdef __NR_pivot_root
    if (add_kill_rule(ctx, SCMP_SYS(pivot_root), "pivot_root") < 0) return -1;
#endif

#ifdef __NR_open_by_handle_at
    if (add_kill_rule(ctx, SCMP_SYS(open_by_handle_at), "open_by_handle_at") < 0) return -1;
#endif

#ifdef __NR_keyctl
    if (add_kill_rule(ctx, SCMP_SYS(keyctl), "keyctl") < 0) return -1;
#endif

#ifdef __NR_add_key
    if (add_kill_rule(ctx, SCMP_SYS(add_key), "add_key") < 0) return -1;
#endif

#ifdef __NR_request_key
    if (add_kill_rule(ctx, SCMP_SYS(request_key), "request_key") < 0) return -1;
#endif

#ifdef __NR_move_mount
    if (add_kill_rule(ctx, SCMP_SYS(move_mount), "move_mount") < 0) return -1;
#endif

#ifdef __NR_fsopen
    if (add_kill_rule(ctx, SCMP_SYS(fsopen), "fsopen") < 0) return -1;
#endif

#ifdef __NR_fsconfig
    if (add_kill_rule(ctx, SCMP_SYS(fsconfig), "fsconfig") < 0) return -1;
#endif

#ifdef __NR_fsmount
    if (add_kill_rule(ctx, SCMP_SYS(fsmount), "fsmount") < 0) return -1;
#endif

#ifdef __NR_fspick
    if (add_kill_rule(ctx, SCMP_SYS(fspick), "fspick") < 0) return -1;
#endif

#ifdef __NR_iopl
    if (add_kill_rule(ctx, SCMP_SYS(iopl), "iopl") < 0) return -1;
#endif

#ifdef __NR_ioperm
    if (add_kill_rule(ctx, SCMP_SYS(ioperm), "ioperm") < 0) return -1;
#endif

#ifdef __NR_acct
    if (add_kill_rule(ctx, SCMP_SYS(acct), "acct") < 0) return -1;
#endif

#ifdef __NR_quotactl
    if (add_kill_rule(ctx, SCMP_SYS(quotactl), "quotactl") < 0) return -1;
#endif

    return 0;
}

int lsandbox_install_seccomp_filter(lsandbox_seccomp_mode_t mode) {
    if (mode == LSANDBOX_SECCOMP_OFF) {
        return 0;
    }

    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);

    if (ctx == NULL) {
        fprintf(stderr, "Error: seccomp_init failed\n");
        return -1;
    }

    if (add_basic_rules(ctx) < 0) {
        goto fail;
    }

    if (mode == LSANDBOX_SECCOMP_STRICT) {
        if (add_strict_rules(ctx) < 0) {
            goto fail;
        }
    }

    int rc = seccomp_load(ctx);
    if (rc < 0) {
        fprintf(stderr, "Error: seccomp_load failed: %s\n", strerror(-rc));
        goto fail;
    }

    seccomp_release(ctx);
    return 0;

fail:
    seccomp_release(ctx);
    return -1;
}