/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * stdcc_util.c
 * utility functions used in implementing the ccache api for krb5
 * not publicly exported
 * Frank Dabek, July 1998
 */

#if defined(_WIN32) || defined(USE_CCAPI)

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32)
#include <malloc.h>
#endif

#include "stdcc_util.h"
#include "krb5.h"
#ifdef _WIN32                   /* it's part of krb5.h everywhere else */
#include "kv5m_err.h"
#endif

#define fieldSize 255

#ifdef USE_CCAPI_V3


static void
free_cc_array (cc_data **io_cc_array)
{
    if (io_cc_array) {
        unsigned int i;

        for (i = 0; io_cc_array[i]; i++) {
            if (io_cc_array[i]->data) { free (io_cc_array[i]->data); }
            free (io_cc_array[i]);
        }
        free (io_cc_array);
    }
}

static krb5_error_code
copy_cc_array_to_addresses (krb5_context in_context,
                            cc_data **in_cc_array,
                            krb5_address ***out_addresses)
{
    krb5_error_code err = 0;

    if (in_cc_array == NULL) {
        *out_addresses = NULL;

    } else {
        unsigned int count, i;
        krb5_address **addresses = NULL;

        /* get length of array */
        for (count = 0; in_cc_array[count]; count++);
        addresses = (krb5_address **) malloc (sizeof (*addresses) * (count + 1));
        if (!addresses) { err = KRB5_CC_NOMEM; }

        for (i = 0; !err && i < count; i++) {
            addresses[i] = (krb5_address *) malloc (sizeof (krb5_address));
            if (!addresses[i]) { err = KRB5_CC_NOMEM; }

            if (!err) {
                addresses[i]->contents = (krb5_octet *) malloc (sizeof (krb5_octet) *
                                                                in_cc_array[i]->length);
                if (!addresses[i]->contents) { err = KRB5_CC_NOMEM; }
            }

            if (!err) {
                addresses[i]->magic = KV5M_ADDRESS;
                addresses[i]->addrtype = in_cc_array[i]->type;
                addresses[i]->length = in_cc_array[i]->length;
                memcpy (addresses[i]->contents,
                        in_cc_array[i]->data, in_cc_array[i]->length);
            }
        }

        if (!err) {
            addresses[i] = NULL; /* terminator */
            *out_addresses = addresses;
            addresses = NULL;
        }

        if (addresses) { krb5_free_addresses (in_context, addresses); }
    }

    return err;
}

static krb5_error_code
copy_cc_array_to_authdata (krb5_context in_context,
                           cc_data **in_cc_array,
                           krb5_authdata ***out_authdata)
{
    krb5_error_code err = 0;

    if (in_cc_array == NULL) {
        *out_authdata = NULL;

    } else {
        unsigned int count, i;
        krb5_authdata **authdata = NULL;

        /* get length of array */
        for (count = 0; in_cc_array[count]; count++);
        authdata = (krb5_authdata **) malloc (sizeof (*authdata) * (count + 1));
        if (!authdata) { err = KRB5_CC_NOMEM; }

        for (i = 0; !err && i < count; i++) {
            authdata[i] = (krb5_authdata *) malloc (sizeof (krb5_authdata));
            if (!authdata[i]) { err = KRB5_CC_NOMEM; }

            if (!err) {
                authdata[i]->contents = (krb5_octet *) malloc (sizeof (krb5_octet) *
                                                               in_cc_array[i]->length);
                if (!authdata[i]->contents) { err = KRB5_CC_NOMEM; }
            }

            if (!err) {
                authdata[i]->magic = KV5M_AUTHDATA;
                authdata[i]->ad_type = in_cc_array[i]->type;
                authdata[i]->length = in_cc_array[i]->length;
                memcpy (authdata[i]->contents,
                        in_cc_array[i]->data, in_cc_array[i]->length);
            }
        }

        if (!err) {
            authdata[i] = NULL; /* terminator */
            *out_authdata = authdata;
            authdata = NULL;
        }

        if (authdata) { krb5_free_authdata (in_context, authdata); }
    }

    return err;
}

static krb5_error_code
copy_addresses_to_cc_array (krb5_context in_context,
                            krb5_address **in_addresses,
                            cc_data ***out_cc_array)
{
    krb5_error_code err = 0;

    if (in_addresses == NULL) {
        *out_cc_array = NULL;

    } else {
        unsigned int count, i;
        cc_data **cc_array = NULL;

        /* get length of array */
        for (count = 0; in_addresses[count]; count++);
        cc_array = (cc_data **) malloc (sizeof (*cc_array) * (count + 1));
        if (!cc_array) { err = KRB5_CC_NOMEM; }

        for (i = 0; !err && i < count; i++) {
            cc_array[i] = (cc_data *) malloc (sizeof (cc_data));
            if (!cc_array[i]) { err = KRB5_CC_NOMEM; }

            if (!err) {
                cc_array[i]->data = malloc (in_addresses[i]->length);
                if (!cc_array[i]->data) { err = KRB5_CC_NOMEM; }
            }

            if (!err) {
                cc_array[i]->type = in_addresses[i]->addrtype;
                cc_array[i]->length = in_addresses[i]->length;
                memcpy (cc_array[i]->data, in_addresses[i]->contents, in_addresses[i]->length);
            }
        }

        if (!err) {
            cc_array[i] = NULL; /* terminator */
            *out_cc_array = cc_array;
            cc_array = NULL;
        }

        if (cc_array) { free_cc_array (cc_array); }
    }


    return err;
}

static krb5_error_code
copy_authdata_to_cc_array (krb5_context in_context,
                           krb5_authdata **in_authdata,
                           cc_data ***out_cc_array)
{
    krb5_error_code err = 0;

    if (in_authdata == NULL) {
        *out_cc_array = NULL;

    } else {
        unsigned int count, i;
        cc_data **cc_array = NULL;

        /* get length of array */
        for (count = 0; in_authdata[count]; count++);
        cc_array = (cc_data **) malloc (sizeof (*cc_array) * (count + 1));
        if (!cc_array) { err = KRB5_CC_NOMEM; }

        for (i = 0; !err && i < count; i++) {
            cc_array[i] = (cc_data *) malloc (sizeof (cc_data));
            if (!cc_array[i]) { err = KRB5_CC_NOMEM; }

            if (!err) {
                cc_array[i]->data = malloc (in_authdata[i]->length);
                if (!cc_array[i]->data) { err = KRB5_CC_NOMEM; }
            }

            if (!err) {
                cc_array[i]->type = in_authdata[i]->ad_type;
                cc_array[i]->length = in_authdata[i]->length;
                memcpy (cc_array[i]->data, in_authdata[i]->contents, in_authdata[i]->length);
            }
        }

        if (!err) {
            cc_array[i] = NULL; /* terminator */
            *out_cc_array = cc_array;
            cc_array = NULL;
        }

        if (cc_array) { free_cc_array (cc_array); }
    }


    return err;
}


/*
 * copy_cc_credentials_to_krb5_creds
 * - allocate an empty k5 style ticket and copy info from the cc_creds ticket
 */

krb5_error_code
copy_cc_cred_union_to_krb5_creds (krb5_context in_context,
                                  const cc_credentials_union *in_cred_union,
                                  krb5_creds *out_creds)
{
    krb5_error_code err = 0;
    cc_credentials_v5_t *cv5 = NULL;
    krb5_int32 offset_seconds = 0, offset_microseconds = 0;
    krb5_principal client = NULL;
    krb5_principal server = NULL;
    char *ticket_data = NULL;
    char *second_ticket_data = NULL;
    unsigned char *keyblock_contents = NULL;
    krb5_address **addresses = NULL;
    krb5_authdata **authdata = NULL;

    if (in_cred_union->version != cc_credentials_v5) {
        err = KRB5_CC_NOT_KTYPE;
    } else {
        cv5 = in_cred_union->credentials.credentials_v5;
    }

#if TARGET_OS_MAC
    if (!err) {
        err = krb5_get_time_offsets (in_context, &offset_seconds, &offset_microseconds);
    }
#endif

    if (!err) {
        err = krb5_parse_name (in_context, cv5->client, &client);
    }

    if (!err) {
        err = krb5_parse_name (in_context, cv5->server, &server);
    }

    if (!err && cv5->keyblock.data) {
        keyblock_contents = (unsigned char *) malloc (cv5->keyblock.length);
        if (!keyblock_contents) { err = KRB5_CC_NOMEM; }
    }

    if (!err && cv5->ticket.data) {
        ticket_data = (char *) malloc (cv5->ticket.length);
        if (!ticket_data) { err = KRB5_CC_NOMEM; }
    }

    if (!err && cv5->second_ticket.data) {
        second_ticket_data = (char *) malloc (cv5->second_ticket.length);
        if (!second_ticket_data) { err = KRB5_CC_NOMEM; }
    }

    if (!err) {
        /* addresses */
        err = copy_cc_array_to_addresses (in_context, cv5->addresses, &addresses);
    }

    if (!err) {
        /* authdata */
        err = copy_cc_array_to_authdata (in_context, cv5->authdata, &authdata);
    }

    if (!err) {
        /* principals */
        out_creds->client = client;
        client = NULL;
        out_creds->server = server;
        server = NULL;

        /* copy keyblock */
        if (cv5->keyblock.data) {
            memcpy (keyblock_contents, cv5->keyblock.data, cv5->keyblock.length);
        }
        out_creds->keyblock.enctype = cv5->keyblock.type;
        out_creds->keyblock.length = cv5->keyblock.length;
        out_creds->keyblock.contents = keyblock_contents;
        keyblock_contents = NULL;

        /* copy times */
        out_creds->times.authtime   = cv5->authtime     + offset_seconds;
        out_creds->times.starttime  = cv5->starttime    + offset_seconds;
        out_creds->times.endtime    = cv5->endtime      + offset_seconds;
        out_creds->times.renew_till = cv5->renew_till   + offset_seconds;
        out_creds->is_skey          = cv5->is_skey;
        out_creds->ticket_flags     = cv5->ticket_flags;

        /* first ticket */
        if (cv5->ticket.data) {
            memcpy(ticket_data, cv5->ticket.data, cv5->ticket.length);
        }
        out_creds->ticket.length = cv5->ticket.length;
        out_creds->ticket.data = ticket_data;
        ticket_data = NULL;

        /* second ticket */
        if (cv5->second_ticket.data) {
            memcpy(second_ticket_data, cv5->second_ticket.data, cv5->second_ticket.length);
        }
        out_creds->second_ticket.length = cv5->second_ticket.length;
        out_creds->second_ticket.data = second_ticket_data;
        second_ticket_data = NULL;

        out_creds->addresses = addresses;
        addresses = NULL;

        out_creds->authdata = authdata;
        authdata = NULL;

        /* zero out magic number */
        out_creds->magic = 0;
    }

    if (addresses)          { krb5_free_addresses (in_context, addresses); }
    if (authdata)           { krb5_free_authdata (in_context, authdata); }
    if (keyblock_contents)  { free (keyblock_contents); }
    if (ticket_data)        { free (ticket_data); }
    if (second_ticket_data) { free (second_ticket_data); }
    if (client)             { krb5_free_principal (in_context, client); }
    if (server)             { krb5_free_principal (in_context, server); }

    return err;
}

/*
 * copy_krb5_creds_to_cc_credentials
 * - analagous to above but in the reverse direction
 */
krb5_error_code
copy_krb5_creds_to_cc_cred_union (krb5_context in_context,
                                  krb5_creds *in_creds,
                                  cc_credentials_union **out_cred_union)
{
    krb5_error_code err = 0;
    cc_credentials_union *cred_union = NULL;
    cc_credentials_v5_t *cv5 = NULL;
    char *client = NULL;
    char *server = NULL;
    unsigned char *ticket_data = NULL;
    unsigned char *second_ticket_data = NULL;
    unsigned char *keyblock_data = NULL;
    krb5_int32 offset_seconds = 0, offset_microseconds = 0;
    cc_data **cc_address_array = NULL;
    cc_data **cc_authdata_array = NULL;

    if (out_cred_union == NULL) { err = KRB5_CC_NOMEM; }

#if TARGET_OS_MAC
    if (!err) {
        err = krb5_get_time_offsets (in_context, &offset_seconds, &offset_microseconds);
    }
#endif

    if (!err) {
        cred_union = (cc_credentials_union *) malloc (sizeof (*cred_union));
        if (!cred_union) { err = KRB5_CC_NOMEM; }
    }

    if (!err) {
        cv5 = (cc_credentials_v5_t *) malloc (sizeof (*cv5));
        if (!cv5) { err = KRB5_CC_NOMEM; }
    }

    if (!err) {
        err = krb5_unparse_name (in_context, in_creds->client, &client);
    }

    if (!err) {
        err = krb5_unparse_name (in_context, in_creds->server, &server);
    }

    if (!err && in_creds->keyblock.contents) {
        keyblock_data = (unsigned char *) malloc (in_creds->keyblock.length);
        if (!keyblock_data) { err = KRB5_CC_NOMEM; }
    }

    if (!err && in_creds->ticket.data) {
        ticket_data = (unsigned char *) malloc (in_creds->ticket.length);
        if (!ticket_data) { err = KRB5_CC_NOMEM; }
    }

    if (!err && in_creds->second_ticket.data) {
        second_ticket_data = (unsigned char *) malloc (in_creds->second_ticket.length);
        if (!second_ticket_data) { err = KRB5_CC_NOMEM; }
    }

    if (!err) {
        err = copy_addresses_to_cc_array (in_context, in_creds->addresses, &cc_address_array);
    }

    if (!err) {
        err = copy_authdata_to_cc_array (in_context, in_creds->authdata, &cc_authdata_array);
    }

    if (!err) {
        /* principals */
        cv5->client = client;
        client = NULL;
        cv5->server = server;
        server = NULL;

        /* copy more fields */
        if (in_creds->keyblock.contents) {
            memcpy(keyblock_data, in_creds->keyblock.contents, in_creds->keyblock.length);
        }
        cv5->keyblock.type = in_creds->keyblock.enctype;
        cv5->keyblock.length = in_creds->keyblock.length;
        cv5->keyblock.data = keyblock_data;
        keyblock_data = NULL;

        cv5->authtime     = in_creds->times.authtime   - offset_seconds;
        cv5->starttime    = in_creds->times.starttime  - offset_seconds;
        cv5->endtime      = in_creds->times.endtime    - offset_seconds;
        cv5->renew_till   = in_creds->times.renew_till - offset_seconds;
        cv5->is_skey      = in_creds->is_skey;
        cv5->ticket_flags = in_creds->ticket_flags;

        if (in_creds->ticket.data) {
            memcpy (ticket_data, in_creds->ticket.data, in_creds->ticket.length);
        }
        cv5->ticket.length = in_creds->ticket.length;
        cv5->ticket.data = ticket_data;
        ticket_data = NULL;

        if (in_creds->second_ticket.data) {
            memcpy (second_ticket_data, in_creds->second_ticket.data, in_creds->second_ticket.length);
        }
        cv5->second_ticket.length = in_creds->second_ticket.length;
        cv5->second_ticket.data = second_ticket_data;
        second_ticket_data = NULL;

        cv5->addresses = cc_address_array;
        cc_address_array = NULL;

        cv5->authdata = cc_authdata_array;
        cc_authdata_array = NULL;

        /* Set up the structures to return to the caller */
        cred_union->version = cc_credentials_v5;
        cred_union->credentials.credentials_v5 = cv5;
        cv5 = NULL;

        *out_cred_union = cred_union;
        cred_union = NULL;
    }

    if (cc_address_array)   { free_cc_array (cc_address_array); }
    if (cc_authdata_array)  { free_cc_array (cc_authdata_array); }
    if (keyblock_data)      { free (keyblock_data); }
    if (ticket_data)        { free (ticket_data); }
    if (second_ticket_data) { free (second_ticket_data); }
    if (client)             { krb5_free_unparsed_name (in_context, client); }
    if (server)             { krb5_free_unparsed_name (in_context, server); }
    if (cv5)                { free (cv5); }
    if (cred_union)         { free (cred_union); }

    return err;
}

krb5_error_code
cred_union_release (cc_credentials_union *in_cred_union)
{
    if (in_cred_union) {
        if (in_cred_union->version == cc_credentials_v5 &&
            in_cred_union->credentials.credentials_v5) {
            cc_credentials_v5_t *cv5 = in_cred_union->credentials.credentials_v5;

            /* should use krb5_free_unparsed_name but we have no context */
            if (cv5->client) { free (cv5->client); }
            if (cv5->server) { free (cv5->server); }

            if (cv5->keyblock.data)      { free (cv5->keyblock.data); }
            if (cv5->ticket.data)        { free (cv5->ticket.data); }
            if (cv5->second_ticket.data) { free (cv5->second_ticket.data); }

            free_cc_array (cv5->addresses);
            free_cc_array (cv5->authdata);

            free (cv5);

        } else if (in_cred_union->version == cc_credentials_v4 &&
                   in_cred_union->credentials.credentials_v4) {
            free (in_cred_union->credentials.credentials_v4);
        }
        free ((cc_credentials_union *) in_cred_union);
    }

    return 0;
}

#else /* !USE_CCAPI_V3 */
/*
 * CopyCCDataArrayToK5
 * - copy and translate the null terminated arrays of data records
 *       used in k5 tickets
 */
int copyCCDataArrayToK5(cc_creds *ccCreds, krb5_creds *v5Creds, char whichArray) {

    if (whichArray == kAddressArray) {
        if (ccCreds->addresses == NULL) {
            v5Creds->addresses = NULL;
        } else {

            krb5_address        **addrPtr, *addr;
            cc_data                     **dataPtr, *data;
            unsigned int                numRecords = 0;

            /* Allocate the array of pointers: */
            for (dataPtr = ccCreds->addresses; *dataPtr != NULL; numRecords++, dataPtr++) {}

            v5Creds->addresses = (krb5_address **) malloc (sizeof(krb5_address *) * (numRecords + 1));
            if (v5Creds->addresses == NULL)
                return ENOMEM;

            /* Fill in the array, allocating the address structures: */
            for (dataPtr = ccCreds->addresses, addrPtr = v5Creds->addresses; *dataPtr != NULL; addrPtr++, dataPtr++) {

                *addrPtr = (krb5_address *) malloc (sizeof(krb5_address));
                if (*addrPtr == NULL)
                    return ENOMEM;
                data = *dataPtr;
                addr = *addrPtr;

                addr->addrtype = data->type;
                addr->magic    = KV5M_ADDRESS;
                addr->length   = data->length;
                addr->contents = (krb5_octet *) malloc (sizeof(krb5_octet) * addr->length);
                if (addr->contents == NULL)
                    return ENOMEM;
                memmove(addr->contents, data->data, addr->length); /* copy contents */
            }

            /* Write terminator: */
            *addrPtr = NULL;
        }
    }

    if (whichArray == kAuthDataArray) {
        if (ccCreds->authdata == NULL) {
            v5Creds->authdata = NULL;
        } else {
            krb5_authdata       **authPtr, *auth;
            cc_data                     **dataPtr, *data;
            unsigned int                numRecords = 0;

            /* Allocate the array of pointers: */
            for (dataPtr = ccCreds->authdata; *dataPtr != NULL; numRecords++, dataPtr++) {}

            v5Creds->authdata = (krb5_authdata **) malloc (sizeof(krb5_authdata *) * (numRecords + 1));
            if (v5Creds->authdata == NULL)
                return ENOMEM;

            /* Fill in the array, allocating the address structures: */
            for (dataPtr = ccCreds->authdata, authPtr = v5Creds->authdata; *dataPtr != NULL; authPtr++, dataPtr++) {

                *authPtr = (krb5_authdata *) malloc (sizeof(krb5_authdata));
                if (*authPtr == NULL)
                    return ENOMEM;
                data = *dataPtr;
                auth = *authPtr;

                auth->ad_type  = data->type;
                auth->magic    = KV5M_AUTHDATA;
                auth->length   = data->length;
                auth->contents = (krb5_octet *) malloc (sizeof(krb5_octet) * auth->length);
                if (auth->contents == NULL)
                    return ENOMEM;
                memmove(auth->contents, data->data, auth->length); /* copy contents */
            }

            /* Write terminator: */
            *authPtr = NULL;
        }
    }

    return 0;
}

/*
 * copyK5DataArrayToCC
 * - analagous to above, but in the other direction
 */
int copyK5DataArrayToCC(krb5_creds *v5Creds, cc_creds *ccCreds, char whichArray)
{
    if (whichArray == kAddressArray) {
        if (v5Creds->addresses == NULL) {
            ccCreds->addresses = NULL;
        } else {

            krb5_address        **addrPtr, *addr;
            cc_data                     **dataPtr, *data;
            unsigned int                        numRecords = 0;

            /* Allocate the array of pointers: */
            for (addrPtr = v5Creds->addresses; *addrPtr != NULL; numRecords++, addrPtr++) {}

            ccCreds->addresses = (cc_data **) malloc (sizeof(cc_data *) * (numRecords + 1));
            if (ccCreds->addresses == NULL)
                return ENOMEM;

            /* Fill in the array, allocating the address structures: */
            for (dataPtr = ccCreds->addresses, addrPtr = v5Creds->addresses; *addrPtr != NULL; addrPtr++, dataPtr++) {

                *dataPtr = (cc_data *) malloc (sizeof(cc_data));
                if (*dataPtr == NULL)
                    return ENOMEM;
                data = *dataPtr;
                addr = *addrPtr;

                data->type   = addr->addrtype;
                data->length = addr->length;
                data->data   = malloc (sizeof(char) * data->length);
                if (data->data == NULL)
                    return ENOMEM;
                memmove(data->data, addr->contents, data->length); /* copy contents */
            }

            /* Write terminator: */
            *dataPtr = NULL;
        }
    }

    if (whichArray == kAuthDataArray) {
        if (v5Creds->authdata == NULL) {
            ccCreds->authdata = NULL;
        } else {
            krb5_authdata       **authPtr, *auth;
            cc_data                     **dataPtr, *data;
            unsigned int                        numRecords = 0;

            /* Allocate the array of pointers: */
            for (authPtr = v5Creds->authdata; *authPtr != NULL; numRecords++, authPtr++) {}

            ccCreds->authdata = (cc_data **) malloc (sizeof(cc_data *) * (numRecords + 1));
            if (ccCreds->authdata == NULL)
                return ENOMEM;

            /* Fill in the array, allocating the address structures: */
            for (dataPtr = ccCreds->authdata, authPtr = v5Creds->authdata; *authPtr != NULL; authPtr++, dataPtr++) {

                *dataPtr = (cc_data *) malloc (sizeof(cc_data));
                if (*dataPtr == NULL)
                    return ENOMEM;
                data = *dataPtr;
                auth = *authPtr;

                data->type   = auth->ad_type;
                data->length = auth->length;
                data->data   = malloc (sizeof(char) * data->length);
                if (data->data == NULL)
                    return ENOMEM;
                memmove(data->data, auth->contents, data->length); /* copy contents */
            }

            /* Write terminator: */
            *dataPtr = NULL;
        }
    }

    return 0;
}

/*
 * dupcctok5
 * - allocate an empty k5 style ticket and copy info from the cc_creds ticket
 */

void dupCCtoK5(krb5_context context, cc_creds *src, krb5_creds *dest)
{
    krb5_int32 offset_seconds = 0, offset_microseconds = 0;
    int err;

    /*
     * allocate and copy
     * copy all of those damn fields back
     */
    err = krb5_parse_name(context, src->client, &(dest->client));
    err = krb5_parse_name(context, src->server, &(dest->server));
    if (err) return; /* parsename fails w/o krb5.ini for example */

    /* copy keyblock */
    dest->keyblock.enctype = src->keyblock.type;
    dest->keyblock.length = src->keyblock.length;
    dest->keyblock.contents = (krb5_octet *)malloc(dest->keyblock.length);
    memcpy(dest->keyblock.contents, src->keyblock.data, dest->keyblock.length);

    /* copy times */
#if TARGET_OS_MAC
    err = krb5_get_time_offsets(context, &offset_seconds, &offset_microseconds);
    if (err) return;
#endif
    dest->times.authtime   = src->authtime     + offset_seconds;
    dest->times.starttime  = src->starttime    + offset_seconds;
    dest->times.endtime    = src->endtime      + offset_seconds;
    dest->times.renew_till = src->renew_till   + offset_seconds;
    dest->is_skey          = src->is_skey;
    dest->ticket_flags     = src->ticket_flags;

    /* more branching fields */
    err = copyCCDataArrayToK5(src, dest, kAddressArray);
    if (err) return;

    dest->ticket.length = src->ticket.length;
    dest->ticket.data = (char *)malloc(src->ticket.length);
    memcpy(dest->ticket.data, src->ticket.data, src->ticket.length);
    dest->second_ticket.length = src->second_ticket.length;
    (dest->second_ticket).data = ( char *)malloc(src->second_ticket.length);
    memcpy(dest->second_ticket.data, src->second_ticket.data, src->second_ticket.length);

    /* zero out magic number */
    dest->magic = 0;

    /* authdata */
    err = copyCCDataArrayToK5(src, dest, kAuthDataArray);
    if (err) return;

    return;
}

/*
 * dupK5toCC
 * - analagous to above but in the reverse direction
 */
void dupK5toCC(krb5_context context, krb5_creds *creds, cred_union **cu)
{
    cc_creds *c;
    int err;
    krb5_int32 offset_seconds = 0, offset_microseconds = 0;

    if (cu == NULL) return;

    /* allocate the cred_union */
    *cu = (cred_union *)malloc(sizeof(cred_union));
    if ((*cu) == NULL)
        return;

    (*cu)->cred_type = CC_CRED_V5;

    /* allocate creds structure (and install) */
    c  = (cc_creds *)malloc(sizeof(cc_creds));
    if (c == NULL) return;
    (*cu)->cred.pV5Cred = c;

    /* convert krb5 principals to flat principals */
    err = krb5_unparse_name(context, creds->client, &(c->client));
    err = krb5_unparse_name(context, creds->server, &(c->server));
    if (err) return;

    /* copy more fields */
    c->keyblock.type = creds->keyblock.enctype;
    c->keyblock.length = creds->keyblock.length;

    if (creds->keyblock.contents != NULL) {
        c->keyblock.data = (unsigned char *)malloc(creds->keyblock.length);
        memcpy(c->keyblock.data, creds->keyblock.contents, creds->keyblock.length);
    } else {
        c->keyblock.data = NULL;
    }

#if TARGET_OS_MAC
    err = krb5_get_time_offsets(context, &offset_seconds, &offset_microseconds);
    if (err) return;
#endif
    c->authtime     = creds->times.authtime   - offset_seconds;
    c->starttime    = creds->times.starttime  - offset_seconds;
    c->endtime      = creds->times.endtime    - offset_seconds;
    c->renew_till   = creds->times.renew_till - offset_seconds;
    c->is_skey      = creds->is_skey;
    c->ticket_flags = creds->ticket_flags;

    err = copyK5DataArrayToCC(creds, c, kAddressArray);
    if (err) return;

    c->ticket.length = creds->ticket.length;
    if (creds->ticket.data != NULL) {
        c->ticket.data = (unsigned char *)malloc(creds->ticket.length);
        memcpy(c->ticket.data, creds->ticket.data, creds->ticket.length);
    } else {
        c->ticket.data = NULL;
    }

    c->second_ticket.length = creds->second_ticket.length;
    if (creds->second_ticket.data != NULL) {
        c->second_ticket.data = (unsigned char *)malloc(creds->second_ticket.length);
        memcpy(c->second_ticket.data, creds->second_ticket.data, creds->second_ticket.length);
    } else {
        c->second_ticket.data = NULL;
    }

    err = copyK5DataArrayToCC(creds, c, kAuthDataArray);
    if (err) return;

    return;
}

/* ----- free_cc_cred_union, etc -------------- */
/*
  Since the Kerberos5 library allocates a credentials cache structure
  (in dupK5toCC() above) with its own memory allocation routines - which
  may be different than how the CCache allocates memory - the Kerb5 library
  must have its own version of cc_free_creds() to deallocate it.  These
  functions do that.  The top-level function to substitue for cc_free_creds()
  is krb5_free_cc_cred_union().

  If the CCache library wants to use a cred_union structure created by
  the Kerb5 library, it should make a deep copy of it to "translate" to its
  own memory allocation space.
*/
static void deep_free_cc_data (cc_data data)
{
    if (data.data != NULL)
        free (data.data);
}

static void deep_free_cc_data_array (cc_data** data) {

    unsigned int i;

    if (data == NULL)
        return;

    for (i = 0; data [i] != NULL; i++) {
        deep_free_cc_data (*(data [i]));
        free (data [i]);
    }

    free (data);
}

static void deep_free_cc_v5_creds (cc_creds* creds)
{
    if (creds == NULL)
        return;

    if (creds -> client != NULL)
        free (creds -> client);
    if (creds -> server != NULL)
        free (creds -> server);

    deep_free_cc_data (creds -> keyblock);
    deep_free_cc_data (creds -> ticket);
    deep_free_cc_data (creds -> second_ticket);

    deep_free_cc_data_array (creds -> addresses);
    deep_free_cc_data_array (creds -> authdata);

    free(creds);
}

static void deep_free_cc_creds (cred_union creds)
{
    if (creds.cred_type == CC_CRED_V4) {
        /* we shouldn't get this, of course */
        free (creds.cred.pV4Cred);
    } else if (creds.cred_type == CC_CRED_V5) {
        deep_free_cc_v5_creds (creds.cred.pV5Cred);
    }
}

/* top-level exported function */
cc_int32 krb5int_free_cc_cred_union (cred_union** creds)
{
    if (creds == NULL)
        return CC_BAD_PARM;

    if (*creds != NULL) {
        deep_free_cc_creds (**creds);
        free (*creds);
        *creds = NULL;
    }

    return CC_NOERROR;
}
#endif

/*
 * Utility functions...
 */
static krb5_boolean
times_match(t1, t2)
    register const krb5_ticket_times *t1;
    register const krb5_ticket_times *t2;
{
    if (t1->renew_till) {
        if (t1->renew_till > t2->renew_till)
            return FALSE;               /* this one expires too late */
    }
    if (t1->endtime) {
        if (t1->endtime > t2->endtime)
            return FALSE;               /* this one expires too late */
    }
    /* only care about expiration on a times_match */
    return TRUE;
}

static krb5_boolean
times_match_exact (t1, t2)
    register const krb5_ticket_times *t1, *t2;
{
    return (t1->authtime == t2->authtime
            && t1->starttime == t2->starttime
            && t1->endtime == t2->endtime
            && t1->renew_till == t2->renew_till);
}

static krb5_boolean
standard_fields_match(context, mcreds, creds)
    krb5_context context;
    register const krb5_creds *mcreds, *creds;
{
    return (krb5_principal_compare(context, mcreds->client,creds->client) &&
            krb5_principal_compare(context, mcreds->server,creds->server));
}

/* only match the server name portion, not the server realm portion */

static krb5_boolean
srvname_match(context, mcreds, creds)
    krb5_context context;
    register const krb5_creds *mcreds, *creds;
{
    krb5_boolean retval;
    krb5_principal_data p1, p2;

    retval = krb5_principal_compare(context, mcreds->client,creds->client);
    if (retval != TRUE)
        return retval;
    /*
     * Hack to ignore the server realm for the purposes of the compare.
     */
    p1 = *mcreds->server;
    p2 = *creds->server;
    p1.realm = p2.realm;
    return krb5_principal_compare(context, &p1, &p2);
}


static krb5_boolean
authdata_match(mdata, data)
    krb5_authdata *const *mdata, *const *data;
{
    const krb5_authdata *mdatap, *datap;

    if (mdata == data)
        return TRUE;

    if (mdata == NULL)
        return *data == NULL;

    if (data == NULL)
        return *mdata == NULL;

    while ((mdatap = *mdata)
           && (datap = *data)
           && mdatap->ad_type == datap->ad_type
           && mdatap->length == datap->length
           && !memcmp ((char *) mdatap->contents, (char *) datap->contents,
                       datap->length)) {
        mdata++;
        data++;
    }

    return !*mdata && !*data;
}

static krb5_boolean
data_match(data1, data2)
    register const krb5_data *data1, *data2;
{
    if (!data1) {
        if (!data2)
            return TRUE;
        else
            return FALSE;
    }
    if (!data2) return FALSE;

    if (data1->length != data2->length)
        return FALSE;
    else
        return memcmp(data1->data, data2->data, data1->length) ? FALSE : TRUE;
}

#define MATCH_SET(bits) (whichfields & bits)
#define flags_match(a,b) (((a) & (b)) == (a))

/*  stdccCredsMatch
 *  - check to see if the creds match based on the whichFields variable
 *  NOTE: if whichfields is zero we are now comparing 'standard fields.'
 *               This is the bug that was killing fetch for a
 *               week. The behaviour is what krb5 expects, however.
 */
int stdccCredsMatch(krb5_context context, krb5_creds *base,
                    krb5_creds *match, int whichfields)
{
    if (((MATCH_SET(KRB5_TC_MATCH_SRV_NAMEONLY) &&
          srvname_match(context, match, base)) ||
         standard_fields_match(context, match, base))
        &&
        (! MATCH_SET(KRB5_TC_MATCH_IS_SKEY) ||
         match->is_skey == base->is_skey)
        &&
        (! MATCH_SET(KRB5_TC_MATCH_FLAGS_EXACT) ||
         match->ticket_flags == base->ticket_flags)
        &&
        (! MATCH_SET(KRB5_TC_MATCH_FLAGS) ||
         flags_match(match->ticket_flags, base->ticket_flags))
        &&
        (! MATCH_SET(KRB5_TC_MATCH_TIMES_EXACT) ||
         times_match_exact(&match->times, &base->times))
        &&
        (! MATCH_SET(KRB5_TC_MATCH_TIMES) ||
         times_match(&match->times, &base->times))
        &&
        (! MATCH_SET(KRB5_TC_MATCH_AUTHDATA) ||
         authdata_match (match->authdata, base->authdata))
        &&
        (! MATCH_SET(KRB5_TC_MATCH_2ND_TKT) ||
         data_match (&match->second_ticket, &base->second_ticket))
        &&
        ((! MATCH_SET(KRB5_TC_MATCH_KTYPE))||
         (match->keyblock.enctype == base->keyblock.enctype))
    )
        return TRUE;
    return FALSE;
}

#endif /* defined(_WIN32) || defined(USE_CCAPI) */
