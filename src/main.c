#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sandbox.h"

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s run [options] -- <command> [args...]\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --name <box>       Sandbox name\n");
    fprintf(stderr, "  --rm               Remove sandbox files after exit\n");
    fprintf(stderr, "  --mem <limit>      Memory limit, example: 64M, 512M, 1G\n");
    fprintf(stderr, "  --pids <num>       Max process count\n");
    fprintf(stderr, "  --cpu <percent>    CPU limit percent, example: 50\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s run -- /bin/echo hello\n", prog);
    fprintf(stderr, "  sudo %s run --name box1 -- bash\n", prog);
    fprintf(stderr, "  sudo %s run --name temp1 --rm -- bash\n", prog);
}

static int find_command_index(int argc, char *argv[]) {
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            return i + 1;
        }
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

int main(int argc, char *argv[]) {
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "run") != 0) {
        fprintf(stderr, "Error: unsupported command '%s'\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }

    sandbox_config_t cfg;
    sandbox_config_init(&cfg);

    if (parse_run_options(argc, argv, &cfg) < 0) {
        print_usage(argv[0]);
        return 1;
    }

    int cmd_index = find_command_index(argc, argv);
    if (cmd_index == -1 || cmd_index >= argc) {
        fprintf(stderr, "Error: missing command after '--'\n");
        print_usage(argv[0]);
        return 1;
    }

    cfg.cmd_argv = &argv[cmd_index];

    return sandbox_run(&cfg);
}
