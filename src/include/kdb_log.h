/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _KDB_LOG_H
#define _KDB_LOG_H

/* #pragma ident        "@(#)kdb_log.h  1.3     04/02/23 SMI" */

#include <iprop_hdr.h>
#include <iprop.h>
#include <limits.h>
#include "kdb.h"

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * DB macros
 */
#define INDEX(ulog, i) (kdb_ent_header_t *)((char *)(ulog) +            \
                                            sizeof(kdb_hlog_t) +        \
                                            (i) * ulog->kdb_block)

/*
 * Current DB version #
 */
#define KDB_VERSION     1

/*
 * DB log states
 */
#define KDB_STABLE      1
#define KDB_UNSTABLE    2
#define KDB_CORRUPT     3

/*
 * DB log constants
 */
#define KDB_ULOG_MAGIC          0x6661212
#define KDB_ULOG_HDR_MAGIC      0x6662323

/*
 * Default ulog file attributes
 */
#define DEF_ULOGENTRIES 1000
#define ULOG_IDLE_TIME  10              /* in seconds */
/*
 * Max size of update entry + update header
 * We make this large since resizing can be costly.
 */
#define ULOG_BLOCK      2048            /* Default size of principal record */

#define MAXLOGLEN       0x10000000      /* 256 MB log file */

/*
 * Prototype declarations
 */
krb5_error_code ulog_map(krb5_context context, const char *logname,
                         uint32_t entries);
krb5_error_code ulog_init_header(krb5_context context);
krb5_error_code ulog_add_update(krb5_context context, kdb_incr_update_t *upd);
krb5_error_code ulog_get_entries(krb5_context context, const kdb_last_t *last,
                                 kdb_incr_result_t *ulog_handle);
krb5_error_code ulog_replay(krb5_context context, kdb_incr_result_t *incr_ret,
                            char **db_args);
krb5_error_code ulog_conv_2logentry(krb5_context context, krb5_db_entry *entry,
                                    kdb_incr_update_t *update);
krb5_error_code ulog_conv_2dbentry(krb5_context context, krb5_db_entry **entry,
                                   kdb_incr_update_t *update);
void ulog_free_entries(kdb_incr_update_t *updates, int no_of_updates);
krb5_error_code ulog_set_role(krb5_context ctx, iprop_role role);
update_status_t ulog_get_sno_status(krb5_context context,
                                    const kdb_last_t *last);
krb5_error_code ulog_get_last(krb5_context context, kdb_last_t *last_out);
krb5_error_code ulog_set_last(krb5_context context, const kdb_last_t *last);
void ulog_fini(krb5_context context);

typedef struct kdb_hlog {
    uint32_t        kdb_hmagic;     /* Log header magic # */
    uint16_t        db_version_num; /* Kerberos database version no. */
    uint32_t        kdb_num;        /* # of updates in log */
    kdbe_time_t     kdb_first_time; /* Timestamp of first update */
    kdbe_time_t     kdb_last_time;  /* Timestamp of last update */
    kdb_sno_t       kdb_first_sno;  /* First serial # in the update log */
    kdb_sno_t       kdb_last_sno;   /* Last serial # in the update log */
    uint16_t        kdb_state;      /* State of update log */
    uint16_t        kdb_block;      /* Block size of each element */
} kdb_hlog_t;

typedef struct kdb_ent_header {
    uint32_t        kdb_umagic;     /* Update entry magic # */
    kdb_sno_t       kdb_entry_sno;  /* Serial # of entry */
    kdbe_time_t     kdb_time;       /* Timestamp of update */
    bool_t          kdb_commit;     /* Is the entry committed or not */
    uint32_t        kdb_entry_size; /* Size of update entry */
    uint8_t         entry_data[4];  /* Address of kdb_incr_update_t */
} kdb_ent_header_t;

typedef struct _kdb_log_context {
    iprop_role      iproprole;
    kdb_hlog_t      *ulog;
    uint32_t        ulogentries;
    int             ulogfd;
} kdb_log_context;

#ifdef  __cplusplus
}
#endif

#endif  /* !_KDB_LOG_H */
