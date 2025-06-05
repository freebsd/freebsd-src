/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * test_profile.c --- testing program for the profile routine
 */

#include "prof_int.h"

#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "argv_parse.h"
#include "com_err.h"

const char *program_name = "test_profile";

#define PRINT_VALUE     1
#define PRINT_VALUES    2

static void do_batchmode(profile)
    profile_t       profile;
{
    errcode_t       retval;
    int             argc, ret;
    char            **argv, **values, *value, **cpp;
    char            buf[256];
    const char      **names, *name;
    char            *cmd;
    int             print_status;

    while (!feof(stdin)) {
        if (fgets(buf, sizeof(buf), stdin) == NULL)
            break;
        printf(">%s", buf);
        ret = argv_parse(buf, &argc, &argv);
        if (ret != 0) {
            printf("Argv_parse returned %d!\n", ret);
            continue;
        }
        cmd = *(argv);
        names = (const char **) argv + 1;
        print_status = 0;
        retval = 0;
        if (cmd == 0) {
            argv_free(argv);
            continue;
        }
        if (!strcmp(cmd, "query")) {
            retval = profile_get_values(profile, names, &values);
            print_status = PRINT_VALUES;
        } else if (!strcmp(cmd, "query1")) {
            retval = profile_get_value(profile, names, &value);
            print_status = PRINT_VALUE;
        } else if (!strcmp(cmd, "list_sections")) {
            retval = profile_get_subsection_names(profile, names,
                                                  &values);
            print_status = PRINT_VALUES;
        } else if (!strcmp(cmd, "list_relations")) {
            retval = profile_get_relation_names(profile, names,
                                                &values);
            print_status = PRINT_VALUES;
        } else if (!strcmp(cmd, "dump")) {
            retval = profile_write_tree_file
                (profile->first_file->data->root, stdout);
        } else if (!strcmp(cmd, "clear")) {
            retval = profile_clear_relation(profile, names);
        } else if (!strcmp(cmd, "update")) {
            retval = profile_update_relation(profile, names+2,
                                             *names, *(names+1));
        } else if (!strcmp(cmd, "verify")) {
            retval = profile_verify_node
                (profile->first_file->data->root);
        } else if (!strcmp(cmd, "rename_section")) {
            retval = profile_rename_section(profile, names+1,
                                            *names);
        } else if (!strcmp(cmd, "add")) {
            name = *names;
            if (strcmp(name, "NULL") == 0)
                name = NULL;
            retval = profile_add_relation(profile, names+1, name);
        } else if (!strcmp(cmd, "flush")) {
            retval = profile_flush(profile);
        } else {
            printf("Invalid command.\n");
        }
        if (retval) {
            com_err(cmd, retval, "");
            print_status = 0;
        }
        switch (print_status) {
        case PRINT_VALUE:
            printf("%s\n", value);
            profile_release_string(value);
            break;
        case PRINT_VALUES:
            for (cpp = values; *cpp; cpp++)
                printf("%s\n", *cpp);
            profile_free_list(values);
            break;
        }
        printf("\n");
        argv_free(argv);
    }
    profile_release(profile);
    exit(0);

}


int main(argc, argv)
    int         argc;
    char        **argv;
{
    profile_t   profile;
    long        retval;
    char        **values, *value, **cpp;
    const char  **names;
    char        *cmd;
    int         print_value = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s filename [cmd argset]\n", program_name);
        exit(1);
    }

    initialize_prof_error_table();

    retval = profile_init_path(argv[1], &profile);
    if (retval) {
        com_err(program_name, retval, "while initializing profile");
        exit(1);
    }
    cmd = *(argv+2);
    names = (const char **) argv+3;
    if (!cmd || !strcmp(cmd, "batch"))
        do_batchmode(profile);
    if (!strcmp(cmd, "query")) {
        retval = profile_get_values(profile, names, &values);
    } else if (!strcmp(cmd, "query1")) {
        retval = profile_get_value(profile, names, &value);
        print_value++;
    } else if (!strcmp(cmd, "list_sections")) {
        retval = profile_get_subsection_names(profile, names, &values);
    } else if (!strcmp(cmd, "list_relations")) {
        retval = profile_get_relation_names(profile, names, &values);
    } else {
        fprintf(stderr, "Invalid command.\n");
        exit(1);
    }
    if (retval) {
        com_err(argv[0], retval, "while getting values");
        profile_release(profile);
        exit(1);
    }
    if (print_value) {
        printf("%s\n", value);
    } else {
        for (cpp = values; *cpp; cpp++)
            printf("%s\n", *cpp);
        profile_free_list(values);
    }
    profile_release(profile);

    return 0;
}
