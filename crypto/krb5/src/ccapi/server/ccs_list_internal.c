/* ccapi/server/ccs_list_internal.c */
/*
 * Copyright 2006, 2007 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
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

#include "ccs_list_internal.h"
#include "cci_array_internal.h"
#include "cci_identifier.h"
#include "ccs_server.h"

typedef enum {
    ccs_list_action_insert,
    ccs_list_action_remove,
    ccs_list_action_push_front
} ccs_list_action_enum;

/* ------------------------------------------------------------------------ */

struct ccs_list_d {
    cci_array_t objects;
    cci_array_t iterators;

    cc_int32 object_not_found_err;
    cc_int32 iterator_not_found_err;

    ccs_object_compare_identifier_t object_compare_identifier;
};

struct ccs_list_d ccs_list_initializer = { NULL, NULL, -1, -1, NULL };

/* ------------------------------------------------------------------------ */

struct ccs_list_iterator_d {
    cci_identifier_t identifier;
    ccs_pipe_t client_pipe;
    ccs_list_t list;
    cc_uint64 current;
};

struct ccs_list_iterator_d ccs_list_iterator_initializer = { NULL, CCS_PIPE_NULL, NULL, 0 };

static cc_int32 ccs_list_iterator_new (ccs_list_iterator_t *out_list_iterator,
                                       ccs_list_t           in_list,
                                       ccs_pipe_t           in_client_pipe);

static cc_int32 ccs_list_iterator_object_release (cci_array_object_t io_list_iterator);

static cc_int32 ccs_list_iterator_update (ccs_list_iterator_t  io_list_iterator,
                                          ccs_list_action_enum in_action,
                                          cc_uint64 in_object_index);

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_int32 ccs_list_new (ccs_list_t                      *out_list,
                       cc_int32                         in_object_not_found_err,
                       cc_int32                         in_iterator_not_found_err,
                       ccs_object_compare_identifier_t  in_object_compare_identifier,
                       ccs_object_release_t             in_object_release)
{
    cc_int32 err = ccNoError;
    ccs_list_t list = NULL;

    if (!out_list) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        list = malloc (sizeof (*list));
        if (list) {
            *list = ccs_list_initializer;
            list->object_not_found_err = in_object_not_found_err;
            list->iterator_not_found_err = in_iterator_not_found_err;
            list->object_compare_identifier = in_object_compare_identifier;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        err = cci_array_new (&list->objects, in_object_release);
    }

    if (!err) {
        err = cci_array_new (&list->iterators, ccs_list_iterator_object_release);
    }

    if (!err) {
        *out_list = list;
        list = NULL;
    }

    ccs_list_release (list);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_list_release (ccs_list_t io_list)
{
    cc_int32 err = ccNoError;

    if (!err && io_list) {
        cci_array_release (io_list->iterators);
        cci_array_release (io_list->objects);
        free (io_list);
    }

    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_list_new_iterator (ccs_list_t           io_list,
                                ccs_pipe_t           in_client_pipe,
                                ccs_list_iterator_t *out_list_iterator)
{
    return cci_check_error (ccs_list_iterator_new (out_list_iterator,
                                                   io_list,
                                                   in_client_pipe));
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_list_release_iterator (ccs_list_t       io_list,
                                    cci_identifier_t in_identifier)
{
    cc_int32 err = ccNoError;
    ccs_list_iterator_t iterator = NULL;

    if (!io_list      ) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_list_find_iterator (io_list, in_identifier, &iterator);
    }

    if (!err) {
        err = ccs_list_iterator_release (iterator);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_list_count (ccs_list_t  in_list,
                         cc_uint64  *out_count)
{
    cc_int32 err = ccNoError;

    if (!in_list  ) { err = cci_check_error (ccErrBadParam); }
    if (!out_count) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        *out_count = cci_array_count (in_list->objects);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static ccs_list_iterator_t ccs_list_iterator_at_index (ccs_list_t in_list,
						       cc_uint64  in_index)
{
    return (ccs_list_iterator_t) cci_array_object_at_index (in_list->iterators, in_index);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_list_find_index (ccs_list_t        in_list,
                                     cci_identifier_t  in_identifier,
                                     cc_uint64        *out_object_index)
{
    cc_int32 err = ccNoError;
    cc_int32 found = 0;

    if (!in_list         ) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier   ) { err = cci_check_error (ccErrBadParam); }
    if (!out_object_index) { err = cci_check_error (ccErrBadParam); }

    if (!err && !found) {
        cc_uint64 i;

        for (i = 0; !err && i < cci_array_count (in_list->objects); i++) {
            cc_uint32 equal = 0;
            cci_array_object_t object = cci_array_object_at_index (in_list->objects, i);

            err = in_list->object_compare_identifier (object, in_identifier, &equal);

            if (!err && equal) {
                found = 1;
                *out_object_index = i;
                break;
            }
        }
    }

    if (!err && !found) {
        err = cci_check_error (in_list->object_not_found_err);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */
cc_int32 ccs_list_find (ccs_list_t         in_list,
                        cci_identifier_t   in_identifier,
                        ccs_list_object_t *out_object)
{
    cc_int32 err = ccNoError;
    cc_uint64 i;

    if (!in_list      ) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier) { err = cci_check_error (ccErrBadParam); }
    if (!out_object   ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_list_find_index (in_list, in_identifier, &i);
    }

    if (!err) {
        *out_object = cci_array_object_at_index (in_list->objects, i);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_list_find_iterator_index (ccs_list_t        in_list,
                                              cci_identifier_t  in_identifier,
                                              cc_uint64        *out_object_index)
{
    cc_int32 err = ccNoError;
    cc_int32 found = 0;

    if (!in_list         ) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier   ) { err = cci_check_error (ccErrBadParam); }
    if (!out_object_index) { err = cci_check_error (ccErrBadParam); }

    if (!err && !found) {
        cc_uint64 i;

        for (i = 0; !err && i < cci_array_count (in_list->iterators); i++) {
            cc_uint32 equal = 0;
            ccs_list_iterator_t iterator = ccs_list_iterator_at_index (in_list, i);

            err = cci_identifier_compare (iterator->identifier, in_identifier, &equal);

            if (!err && equal) {
                found = 1;
                *out_object_index = i;
                break;
            }
        }
    }

    if (!err && !found) {
        // Don't report this error to the log file.  Non-fatal.
        return in_list->object_not_found_err;
    } else {
        return cci_check_error (err);
    }
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_list_find_iterator (ccs_list_t           in_list,
                                 cci_identifier_t     in_identifier,
                                 ccs_list_iterator_t *out_list_iterator)
{
    cc_int32 err = ccNoError;
    cc_uint64 i;

    if (!in_list          ) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier    ) { err = cci_check_error (ccErrBadParam); }
    if (!out_list_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_list_find_iterator_index (in_list, in_identifier, &i);
    }

    if (!err) {
        *out_list_iterator = ccs_list_iterator_at_index (in_list, i);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_list_add (ccs_list_t        io_list,
                       ccs_list_object_t in_object)
{
    cc_int32 err = ccNoError;
    cc_uint64 add_index;

    if (!io_list  ) { err = cci_check_error (ccErrBadParam); }
    if (!in_object) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        add_index = cci_array_count (io_list->objects);

        err = cci_array_insert (io_list->objects, in_object, add_index);
    }

    if (!err) {
        /* Fixup iterator indexes */
        cc_uint64 i;

        for (i = 0; !err && i < cci_array_count (io_list->iterators); i++) {
            ccs_list_iterator_t iterator = ccs_list_iterator_at_index (io_list, i);

            err = ccs_list_iterator_update (iterator, ccs_list_action_insert, add_index);
        }
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_list_remove (ccs_list_t       io_list,
                          cci_identifier_t in_identifier)
{
    cc_int32 err = ccNoError;
    cc_uint64 remove_index;

    if (!io_list      ) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_list_find_index (io_list, in_identifier, &remove_index);
    }

    if (!err) {
        err = cci_array_remove (io_list->objects, remove_index);
    }

    if (!err) {
        /* Fixup iterator indexes */
        cc_uint64 i;

        for (i = 0; !err && i < cci_array_count (io_list->iterators); i++) {
            ccs_list_iterator_t iterator = ccs_list_iterator_at_index (io_list, i);

            err = ccs_list_iterator_update (iterator, ccs_list_action_remove, remove_index);
        }
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_list_push_front (ccs_list_t       io_list,
                              cci_identifier_t in_identifier)
{
    cc_int32 err = ccNoError;
    cc_uint64 push_front_index;

    if (!io_list      ) { err = cci_check_error (ccErrBadParam); }
    if (!in_identifier) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_list_find_index (io_list, in_identifier, &push_front_index);
    }

    if (!err) {
        err = cci_array_push_front (io_list->objects, push_front_index);
    }

    if (!err) {
        /* Fixup iterator indexes */
        cc_uint64 i;

        for (i = 0; !err && i < cci_array_count (io_list->iterators); i++) {
            ccs_list_iterator_t iterator = ccs_list_iterator_at_index (io_list, i);

            err = ccs_list_iterator_update (iterator,
                                            ccs_list_action_push_front,
                                            push_front_index);
        }
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_list_iterator_new (ccs_list_iterator_t *out_list_iterator,
                                       ccs_list_t           io_list,
                                       ccs_pipe_t           in_client_pipe)
{
    cc_int32 err = ccNoError;
    ccs_list_iterator_t list_iterator = NULL;

    if (!out_list_iterator) { err = cci_check_error (ccErrBadParam); }
    if (!io_list          ) { err = cci_check_error (ccErrBadParam); }
    /* client_pipe may be NULL if the iterator exists for internal server use */

    if (!err) {
        list_iterator = malloc (sizeof (*list_iterator));
        if (list_iterator) {
            *list_iterator = ccs_list_iterator_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        err = ccs_server_new_identifier (&list_iterator->identifier);
    }

    if (!err) {
        list_iterator->list = io_list;
        list_iterator->current = 0;

        err = cci_array_insert (io_list->iterators,
                                (cci_array_object_t) list_iterator,
				cci_array_count (io_list->iterators));
    }

    if (!err && ccs_pipe_valid (in_client_pipe)) {
        ccs_client_t client = NULL;

        err = ccs_pipe_copy (&list_iterator->client_pipe, in_client_pipe);

        if (!err) {
            err = ccs_server_client_for_pipe (in_client_pipe, &client);
        }

        if (!err) {
            err = ccs_client_add_iterator (client, list_iterator);
        }
    }

    if (!err) {
        *out_list_iterator = list_iterator;
        list_iterator = NULL;
    }

    ccs_list_iterator_release (list_iterator);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_list_iterator_write (ccs_list_iterator_t in_list_iterator,
                                  k5_ipc_stream        in_stream)
{
    cc_int32 err = ccNoError;

    if (!in_list_iterator) { err = cci_check_error (ccErrBadParam); }
    if (!in_stream       ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = cci_identifier_write (in_list_iterator->identifier,
                                    in_stream);
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_list_iterator_clone (ccs_list_iterator_t  in_list_iterator,
                                  ccs_list_iterator_t *out_list_iterator)
{
    cc_int32 err = ccNoError;
    ccs_list_iterator_t list_iterator = NULL;

    if (!in_list_iterator ) { err = cci_check_error (ccErrBadParam); }
    if (!out_list_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = ccs_list_iterator_new (&list_iterator,
                                     in_list_iterator->list,
                                     in_list_iterator->client_pipe);
    }

    if (!err) {
        list_iterator->current = in_list_iterator->current;

        *out_list_iterator = list_iterator;
        list_iterator = NULL;
    }

    ccs_list_iterator_release (list_iterator);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_list_iterator_object_release (cci_array_object_t io_list_iterator)
{
    cc_int32 err = ccNoError;
    ccs_list_iterator_t list_iterator = (ccs_list_iterator_t) io_list_iterator;

    if (!io_list_iterator) { err = ccErrBadParam; }

    if (!err && ccs_pipe_valid (list_iterator->client_pipe)) {
	ccs_client_t client = NULL;

        err = ccs_server_client_for_pipe (list_iterator->client_pipe, &client);

        if (!err && client) {
	    /* if client object still has a reference to us, remove it */
	    err = ccs_client_remove_iterator (client, list_iterator);
        }
    }

    if (!err) {
        ccs_pipe_release (list_iterator->client_pipe);
        cci_identifier_release (list_iterator->identifier);
        free (io_list_iterator);
    }

    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_list_iterator_release (ccs_list_iterator_t io_list_iterator)
{
    cc_int32 err = ccNoError;

    if (!err && io_list_iterator) {
        cc_uint64 i = 0;

        if (ccs_list_find_iterator_index (io_list_iterator->list,
                                          io_list_iterator->identifier,
                                          &i) == ccNoError) {
            /* cci_array_remove will call ccs_list_iterator_object_release */
            err = cci_array_remove (io_list_iterator->list->iterators, i);
        } else {
            cci_debug_printf ("Warning: iterator not in iterator list!");
        }
    }

    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_list_iterator_invalidate (ccs_list_iterator_t io_list_iterator)
{
    cc_int32 err = ccNoError;
    ccs_list_iterator_t list_iterator = (ccs_list_iterator_t) io_list_iterator;

    if (!io_list_iterator) { err = ccErrBadParam; }

    if (!err) {
        /* Client owner died.  Remove client reference and then the iterator. */
        if (ccs_pipe_valid (list_iterator->client_pipe)) {
            ccs_pipe_release (list_iterator->client_pipe);
            list_iterator->client_pipe = CCS_PIPE_NULL;
        }

        err = ccs_list_iterator_release (io_list_iterator);
    }

    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_list_iterator_current (ccs_list_iterator_t  io_list_iterator,
                                    ccs_list_object_t   *out_object)
{
    cc_int32 err = ccNoError;

    if (!io_list_iterator) { err = cci_check_error (ccErrBadParam); }
    if (!out_object      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        if (io_list_iterator->current < cci_array_count (io_list_iterator->list->objects)) {
            *out_object = cci_array_object_at_index (io_list_iterator->list->objects,
                                                     io_list_iterator->current);
        } else {
            err = ccIteratorEnd;
        }
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_list_iterator_next (ccs_list_iterator_t  io_list_iterator,
                                 ccs_list_object_t   *out_object)
{
    cc_int32 err = ccNoError;

    if (!io_list_iterator) { err = cci_check_error (ccErrBadParam); }
    if (!out_object      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        if (io_list_iterator->current < cci_array_count (io_list_iterator->list->objects)) {
            *out_object = cci_array_object_at_index (io_list_iterator->list->objects,
                                                     io_list_iterator->current);
            io_list_iterator->current++;
        } else {
            err = ccIteratorEnd;
        }
    }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_list_iterator_update (ccs_list_iterator_t  io_list_iterator,
                                          ccs_list_action_enum in_action,
                                          cc_uint64            in_object_index)
{
    cc_int32 err = ccNoError;

    if (!io_list_iterator) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        /* When the list changes adjust the current index so that */
        /* we don't unnecessarily skip or double count items      */
        if (in_action == ccs_list_action_insert) {
            if (io_list_iterator->current > in_object_index) {
                io_list_iterator->current++;
            }

        } else if (in_action == ccs_list_action_remove) {
            if (io_list_iterator->current >= in_object_index) {
                io_list_iterator->current--;
            }

        } else if (in_action == ccs_list_action_push_front) {
            if (io_list_iterator->current < in_object_index) {
                io_list_iterator->current++;
            }

        } else {
            err = cci_check_error (ccErrBadParam);
        }
    }

    return cci_check_error (err);
}
