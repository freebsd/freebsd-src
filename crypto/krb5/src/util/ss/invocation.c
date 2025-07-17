/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2007 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */
/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * For copyright information, see copyright.h.
 */
#include "ss_internal.h"
#include "copyright.h"
#define size    sizeof(ss_data *)

/* XXX The memory in _ss_table never gets freed up until program exit!
   If you change the code to free it and stick a null pointer into
   _ss_table[sci_idx], make sure you change the allocation routine to
   not assume there are no null pointers in the middle of the
   array.  */
int ss_create_invocation(subsystem_name, version_string, info_ptr,
                         request_table_ptr, code_ptr)
    char *subsystem_name, *version_string;
    char *info_ptr;
    ss_request_table *request_table_ptr;
    int *code_ptr;
{
    int sci_idx;
    ss_data *new_table;
    ss_data **table, **tmp;

    *code_ptr = 0;
    table = _ss_table;
    new_table = (ss_data *) malloc(sizeof(ss_data));
    if (new_table == NULL) {
        *code_ptr = errno;
        return -1;
    }

    if (table == (ss_data **) NULL) {
        table = (ss_data **) malloc(2 * size);
        if (table == NULL) {
            *code_ptr = errno;
            return -1;
        }
        table[0] = table[1] = (ss_data *)NULL;
        _ss_table = table;
    }
    initialize_ss_error_table ();

    for (sci_idx = 1; table[sci_idx] != (ss_data *)NULL; sci_idx++)
        ;
    tmp = (ss_data **) realloc((char *)table,
                               ((unsigned)sci_idx+2)*size);
    if (tmp == NULL) {
        *code_ptr = errno;
        return 0;
    }
    _ss_table = table = tmp;
    table[sci_idx+1] = (ss_data *) NULL;
    table[sci_idx] = NULL;

    new_table->subsystem_name = subsystem_name;
    new_table->subsystem_version = version_string;
    new_table->argv = (char **)NULL;
    new_table->current_request = (char *)NULL;
    new_table->info_dirs = (char **)malloc(sizeof(char *));
    if (new_table->info_dirs == NULL) {
        *code_ptr = errno;
        free(new_table);
        return 0;
    }
    *new_table->info_dirs = (char *)NULL;
    new_table->info_ptr = info_ptr;
    if (asprintf(&new_table->prompt, "%s:  ", subsystem_name) < 0) {
        *code_ptr = errno;
        free(new_table->info_dirs);
        free(new_table);
        return 0;
    }
    new_table->abbrev_info = NULL;
    new_table->flags.escape_disabled = 0;
    new_table->flags.abbrevs_disabled = 0;
    new_table->rqt_tables =
        (ss_request_table **) calloc(2, sizeof(ss_request_table *));
    if (new_table->rqt_tables == NULL) {
        *code_ptr = errno;
        free(new_table->prompt);
        free(new_table->info_dirs);
        free(new_table);
        return 0;
    }
    *(new_table->rqt_tables) = request_table_ptr;
    *(new_table->rqt_tables+1) = (ss_request_table *) NULL;
    table[sci_idx] = new_table;
    return(sci_idx);
}

void
ss_delete_invocation(sci_idx)
    int sci_idx;
{
    ss_data *t;
    int ignored_code;

    t = ss_info(sci_idx);
    free(t->prompt);
    free(t->rqt_tables);
    while(t->info_dirs[0] != (char *)NULL)
        ss_delete_info_dir(sci_idx, t->info_dirs[0], &ignored_code);
    free(t->info_dirs);
    free(t);
}
