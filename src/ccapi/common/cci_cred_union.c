/* ccapi/common/cci_cred_union.c */
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

#include "cci_common.h"

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

static cc_uint32 cci_credentials_v4_release (cc_credentials_v4_t *io_v4creds)
{
    cc_int32 err = ccNoError;

    if (!io_v4creds) { err = ccErrBadParam; }

    if (!err) {
        memset (io_v4creds, 0, sizeof (*io_v4creds));
        free (io_v4creds);
    }

    return err;
}

/* ------------------------------------------------------------------------ */

static cc_uint32 cci_credentials_v4_read (cc_credentials_v4_t **out_v4creds,
                                          k5_ipc_stream          io_stream)
{
    cc_int32 err = ccNoError;
    cc_credentials_v4_t *v4creds = NULL;

    if (!io_stream  ) { err = cci_check_error (ccErrBadParam); }
    if (!out_v4creds) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        v4creds = malloc (sizeof (*v4creds));
        if (!v4creds) { err = cci_check_error (ccErrNoMem); }
    }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (io_stream, &v4creds->version);
    }

    if (!err) {
        err = krb5int_ipc_stream_read (io_stream, v4creds->principal, cc_v4_name_size);
    }

    if (!err) {
        err = krb5int_ipc_stream_read (io_stream, v4creds->principal_instance, cc_v4_instance_size);
    }

    if (!err) {
        err = krb5int_ipc_stream_read (io_stream, v4creds->service, cc_v4_name_size);
    }

    if (!err) {
        err = krb5int_ipc_stream_read (io_stream, v4creds->service_instance, cc_v4_instance_size);
    }

    if (!err) {
        err = krb5int_ipc_stream_read (io_stream, v4creds->realm, cc_v4_realm_size);
    }

    if (!err) {
        err = krb5int_ipc_stream_read (io_stream, v4creds->session_key, cc_v4_key_size);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_int32 (io_stream, &v4creds->kvno);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_int32 (io_stream, &v4creds->string_to_key_type);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_time (io_stream, &v4creds->issue_date);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_int32 (io_stream, &v4creds->lifetime);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (io_stream, &v4creds->address);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_int32 (io_stream, &v4creds->ticket_size);
    }

    if (!err) {
        err = krb5int_ipc_stream_read (io_stream, v4creds->ticket, cc_v4_ticket_size);
    }

    if (!err) {
        *out_v4creds = v4creds;
        v4creds = NULL;
    }

    free (v4creds);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_uint32 cci_credentials_v4_write (cc_credentials_v4_t *in_v4creds,
                                           k5_ipc_stream         io_stream)
{
    cc_int32 err = ccNoError;

    if (!io_stream ) { err = cci_check_error (ccErrBadParam); }
    if (!in_v4creds) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (io_stream, in_v4creds->version);
    }

    if (!err) {
        err = krb5int_ipc_stream_write (io_stream, in_v4creds->principal, cc_v4_name_size);
    }

    if (!err) {
        err = krb5int_ipc_stream_write (io_stream, in_v4creds->principal_instance, cc_v4_instance_size);
    }

    if (!err) {
        err = krb5int_ipc_stream_write (io_stream, in_v4creds->service, cc_v4_name_size);
    }

    if (!err) {
        err = krb5int_ipc_stream_write (io_stream, in_v4creds->service_instance, cc_v4_instance_size);
    }

    if (!err) {
        err = krb5int_ipc_stream_write (io_stream, in_v4creds->realm, cc_v4_realm_size);
    }

    if (!err) {
        err = krb5int_ipc_stream_write (io_stream, in_v4creds->session_key, cc_v4_key_size);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_int32 (io_stream, in_v4creds->kvno);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_int32 (io_stream, in_v4creds->string_to_key_type);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_time (io_stream, in_v4creds->issue_date);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_int32 (io_stream, in_v4creds->lifetime);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (io_stream, in_v4creds->address);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_int32 (io_stream, in_v4creds->ticket_size);
    }

    if (!err) {
        err = krb5int_ipc_stream_write (io_stream, in_v4creds->ticket, cc_v4_ticket_size);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

static cc_uint32 cci_cc_data_contents_release (cc_data *io_ccdata)
{
    cc_int32 err = ccNoError;

    if (!io_ccdata && io_ccdata->data) { err = ccErrBadParam; }

    if (!err) {
        if (io_ccdata->length) {
            memset (io_ccdata->data, 0, io_ccdata->length);
        }
        free (io_ccdata->data);
    }

    return err;
}

/* ------------------------------------------------------------------------ */

static cc_uint32 cci_cc_data_release (cc_data *io_ccdata)
{
    cc_int32 err = ccNoError;

    if (!io_ccdata) { err = ccErrBadParam; }

    if (!err) {
        cci_cc_data_contents_release (io_ccdata);
        free (io_ccdata);
    }

    return err;
}

/* ------------------------------------------------------------------------ */

static cc_uint32 cci_cc_data_read (cc_data      *io_ccdata,
                                   k5_ipc_stream io_stream)
{
    cc_int32 err = ccNoError;
    cc_uint32 type = 0;
    cc_uint32 length = 0;
    char *data = NULL;

    if (!io_stream) { err = cci_check_error (ccErrBadParam); }
    if (!io_ccdata) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (io_stream, &type);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (io_stream, &length);
    }

    if (!err && length > 0) {
        data = malloc (length);
        if (!data) { err = cci_check_error (ccErrNoMem); }

        if (!err) {
            err = krb5int_ipc_stream_read (io_stream, data, length);
        }
    }

    if (!err) {
        io_ccdata->type = type;
        io_ccdata->length = length;
        io_ccdata->data = data;
        data = NULL;
    }

    free (data);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_uint32 cci_cc_data_write (cc_data      *in_ccdata,
                                    k5_ipc_stream io_stream)
{
    cc_int32 err = ccNoError;

    if (!io_stream) { err = cci_check_error (ccErrBadParam); }
    if (!in_ccdata) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (io_stream, in_ccdata->type);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (io_stream, in_ccdata->length);
    }

    if (!err && in_ccdata->length > 0) {
        err = krb5int_ipc_stream_write (io_stream, in_ccdata->data, in_ccdata->length);
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

static cc_uint32 cci_cc_data_array_release (cc_data **io_ccdata_array)
{
    cc_int32 err = ccNoError;

    if (!io_ccdata_array) { err = ccErrBadParam; }

    if (!err) {
        cc_uint32 i;

        for (i = 0; io_ccdata_array && io_ccdata_array[i]; i++) {
            cci_cc_data_release (io_ccdata_array[i]);
        }
        free (io_ccdata_array);
    }

    return err;
}

/* ------------------------------------------------------------------------ */

static cc_uint32 cci_cc_data_array_read (cc_data      ***io_ccdata_array,
                                         k5_ipc_stream    io_stream)
{
    cc_int32 err = ccNoError;
    cc_uint32 count = 0;
    cc_data **array = NULL;
    cc_uint32 i;

    if (!io_stream      ) { err = cci_check_error (ccErrBadParam); }
    if (!io_ccdata_array) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (io_stream, &count);
    }

    if (!err && count > 0) {
        array = malloc ((count + 1) * sizeof (*array));
        if (array) {
            for (i = 0; i <= count; i++) { array[i] = NULL; }
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        for (i = 0; !err && i < count; i++) {
            array[i] = malloc (sizeof (cc_data));
            if (!array[i]) { err = cci_check_error (ccErrNoMem); }

            if (!err) {
                err = cci_cc_data_read (array[i], io_stream);
            }
        }
    }

    if (!err) {
        *io_ccdata_array = array;
        array = NULL;
    }

    cci_cc_data_array_release (array);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_uint32 cci_cc_data_array_write (cc_data      **in_ccdata_array,
                                          k5_ipc_stream   io_stream)
{
    cc_int32 err = ccNoError;
    cc_uint32 count = 0;

    if (!io_stream) { err = cci_check_error (ccErrBadParam); }
    /* in_ccdata_array may be NULL */

    if (!err) {
        for (count = 0; in_ccdata_array && in_ccdata_array[count]; count++);

        err = krb5int_ipc_stream_write_uint32 (io_stream, count);
    }

    if (!err) {
        cc_uint32 i;

        for (i = 0; !err && i < count; i++) {
            err = cci_cc_data_write (in_ccdata_array[i], io_stream);
        }
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_credentials_v5_t cci_credentials_v5_initializer = {
    NULL,
    NULL,
    { 0, 0, NULL },
    0, 0, 0, 0, 0, 0,
    NULL,
    { 0, 0, NULL },
    { 0, 0, NULL },
    NULL
};

/* ------------------------------------------------------------------------ */

static cc_uint32 cci_credentials_v5_release (cc_credentials_v5_t *io_v5creds)
{
    cc_int32 err = ccNoError;

    if (!io_v5creds) { err = ccErrBadParam; }

    if (!err) {
        free (io_v5creds->client);
        free (io_v5creds->server);
        cci_cc_data_contents_release (&io_v5creds->keyblock);
        cci_cc_data_array_release (io_v5creds->addresses);
        cci_cc_data_contents_release (&io_v5creds->ticket);
        cci_cc_data_contents_release (&io_v5creds->second_ticket);
        cci_cc_data_array_release (io_v5creds->authdata);
        free (io_v5creds);
    }

    return err;
}

/* ------------------------------------------------------------------------ */

static cc_uint32 cci_credentials_v5_read (cc_credentials_v5_t **out_v5creds,
                                          k5_ipc_stream          io_stream)
{
    cc_int32 err = ccNoError;
    cc_credentials_v5_t *v5creds = NULL;

    if (!io_stream  ) { err = cci_check_error (ccErrBadParam); }
    if (!out_v5creds) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        v5creds = malloc (sizeof (*v5creds));
        if (v5creds) {
            *v5creds = cci_credentials_v5_initializer;
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        err = krb5int_ipc_stream_read_string (io_stream, &v5creds->client);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_string (io_stream, &v5creds->server);
    }

    if (!err) {
        err = cci_cc_data_read (&v5creds->keyblock, io_stream);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_time (io_stream, &v5creds->authtime);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_time (io_stream, &v5creds->starttime);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_time (io_stream, &v5creds->endtime);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_time (io_stream, &v5creds->renew_till);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (io_stream, &v5creds->is_skey);
    }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (io_stream, &v5creds->ticket_flags);
    }

    if (!err) {
        err = cci_cc_data_array_read (&v5creds->addresses, io_stream);
    }

    if (!err) {
        err = cci_cc_data_read (&v5creds->ticket, io_stream);
    }

    if (!err) {
        err = cci_cc_data_read (&v5creds->second_ticket, io_stream);
    }

    if (!err) {
        err = cci_cc_data_array_read (&v5creds->authdata, io_stream);
    }

    if (!err) {
        *out_v5creds = v5creds;
        v5creds = NULL;
    }

    cci_credentials_v5_release (v5creds);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_uint32 cci_credentials_v5_write (cc_credentials_v5_t *in_v5creds,
                                           k5_ipc_stream         io_stream)
{
    cc_int32 err = ccNoError;

    if (!io_stream ) { err = cci_check_error (ccErrBadParam); }
    if (!in_v5creds) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_write_string (io_stream, in_v5creds->client);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_string (io_stream, in_v5creds->server);
    }

    if (!err) {
        err = cci_cc_data_write (&in_v5creds->keyblock, io_stream);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_time (io_stream, in_v5creds->authtime);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_time (io_stream, in_v5creds->starttime);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_time (io_stream, in_v5creds->endtime);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_time (io_stream, in_v5creds->renew_till);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (io_stream, in_v5creds->is_skey);
    }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (io_stream, in_v5creds->ticket_flags);
    }

    if (!err) {
        err = cci_cc_data_array_write (in_v5creds->addresses, io_stream);
    }

    if (!err) {
        err = cci_cc_data_write (&in_v5creds->ticket, io_stream);
    }

    if (!err) {
        err = cci_cc_data_write (&in_v5creds->second_ticket, io_stream);
    }

    if (!err) {
        err = cci_cc_data_array_write (in_v5creds->authdata, io_stream);
    }


    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

cc_uint32 cci_credentials_union_release (cc_credentials_union *io_cred_union)
{
    cc_int32 err = ccNoError;

    if (!io_cred_union) { err = ccErrBadParam; }

    if (!err) {
        if (io_cred_union->version == cc_credentials_v4) {
            cci_credentials_v4_release (io_cred_union->credentials.credentials_v4);
        } else if (io_cred_union->version == cc_credentials_v5) {
            cci_credentials_v5_release (io_cred_union->credentials.credentials_v5);
        }
        free (io_cred_union);
    }

    return err;
}

/* ------------------------------------------------------------------------ */

cc_uint32 cci_credentials_union_read (cc_credentials_union **out_credentials_union,
                                      k5_ipc_stream           io_stream)
{
    cc_int32 err = ccNoError;
    cc_credentials_union *credentials_union = NULL;

    if (!io_stream            ) { err = cci_check_error (ccErrBadParam); }
    if (!out_credentials_union) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        credentials_union = calloc (1, sizeof (*credentials_union));
        if (!credentials_union) { err = cci_check_error (ccErrNoMem); }
    }

    if (!err) {
        err = krb5int_ipc_stream_read_uint32 (io_stream, &credentials_union->version);
    }

    if (!err) {
        if (credentials_union->version == cc_credentials_v4) {
            err = cci_credentials_v4_read (&credentials_union->credentials.credentials_v4,
                                           io_stream);

        } else if (credentials_union->version == cc_credentials_v5) {
            err = cci_credentials_v5_read (&credentials_union->credentials.credentials_v5,
                                           io_stream);


        } else {
            err = ccErrBadCredentialsVersion;
        }
    }

    if (!err) {
        *out_credentials_union = credentials_union;
        credentials_union = NULL;
    }

    if (credentials_union) { cci_credentials_union_release (credentials_union); }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_uint32 cci_credentials_union_write (const cc_credentials_union *in_credentials_union,
                                       k5_ipc_stream                io_stream)
{
    cc_int32 err = ccNoError;

    if (!io_stream           ) { err = cci_check_error (ccErrBadParam); }
    if (!in_credentials_union) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        err = krb5int_ipc_stream_write_uint32 (io_stream, in_credentials_union->version);
    }

    if (!err) {
        if (in_credentials_union->version == cc_credentials_v4) {
            err = cci_credentials_v4_write (in_credentials_union->credentials.credentials_v4,
                                            io_stream);

        } else if (in_credentials_union->version == cc_credentials_v5) {
            err = cci_credentials_v5_write (in_credentials_union->credentials.credentials_v5,
                                            io_stream);

        } else {
            err = ccErrBadCredentialsVersion;
        }
    }

    return cci_check_error (err);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#pragma mark -- CCAPI v2 Compat --
#endif

/* ------------------------------------------------------------------------ */

cc_credentials_v5_compat cci_credentials_v5_compat_initializer = {
    NULL,
    NULL,
    { 0, 0, NULL },
    0, 0, 0, 0, 0, 0,
    NULL,
    { 0, 0, NULL },
    { 0, 0, NULL },
    NULL
};

/* ------------------------------------------------------------------------ */

cc_uint32 cci_cred_union_release (cred_union *io_cred_union)
{
    cc_int32 err = ccNoError;

    if (!io_cred_union) { err = ccErrBadParam; }

    if (!err) {
        if (io_cred_union->cred_type == CC_CRED_V4) {
            memset (io_cred_union->cred.pV4Cred, 0, sizeof (cc_credentials_v4_compat));
            free (io_cred_union->cred.pV4Cred);

        } else if (io_cred_union->cred_type == CC_CRED_V5) {
            free (io_cred_union->cred.pV5Cred->client);
            free (io_cred_union->cred.pV5Cred->server);
            cci_cc_data_contents_release (&io_cred_union->cred.pV5Cred->keyblock);
            cci_cc_data_array_release (io_cred_union->cred.pV5Cred->addresses);
            cci_cc_data_contents_release (&io_cred_union->cred.pV5Cred->ticket);
            cci_cc_data_contents_release (&io_cred_union->cred.pV5Cred->second_ticket);
            cci_cc_data_array_release (io_cred_union->cred.pV5Cred->authdata);
            free (io_cred_union->cred.pV5Cred);
        }
        free (io_cred_union);
    }

    return err;
}

/* ------------------------------------------------------------------------ */

static cc_uint32 cci_cc_data_copy_contents (cc_data      *io_ccdata,
                                            cc_data      *in_ccdata)
{
    cc_int32 err = ccNoError;
    char *data = NULL;

    if (!io_ccdata) { err = cci_check_error (ccErrBadParam); }
    if (!in_ccdata) { err = cci_check_error (ccErrBadParam); }

    if (!err && in_ccdata->length > 0) {
        data = malloc (in_ccdata->length);
        if (data) {
            memcpy (data, in_ccdata->data, in_ccdata->length);
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        io_ccdata->type = in_ccdata->type;
        io_ccdata->length = in_ccdata->length;
        io_ccdata->data = data;
        data = NULL;
    }

    free (data);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

static cc_uint32 cci_cc_data_array_copy (cc_data      ***io_ccdata_array,
                                         cc_data       **in_ccdata_array)
{
    cc_int32 err = ccNoError;
    cc_uint32 count = 0;
    cc_data **array = NULL;
    cc_uint32 i;

    if (!io_ccdata_array) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        for (count = 0; in_ccdata_array && in_ccdata_array[count]; count++);
    }

    if (!err && count > 0) {
        array = malloc ((count + 1) * sizeof (*array));
        if (array) {
            for (i = 0; i <= count; i++) { array[i] = NULL; }
        } else {
            err = cci_check_error (ccErrNoMem);
        }
    }

    if (!err) {
        for (i = 0; !err && i < count; i++) {
            array[i] = malloc (sizeof (cc_data));
            if (!array[i]) { err = cci_check_error (ccErrNoMem); }

            if (!err) {
                err = cci_cc_data_copy_contents (array[i], in_ccdata_array[i]);
            }
        }
    }

    if (!err) {
        *io_ccdata_array = array;
        array = NULL;
    }

    cci_cc_data_array_release (array);

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_uint32 cci_credentials_union_to_cred_union (const cc_credentials_union  *in_credentials_union,
                                               cred_union                 **out_cred_union)
{
    cc_int32 err = ccNoError;
    cred_union *compat_cred_union = NULL;

    if (!in_credentials_union) { err = cci_check_error (ccErrBadParam); }
    if (!out_cred_union      ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        compat_cred_union = calloc (1, sizeof (*compat_cred_union));
        if (!compat_cred_union) { err = cci_check_error (ccErrNoMem); }
    }

    if (!err) {
        if (in_credentials_union->version == cc_credentials_v4) {
            cc_credentials_v4_compat *compat_v4creds = NULL;

            compat_v4creds = malloc (sizeof (*compat_v4creds));
            if (!compat_v4creds) { err = cci_check_error (ccErrNoMem); }

            if (!err) {
                cc_credentials_v4_t *v4creds = in_credentials_union->credentials.credentials_v4;

                compat_cred_union->cred_type = CC_CRED_V4;
                compat_cred_union->cred.pV4Cred = compat_v4creds;

                compat_v4creds->kversion = v4creds->version;
                strncpy (compat_v4creds->principal,          v4creds->principal,          KRB_NAME_SZ+1);
                strncpy (compat_v4creds->principal_instance, v4creds->principal_instance, KRB_INSTANCE_SZ+1);
                strncpy (compat_v4creds->service,            v4creds->service,            KRB_NAME_SZ+1);
                strncpy (compat_v4creds->service_instance,   v4creds->service_instance,   KRB_INSTANCE_SZ+1);
                strncpy (compat_v4creds->realm,              v4creds->realm,              KRB_REALM_SZ+1);
                memcpy (compat_v4creds->session_key, v4creds->session_key, 8);
                compat_v4creds->kvno       = v4creds->kvno;
                compat_v4creds->str_to_key = v4creds->string_to_key_type;
                compat_v4creds->issue_date = v4creds->issue_date;
                compat_v4creds->lifetime   = v4creds->lifetime;
                compat_v4creds->address    = v4creds->address;
                compat_v4creds->ticket_sz  = v4creds->ticket_size;
                memcpy (compat_v4creds->ticket, v4creds->ticket, MAX_V4_CRED_LEN);
                compat_v4creds->oops = 0;
            }

        } else if (in_credentials_union->version == cc_credentials_v5) {
            cc_credentials_v5_t *v5creds = in_credentials_union->credentials.credentials_v5;
            cc_credentials_v5_compat *compat_v5creds = NULL;

            compat_v5creds = malloc (sizeof (*compat_v5creds));
            if (compat_v5creds) {
                *compat_v5creds = cci_credentials_v5_compat_initializer;
            } else {
                err = cci_check_error (ccErrNoMem);
            }

            if (!err) {
                if (!v5creds->client) {
                    err = cci_check_error (ccErrBadParam);
                } else {
                    compat_v5creds->client = strdup (v5creds->client);
                    if (!compat_v5creds->client) { err = cci_check_error (ccErrNoMem); }
                }
            }

            if (!err) {
                if (!v5creds->server) {
                    err = cci_check_error (ccErrBadParam);
                } else {
                    compat_v5creds->server = strdup (v5creds->server);
                    if (!compat_v5creds->server) { err = cci_check_error (ccErrNoMem); }
                }
            }

            if (!err) {
                err = cci_cc_data_copy_contents (&compat_v5creds->keyblock, &v5creds->keyblock);
            }

            if (!err) {
                err = cci_cc_data_array_copy (&compat_v5creds->addresses, v5creds->addresses);
            }

            if (!err) {
                err = cci_cc_data_copy_contents (&compat_v5creds->ticket, &v5creds->ticket);
            }

            if (!err) {
                err = cci_cc_data_copy_contents (&compat_v5creds->second_ticket, &v5creds->second_ticket);
            }

            if (!err) {
                err = cci_cc_data_array_copy (&compat_v5creds->authdata, v5creds->authdata);
            }

            if (!err) {
                compat_cred_union->cred_type = CC_CRED_V5;
                compat_cred_union->cred.pV5Cred = compat_v5creds;

                compat_v5creds->keyblock      = v5creds->keyblock;
                compat_v5creds->authtime      = v5creds->authtime;
                compat_v5creds->starttime     = v5creds->starttime;
                compat_v5creds->endtime       = v5creds->endtime;
                compat_v5creds->renew_till    = v5creds->renew_till;
                compat_v5creds->is_skey       = v5creds->is_skey;
                compat_v5creds->ticket_flags  = v5creds->ticket_flags;
            }
        } else {
            err = cci_check_error (ccErrBadCredentialsVersion);
        }
    }

    if (!err) {
        *out_cred_union = compat_cred_union;
        compat_cred_union = NULL;
    }

    if (compat_cred_union) { cci_cred_union_release (compat_cred_union); }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_uint32 cci_cred_union_to_credentials_union (const cred_union      *in_cred_union,
                                               cc_credentials_union **out_credentials_union)
{
    cc_int32 err = ccNoError;
    cc_credentials_union *creds_union = NULL;

    if (!in_cred_union        ) { err = cci_check_error (ccErrBadParam); }
    if (!out_credentials_union) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        creds_union = calloc (1, sizeof (*creds_union));
        if (!creds_union) { err = cci_check_error (ccErrNoMem); }
    }

    if (!err) {
        if (in_cred_union->cred_type == CC_CRED_V4) {
            cc_credentials_v4_compat *compat_v4creds = in_cred_union->cred.pV4Cred;
            cc_credentials_v4_t *v4creds = NULL;

            if (!err) {
                v4creds = malloc (sizeof (*v4creds));
                if (!v4creds) { err = cci_check_error (ccErrNoMem); }
            }

            if (!err) {
                creds_union->version = cc_credentials_v4;
                creds_union->credentials.credentials_v4 = v4creds;

                v4creds->version = compat_v4creds->kversion;
                strncpy (v4creds->principal,          compat_v4creds->principal,          KRB_NAME_SZ);
                strncpy (v4creds->principal_instance, compat_v4creds->principal_instance, KRB_INSTANCE_SZ);
                strncpy (v4creds->service,            compat_v4creds->service,            KRB_NAME_SZ);
                strncpy (v4creds->service_instance,   compat_v4creds->service_instance,   KRB_INSTANCE_SZ);
                strncpy (v4creds->realm,              compat_v4creds->realm,              KRB_REALM_SZ);
                memcpy (v4creds->session_key, compat_v4creds->session_key, 8);
                v4creds->kvno               = compat_v4creds->kvno;
                v4creds->string_to_key_type = compat_v4creds->str_to_key;
                v4creds->issue_date         = compat_v4creds->issue_date;
                v4creds->lifetime           = compat_v4creds->lifetime;
                v4creds->address            = compat_v4creds->address;
                v4creds->ticket_size        = compat_v4creds->ticket_sz;
                memcpy (v4creds->ticket, compat_v4creds->ticket, MAX_V4_CRED_LEN);
            }

        } else if (in_cred_union->cred_type == CC_CRED_V5) {
            cc_credentials_v5_compat *compat_v5creds = in_cred_union->cred.pV5Cred;
            cc_credentials_v5_t *v5creds = NULL;

            if (!err) {
                v5creds = malloc (sizeof (*v5creds));
                if (v5creds) {
                    *v5creds = cci_credentials_v5_initializer;
                } else {
                    err = cci_check_error (ccErrNoMem);
                }
            }

            if (!err) {
                if (!compat_v5creds->client) {
                    err = cci_check_error (ccErrBadParam);
                } else {
                    v5creds->client = strdup (compat_v5creds->client);
                    if (!v5creds->client) { err = cci_check_error (ccErrNoMem); }
                }
            }

            if (!err) {
                if (!compat_v5creds->server) {
                    err = cci_check_error (ccErrBadParam);
                } else {
                    v5creds->server = strdup (compat_v5creds->server);
                    if (!v5creds->server) { err = cci_check_error (ccErrNoMem); }
                }
            }

            if (!err) {
                err = cci_cc_data_copy_contents (&v5creds->keyblock, &compat_v5creds->keyblock);
            }

            if (!err) {
                err = cci_cc_data_array_copy (&v5creds->addresses, compat_v5creds->addresses);
            }

            if (!err) {
                err = cci_cc_data_copy_contents (&v5creds->ticket, &compat_v5creds->ticket);
            }

            if (!err) {
                err = cci_cc_data_copy_contents (&v5creds->second_ticket, &compat_v5creds->second_ticket);
            }

            if (!err) {
                err = cci_cc_data_array_copy (&v5creds->authdata, compat_v5creds->authdata);
            }

            if (!err) {
                creds_union->version = cc_credentials_v5;
                creds_union->credentials.credentials_v5 = v5creds;

                v5creds->authtime      = compat_v5creds->authtime;
                v5creds->starttime     = compat_v5creds->starttime;
                v5creds->endtime       = compat_v5creds->endtime;
                v5creds->renew_till    = compat_v5creds->renew_till;
                v5creds->is_skey       = compat_v5creds->is_skey;
                v5creds->ticket_flags  = compat_v5creds->ticket_flags;
            }

        } else {
            err = cci_check_error (ccErrBadCredentialsVersion);
        }
    }

    if (!err) {
        *out_credentials_union = creds_union;
        creds_union = NULL;
    }

    if (creds_union) { cci_credentials_union_release (creds_union); }

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_uint32 cci_cred_union_compare_to_credentials_union (const cred_union           *in_cred_union_compat,
                                                       const cc_credentials_union *in_credentials_union,
                                                       cc_uint32                  *out_equal)
{
    cc_int32 err = ccNoError;
    cc_uint32 equal = 0;

    if (!in_cred_union_compat) { err = cci_check_error (ccErrBadParam); }
    if (!in_credentials_union) { err = cci_check_error (ccErrBadParam); }
    if (!out_equal           ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        if (in_cred_union_compat->cred_type == CC_CRED_V4 &&
            in_credentials_union->version == cc_credentials_v4) {
            cc_credentials_v4_compat *old_creds_v4 = in_cred_union_compat->cred.pV4Cred;
            cc_credentials_v4_t *new_creds_v4 = in_credentials_union->credentials.credentials_v4;

            if (old_creds_v4 && new_creds_v4 &&
                !strcmp (old_creds_v4->principal,
                         new_creds_v4->principal) &&
                !strcmp (old_creds_v4->principal_instance,
                         new_creds_v4->principal_instance) &&
                !strcmp (old_creds_v4->service,
                         new_creds_v4->service) &&
                !strcmp (old_creds_v4->service_instance,
                         new_creds_v4->service_instance) &&
                !strcmp (old_creds_v4->realm, new_creds_v4->realm) &&
                (old_creds_v4->issue_date == (long) new_creds_v4->issue_date)) {
                equal = 1;
            }

        } else if (in_cred_union_compat->cred_type == CC_CRED_V5 &&
                   in_credentials_union->version == cc_credentials_v5) {
            cc_credentials_v5_compat *old_creds_v5 = in_cred_union_compat->cred.pV5Cred;
            cc_credentials_v5_t *new_creds_v5 = in_credentials_union->credentials.credentials_v5;

            /* Really should use krb5_parse_name and krb5_principal_compare */
            if (old_creds_v5 && new_creds_v5 &&
                !strcmp (old_creds_v5->client, new_creds_v5->client) &&
                !strcmp (old_creds_v5->server, new_creds_v5->server) &&
                (old_creds_v5->starttime == new_creds_v5->starttime)) {
                equal = 1;
            }
        }
    }

    if (!err) {
        *out_equal = equal;
    }

    return cci_check_error (err);
}
