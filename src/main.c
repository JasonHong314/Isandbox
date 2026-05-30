#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "manage.h"
#include "sandbox.h"

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s run [options] -- <command> [args...]\n", prog);
    fprintf(stderr, "  %s list\n", prog);
    fprintf(stderr, "  %s inspect <name>\n", prog);
    fprintf(stderr, "  %s logs\n", prog);
    fprintf(stderr, "  sudo %s clean <name>\n", prog);
    fprintf(stderr, "  sudo %s delete <name>\n", prog);
    fprintf(stderr, "  sudo %s clean-cgroups\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Run options:\n");
    fprintf(stderr, "  --name <box>       Sandbox name\n");
    fprintf(stderr, "  --rm               Remove sandbox files after exit\n");
    fprintf(stderr, "  --seccomp <mode>   Seccomp mode: off, basic, strict\n");
    fprintf(stderr, "  --mem <limit>      Memory limit, example: 64M, 512M, 1G\n");
    fprintf(stderr, "  --pids <num>       Max process count\n");
    fprintf(stderr, "  --cpu <percent>    CPU limit percent, example: 50\n");
    fprintf(stderr, "  --keep-cgroup      Keep cgroup directory after exit for debugging\n");
    fprintf(stderr, "  --overlay-workdir  Overlay current working directory\n");
    fprintf(stderr, "  --net <mode>       Network mode: host, off. Default: host\n");
    fprintf(stderr, "  --no-drop-privs    Do not drop root privileges before exec\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Defaults:\n");
    fprintf(stderr, "  --net host, --mem 1G, --pids 128, --cpu 100, --seccomp basic\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s run -- /bin/echo hello\n", prog);
    fprintf(stderr, "  sudo %s run --name box1 -- bash\n", prog);
    fprintf(stderr, "  sudo %s run --name temp1 --rm -- bash\n", prog);
    fprintf(stderr, "  sudo %s run --name limitbox --mem 64M --pids 32 --cpu 50 -- bash\n", prog);
    fprintf(stderr, "  sudo %s run --name secbox --seccomp basic -- bash\n", prog);
    fprintf(stderr, "  %s list\n", prog);
    fprintf(stderr, "  %s inspect box1\n", prog);
    fprintf(stderr, "  sudo %s clean box1\n", prog);
    fprintf(stderr, "  sudo %s delete box1\n", prog);
}

static int find_command_index(int argc, char *argv[]) {
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            return i + 1;
        }
    }

    return -1;
}

static int parse_seccomp_mode(const char *text, lsandbox_seccomp_mode_t *mode) {
    if (text == NULL || mode == NULL) {
        return -1;
    }

    if (strcmp(text, "off") == 0) {
        *mode = LSANDBOX_SECCOMP_OFF;
        return 0;
    }

    if (strcmp(text, "basic") == 0) {
        *mode = LSANDBOX_SECCOMP_BASIC;
        return 0;
    }

    if (strcmp(text, "strict") == 0) {
        *mode = LSANDBOX_SECCOMP_STRICT;
        return 0;
    }

    return -1;
}

static int parse_run_options(int argc, char *argv[], sandbox_config_t *cfg) {
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            return 0;
        }

        if (strcmp(argv[i], "--name") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --name requires an argument\n");
                return -1;
            }

            snprintf(cfg->name, sizeof(cfg->name), "%s", argv[i + 1]);
            i++;
            continue;
        }

        if (strcmp(argv[i], "--rm") == 0) {
            cfg->remove_after_exit = 1;
            continue;
        }

        if (strcmp(argv[i], "--net") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --net requires a mode: host, off\n");
                return -1;
            }

            if (strcmp(argv[i + 1], "host") == 0) {
                cfg->enable_net = 1;
            } else if (strcmp(argv[i + 1], "off") == 0) {
                cfg->enable_net = 0;
            } else {
                fprintf(stderr, "Error: invalid net mode '%s'\n", argv[i + 1]);
                fprintf(stderr, "Valid modes: host, off\n");
                return -1;
            }

            i++;
            continue;
        }

        if (strcmp(argv[i], "--overlay-workdir") == 0) {
            cfg->enable_workdir_overlay = 1;
            continue;
        }

        if (strcmp(argv[i], "--no-drop-privs") == 0) {
            cfg->enable_drop_privs = 0;
            continue;
        }

        if (strcmp(argv[i], "--seccomp") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --seccomp requires a mode: off, basic, strict\n");
                return -1;
            }

            if (parse_seccomp_mode(argv[i + 1], &cfg->seccomp_mode) < 0) {
                fprintf(stderr, "Error: invalid seccomp mode '%s'\n", argv[i + 1]);
                fprintf(stderr, "Valid modes: off, basic, strict\n");
                return -1;
            }

            i++;
            continue;
        }

        if (strcmp(argv[i], "--keep-cgroup") == 0) {
            cfg->keep_cgroup = 1;
            continue;
        }

        if (strcmp(argv[i], "--mem") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --mem requires an argument\n");
                return -1;
            }

            snprintf(cfg->memory_limit, sizeof(cfg->memory_limit), "%s", argv[i + 1]);
            cfg->enable_cgroup = 1;
            i++;
            continue;
        }

        if (strcmp(argv[i], "--pids") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --pids requires an argument\n");
                return -1;
            }

            cfg->pids_limit = atoi(argv[i + 1]);
            if (cfg->pids_limit <= 0) {
                fprintf(stderr, "Error: invalid --pids value\n");
                return -1;
            }

            cfg->enable_cgroup = 1;
            i++;
            continue;
        }

        if (strcmp(argv[i], "--cpu") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --cpu requires an argument\n");
                return -1;
            }

            cfg->cpu_percent = atoi(argv[i + 1]);
            if (cfg->cpu_percent <= 0 || cfg->cpu_percent > 100) {
                fprintf(stderr, "Error: --cpu must be between 1 and 100\n");
                return -1;
            }

            cfg->enable_cgroup = 1;
            i++;
            continue;
        }

        fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
        return -1;
    }

    return 0;
}

static int handle_run(int argc, char *argv[]) {
    sandbox_config_t cfg;
    int cmd_index;

    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    sandbox_config_init(&cfg);

    if (parse_run_options(argc, argv, &cfg) < 0) {
        print_usage(argv[0]);
        return 1;
    }

    cmd_index = find_command_index(argc, argv);
    if (cmd_index == -1 || cmd_index >= argc) {
        fprintf(stderr, "Error: missing command after '--'\n");
        print_usage(argv[0]);
        return 1;
    }

    cfg.cmd_argv = &argv[cmd_index];

    return sandbox_run(&cfg);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "run") == 0) {
        return handle_run(argc, argv);
    }

    if (strcmp(argv[1], "list") == 0) {
        if (argc != 2) {
            print_usage(argv[0]);
            return 1;
        }

        return lsandbox_manage_list();
    }

    if (strcmp(argv[1], "inspect") == 0) {
        if (argc != 3) {
            print_usage(argv[0]);
            return 1;
        }

        return lsandbox_manage_inspect(argv[2]);
    }

    if (strcmp(argv[1], "logs") == 0) {
        if (argc != 2) {
            print_usage(argv[0]);
            return 1;
        }

        return lsandbox_log_show();
    }

    if (strcmp(argv[1], "clean") == 0) {
        if (argc != 3) {
            print_usage(argv[0]);
            return 1;
        }

        return lsandbox_manage_clean(argv[2]);
    }

    if (strcmp(argv[1], "delete") == 0) {
        if (argc != 3) {
            print_usage(argv[0]);
            return 1;
        }

        return lsandbox_manage_delete(argv[2]);
    }

    if (strcmp(argv[1], "clean-cgroups") == 0) {
        if (argc != 2) {
            print_usage(argv[0]);
            return 1;
        }

        return lsandbox_manage_clean_cgroups();
    }

    fprintf(stderr, "Error: unsupported command '%s'\n", argv[1]);
    print_usage(argv[0]);
    return 1;
}