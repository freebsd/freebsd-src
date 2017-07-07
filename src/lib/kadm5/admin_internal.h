/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 */

#ifndef __KADM5_ADMIN_INTERNAL_H__
#define __KADM5_ADMIN_INTERNAL_H__

#include <kadm5/admin.h>

#define KADM5_SERVER_HANDLE_MAGIC       0x12345800

#define GENERIC_CHECK_HANDLE(handle, old_api_version, new_api_version)  \
    {                                                                   \
        kadm5_server_handle_t srvr =                                    \
            (kadm5_server_handle_t) handle;                             \
                                                                        \
        if (! srvr)                                                     \
            return KADM5_BAD_SERVER_HANDLE;                             \
        if (srvr->magic_number != KADM5_SERVER_HANDLE_MAGIC)            \
            return KADM5_BAD_SERVER_HANDLE;                             \
        if ((srvr->struct_version & KADM5_MASK_BITS) !=                 \
            KADM5_STRUCT_VERSION_MASK)                                  \
            return KADM5_BAD_STRUCT_VERSION;                            \
        if (srvr->struct_version < KADM5_STRUCT_VERSION_1)              \
            return KADM5_OLD_STRUCT_VERSION;                            \
        if (srvr->struct_version > KADM5_STRUCT_VERSION_1)              \
            return KADM5_NEW_STRUCT_VERSION;                            \
        if ((srvr->api_version & KADM5_MASK_BITS) !=                    \
            KADM5_API_VERSION_MASK)                                     \
            return KADM5_BAD_API_VERSION;                               \
        if (srvr->api_version < KADM5_API_VERSION_2)                    \
            return old_api_version;                                     \
        if (srvr->api_version > KADM5_API_VERSION_4)                    \
            return new_api_version;                                     \
    }

/*
 * _KADM5_CHECK_HANDLE calls the function _kadm5_check_handle and
 * returns any non-zero error code that function returns.
 * _kadm5_check_handle, in client_handle.c and server_handle.c, exists
 * in both the server- and client- side libraries.  In each library,
 * it calls CHECK_HANDLE, which is defined by the appropriate
 * _internal.h header file to call GENERIC_CHECK_HANDLE as well as
 * CLIENT_CHECK_HANDLE and SERVER_CHECK_HANDLE.
 *
 * _KADM5_CHECK_HANDLE should be used by a function that needs to
 * check the handle but wants to be the same code in both the client
 * and server library; it makes a function call to the right handle
 * checker.  Code that only exists in one library can call the
 * CHECK_HANDLE macro, which inlines the test instead of making
 * another function call.
 *
 * Got that?
 */
#define _KADM5_CHECK_HANDLE(handle)                                     \
    { int ecode; if ((ecode = _kadm5_check_handle((void *)handle))) return ecode;}

int         _kadm5_check_handle(void *handle);
kadm5_ret_t _kadm5_chpass_principal_util(void *server_handle,
                                         void *lhandle,
                                         krb5_principal princ,
                                         char *new_pw,
                                         char **ret_pw,
                                         char *msg_ret,
                                         unsigned int msg_len);

/* this is needed by the alt_prof code I stole.  The functions
   maybe shouldn't be named krb5_*, but they are. */

krb5_error_code
krb5_string_to_keysalts(const char *string, const char *tupleseps,
                        const char *ksaltseps, krb5_boolean dups,
                        krb5_key_salt_tuple **ksaltp, krb5_int32 *nksaltp);

#endif /* __KADM5_ADMIN_INTERNAL_H__ */
