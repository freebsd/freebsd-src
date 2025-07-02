/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 * $Header$
 *
 * $Log$
 * Revision 1.1  1996/07/24 22:22:43  tlyu
 *      * Makefile.in, configure.in: break out client lib into a
 *              subdirectory
 *
 * Revision 1.11  1996/07/22 20:35:46  marc
 * this commit includes all the changes on the OV_9510_INTEGRATION and
 * OV_MERGE branches.  This includes, but is not limited to, the new openvision
 * admin system, and major changes to gssapi to add functionality, and bring
 * the implementation in line with rfc1964.  before committing, the
 * code was built and tested for netbsd and solaris.
 *
 * Revision 1.10.4.1  1996/07/18 03:08:37  marc
 * merged in changes from OV_9510_BP to OV_9510_FINAL1
 *
 * Revision 1.10.2.1  1996/06/20  02:16:46  marc
 * File added to the repository on a branch
 *
 * Revision 1.10  1996/06/06  20:09:16  bjaspan
 * add destroy_cache, for kadm5_init_with_creds
 *
 * Revision 1.9  1996/05/30 21:04:42  bjaspan
 * add lhandle to handle
 *
 * Revision 1.8  1996/05/28 20:33:49  bjaspan
 * rework kadm5_config
 *
 * Revision 1.7  1996/05/17 21:36:59  bjaspan
 * rename to kadm5, begin implementing version 2
 *
 * Revision 1.6  1996/05/16 21:45:07  bjaspan
 * add context
 *
 * Revision 1.5  1996/05/08 21:10:23  bjaspan
 * marc's changes
 *
 * Revision 1.4  1996/01/16  20:54:30  grier
 * secure/3570 use krb5_ui_4 not unsigned int
 *
 * Revision 1.3  1995/11/14  17:48:57  grier
 * long to int
 *
 * Revision 1.2  1994/08/16  18:53:47  jik
 * Versioning stuff.
 *
 * Revision 1.1  1994/08/09  21:14:38  jik
 * Initial revision
 *
 */

/*
 * This header file is used internally by the Admin API client
 * libraries.  IF YOU THINK YOU NEED TO USE THIS FILE FOR ANYTHING,
 * YOU'RE ALMOST CERTAINLY WRONG.
 */

#ifndef __KADM5_CLIENT_INTERNAL_H__
#define __KADM5_CLIENT_INTERNAL_H__

#include "admin_internal.h"

typedef struct _kadm5_server_handle_t {
    krb5_ui_4       magic_number;
    krb5_ui_4       struct_version;
    krb5_ui_4       api_version;
    char *          cache_name;
    int             destroy_cache;
    CLIENT *        clnt;
    int             client_socket;
    krb5_context    context;
    gss_cred_id_t   cred;
    kadm5_config_params params;
    struct _kadm5_server_handle_t *lhandle;
} kadm5_server_handle_rec, *kadm5_server_handle_t;

#define CLIENT_CHECK_HANDLE(handle)             \
    {                                           \
        kadm5_server_handle_t srvr =            \
            (kadm5_server_handle_t) handle;     \
                                                \
        if (! srvr->clnt)                       \
            return KADM5_BAD_SERVER_HANDLE;     \
        if (! srvr->cache_name)                 \
            return KADM5_BAD_SERVER_HANDLE;     \
        if (! srvr->lhandle)                    \
            return KADM5_BAD_SERVER_HANDLE;     \
    }

#define CHECK_HANDLE(handle)                                    \
    GENERIC_CHECK_HANDLE(handle, KADM5_OLD_LIB_API_VERSION,     \
                         KADM5_NEW_LIB_API_VERSION)             \
    CLIENT_CHECK_HANDLE(handle)

#endif /* __KADM5_CLIENT_INTERNAL_H__ */
