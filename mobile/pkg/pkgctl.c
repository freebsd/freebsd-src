#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libpkg.h"

#define VERSION "1.0"

static void
usage(void)
{
    fprintf(stderr,
        "usage: pkgctl [-hv] command [args ...]\n"
        "Commands:\n"
        "  install <package>   Install a package\n"
        "  remove <package>    Remove a package\n"
        "  query <package>     Query package info\n"
        "  repo <subcommand>   Manage repositories\n"
        "  update              Update package database\n"
        "  upgrade             Upgrade installed packages\n"
        "  search <keyword>    Search for packages\n"
        "  info <package>      Show package information\n"
        "Options:\n"
        "  -h                  Show this help message\n"
        "  -v                  Show version\n");
}

static void
version(void)
{
    fprintf(stderr, "pkgctl version %s\n", VERSION);
}

int
main(int argc, char *argv[])
{
    int ch;
    while ((ch = getopt(argc, argv, "hv")) != -1) {
        switch (ch) {
        case 'h':
            usage();
            return 0;
        case 'v':
            version();
            return 0;
        default:
            usage();
            return 1;
        }
    }
    argc -= optind;
    argv += optind;

    if (argc < 1) {
        usage();
        return 1;
    }

    if (strcmp(argv[0], "install") == 0) {
        if (argc < 2) {
            fprintf(stderr, "pkgctl install: missing package argument\n");
            return 1;
        }
        if (pkg_install(argv[1]) != 0) {
            fprintf(stderr, "pkgctl install: failed to install '%s'\n", argv[1]);
            return 1;
        }
    } else if (strcmp(argv[0], "remove") == 0) {
        if (argc < 2) {
            fprintf(stderr, "pkgctl remove: missing package argument\n");
            return 1;
        }
        if (pkg_remove(argv[1]) != 0) {
            fprintf(stderr, "pkgctl remove: failed to remove '%s'\n", argv[1]);
            return 1;
        }
    } else if (strcmp(argv[0], "query") == 0) {
        if (argc < 2) {
            fprintf(stderr, "pkgctl query: missing package argument\n");
            return 1;
        }
        if (pkg_query(argv[1]) != 0) {
            fprintf(stderr, "pkgctl query: failed to query '%s'\n", argv[1]);
            return 1;
        }
    } else if (strcmp(argv[0], "repo") == 0) {
        if (argc < 2) {
            fprintf(stderr, "pkgctl repo: missing subcommand\n");
            return 1;
        }
        if (strcmp(argv[1], "add") == 0) {
            if (argc < 3) {
                fprintf(stderr, "pkgctl repo add: missing repository URL\n");
                return 1;
            }
            if (pkg_repo_add(argv[2]) != 0) {
                fprintf(stderr, "pkgctl repo add: failed to add '%s'\n", argv[2]);
                return 1;
            }
        } else if (strcmp(argv[1], "update") == 0) {
            if (pkg_repo_update() != 0) {
                fprintf(stderr, "pkgctl repo update: failed to update repositories\n");
                return 1;
            }
        } else {
            fprintf(stderr, "pkgctl repo: unknown subcommand '%s'\n", argv[1]);
            return 1;
        }
    } else if (strcmp(argv[0], "update") == 0) {
        if (pkg_update() != 0) {
            fprintf(stderr, "pkgctl update: failed to update package database\n");
            return 1;
        }
    } else if (strcmp(argv[0], "upgrade") == 0) {
        if (pkg_upgrade() != 0) {
            fprintf(stderr, "pkgctl upgrade: failed to upgrade packages\n");
            return 1;
        }
    } else if (strcmp(argv[0], "search") == 0) {
        if (argc < 2) {
            fprintf(stderr, "pkgctl search: missing keyword argument\n");
            return 1;
        }
        if (pkg_search(argv[1]) != 0) {
            fprintf(stderr, "pkgctl search: failed to search for '%s'\n", argv[1]);
            return 1;
        }
    } else if (strcmp(argv[0], "info") == 0) {
        if (argc < 2) {
            fprintf(stderr, "pkgctl info: missing package argument\n");
            return 1;
        }
        if (pkg_info(argv[1]) != 0) {
            fprintf(stderr, "pkgctl info: failed to get info for '%s'\n", argv[1]);
            return 1;
        }
    } else {
        fprintf(stderr, "pkgctl: unknown command '%s'\n", argv[0]);
        usage();
        return 1;
    }

    return 0;
}