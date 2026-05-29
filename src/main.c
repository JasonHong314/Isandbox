#include <stdio.h>
#include <string.h>

#include "sandbox.h"

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s run -- <command> [args...]\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s run -- /bin/echo hello\n", prog);
    fprintf(stderr, "  sudo %s run -- bash\n", prog);
}

static int find_command_index(int argc, char *argv[]) {
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            return i + 1;
        }
    }

    return -1;
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

    int cmd_index = find_command_index(argc, argv);
    if (cmd_index == -1 || cmd_index >= argc) {
        fprintf(stderr, "Error: missing command after '--'\n");
        print_usage(argv[0]);
        return 1;
    }

    sandbox_config_t cfg;
    sandbox_config_init(&cfg);

    cfg.cmd_argv = &argv[cmd_index];

    return sandbox_run(&cfg);
}