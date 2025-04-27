/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "cmd_build.h"
#include "cmd_clean.h"

// Function to print usage instructions
void print_usage(const char *program_name) {
    printf("Usage: %s <command>\n", program_name);
    printf("Commands:\n");
    printf("  build [--release|debug] [--target=TARGET] [project_root|source_file]\n");
    printf("  clean\n");
}

int dispatch_build(int argc, char* argv[]) {
    struct cmdopts cmdopts = {
        .jlevel = 1,
        .debug = true,
    };
    
    static struct option long_options[] = {
        {"release", no_argument, 0, 'r'},
        {"target", required_argument, 0, 't'},
        {"jlevel", required_argument, 0, 'j'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "rgt:j:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'r':
                cmdopts.release = true;
                cmdopts.debug = false;
                break;
            case 't':
                cmdopts.targets = optarg;
                break;
            case 'j':
                cmdopts.jlevel = strtol(optarg, NULL, 10);
                if (cmdopts.jlevel < 1) {
                    printf("invalid jlevel: must be >= 1\n");
                    exit(1);
                }
                break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    bool valid_optind = (optind == argc) || (optind+1 == argc);
    if (!valid_optind) {
        printf("error: too many arguments\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    // assume rootdir is cwd if not specified
    cmdopts.rootdir = (optind == argc) ? "." : argv[optind];
    return cc_build(&cmdopts);
}

int dispatch_clean(int argc, char* argv[]) {
    bool valid_optind = (optind == argc) || (optind+1 == argc);
    if (!valid_optind) {
        printf("error: too many arguments\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    struct cmdopts cmdopts = {
        // assume rootdir is cwd if not specified
        .rootdir = (optind == argc) ? "." : argv[optind],
    };
    return cc_clean(&cmdopts);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    const char *command = argv[1];

    if (strcmp(command, "build") == 0) {
        return dispatch_build(argc-1, argv+1);

    } else if (strcmp(command, "clean") == 0) {
        return dispatch_clean(argc-1, argv+1);

    } else {
        printf("Unknown command: %s\n", command);
        return EXIT_FAILURE;
    }
}
