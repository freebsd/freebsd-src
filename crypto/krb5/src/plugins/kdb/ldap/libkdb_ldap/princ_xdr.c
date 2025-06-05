/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "kdb_ldap.h"
#include "ldap_principal.h"
#include "princ_xdr.h"
#include <kadm5/admin.h>
#include <kadm5/server_internal.h>

void
ldap_osa_free_princ_ent(osa_princ_ent_t val)
{
    XDR xdrs;

    xdrmem_create(&xdrs, NULL, 0, XDR_FREE);
    xdr_osa_princ_ent_rec(&xdrs, val);
    xdr_destroy(&xdrs);
}

krb5_error_code
krb5_lookup_tl_kadm_data(krb5_tl_data *tl_data, osa_princ_ent_rec *princ_entry)
{

    XDR xdrs;

    xdrmem_create(&xdrs, (caddr_t)tl_data->tl_data_contents,
                  tl_data->tl_data_length, XDR_DECODE);
    if (!xdr_osa_princ_ent_rec(&xdrs, princ_entry)) {
        xdr_destroy(&xdrs);
        return KADM5_XDR_FAILURE;
    }
    xdr_destroy(&xdrs);

    return 0;
}

krb5_error_code
krb5_update_tl_kadm_data(krb5_context context, krb5_db_entry *entry,
                         osa_princ_ent_rec *princ_entry)
{
    XDR xdrs;
    krb5_tl_data tl_data;
    krb5_error_code retval;

    xdralloc_create(&xdrs, XDR_ENCODE);
    if (!xdr_osa_princ_ent_rec(&xdrs, princ_entry)) {
        xdr_destroy(&xdrs);
        return KADM5_XDR_FAILURE;
    }
    tl_data.tl_data_type = KRB5_TL_KADM_DATA;
    tl_data.tl_data_length = xdr_getpos(&xdrs);
    tl_data.tl_data_contents = (krb5_octet *)xdralloc_getdata(&xdrs);
    retval = krb5_dbe_update_tl_data(context, entry, &tl_data);
    xdr_destroy(&xdrs);
    return retval;
}
