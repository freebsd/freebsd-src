/* ccapi/common/cci_types.h */
/*
 * Copyright 2006 Massachusetts Institute of Technology.
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

#ifndef CCI_TYPES_H
#define CCI_TYPES_H

#include <CredentialsCache.h>
#include <k5-ipc_stream.h>

typedef char *cci_uuid_string_t;

struct cci_identifier_d;
typedef struct cci_identifier_d *cci_identifier_t;

enum cci_msg_id_t {
    /* cc_context_t */
    cci_context_first_msg_id,

    cci_context_unused_release_msg_id,  /* Unused. Handle for old clients. */
    cci_context_sync_msg_id,
    cci_context_get_change_time_msg_id,
    cci_context_wait_for_change_msg_id,
    cci_context_get_default_ccache_name_msg_id,
    cci_context_open_ccache_msg_id,
    cci_context_open_default_ccache_msg_id,
    cci_context_create_ccache_msg_id,
    cci_context_create_default_ccache_msg_id,
    cci_context_create_new_ccache_msg_id,
    cci_context_new_ccache_iterator_msg_id,
    cci_context_lock_msg_id,
    cci_context_unlock_msg_id,

    cci_context_last_msg_id,

    /* cc_ccache_t */
    cci_ccache_first_msg_id,

    cci_ccache_destroy_msg_id,
    cci_ccache_set_default_msg_id,
    cci_ccache_get_credentials_version_msg_id,
    cci_ccache_get_name_msg_id,
    cci_ccache_get_principal_msg_id,
    cci_ccache_set_principal_msg_id,
    cci_ccache_store_credentials_msg_id,
    cci_ccache_remove_credentials_msg_id,
    cci_ccache_new_credentials_iterator_msg_id,
    cci_ccache_move_msg_id,
    cci_ccache_lock_msg_id,
    cci_ccache_unlock_msg_id,
    cci_ccache_get_last_default_time_msg_id,
    cci_ccache_get_change_time_msg_id,
    cci_ccache_wait_for_change_msg_id,
    cci_ccache_get_kdc_time_offset_msg_id,
    cci_ccache_set_kdc_time_offset_msg_id,
    cci_ccache_clear_kdc_time_offset_msg_id,

    cci_ccache_last_msg_id,

    /* cc_ccache_iterator_t */
    cci_ccache_iterator_first_msg_id,

    cci_ccache_iterator_release_msg_id,
    cci_ccache_iterator_next_msg_id,
    cci_ccache_iterator_clone_msg_id,

    cci_ccache_iterator_last_msg_id,

    /* cc_credentials_iterator_t */
    cci_credentials_iterator_first_msg_id,

    cci_credentials_iterator_release_msg_id,
    cci_credentials_iterator_next_msg_id,
    cci_credentials_iterator_clone_msg_id,

    cci_credentials_iterator_last_msg_id,

    cci_max_msg_id  /* must be last! */
};

#endif /* CCI_TYPES_H */
