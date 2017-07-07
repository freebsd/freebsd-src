#include "kdb_ldap.h"
#include "ldap_principal.h"
#include "princ_xdr.h"
#include <kadm5/admin.h>

bool_t
ldap_xdr_krb5_ui_2(XDR *xdrs, krb5_ui_2 *objp)
{
    unsigned int tmp;

    tmp = (unsigned int) *objp;

    if (!xdr_u_int(xdrs, &tmp))
	return(FALSE);

    *objp = (krb5_ui_2) tmp;
    return(TRUE);
}

bool_t
ldap_xdr_krb5_int16(XDR *xdrs, krb5_int16 *objp)
{
    int tmp;

    tmp = (int) *objp;

    if (!xdr_int(xdrs, &tmp))
	return(FALSE);

    *objp = (krb5_int16) tmp;
    return(TRUE);
}

bool_t
ldap_xdr_nullstring(XDR *xdrs, char **objp)
{
    u_int size;

    if (xdrs->x_op == XDR_ENCODE) {
	if (*objp == NULL)
	    size = 0;
	else
	    size = strlen(*objp) + 1;
    }
    if (! xdr_u_int(xdrs, &size)) {
	return FALSE;
    }
    switch (xdrs->x_op) {
    case XDR_DECODE:
	if (size == 0) {
	    *objp = NULL;
	    return TRUE;
	} else if (*objp == NULL) {
	    *objp = (char *) mem_alloc(size);
	    if (*objp == NULL) {
		/*errno = ENOMEM;*/
		return FALSE;
	    }
	}
	return (xdr_opaque(xdrs, *objp, size));

    case XDR_ENCODE:
	if (size != 0)
	    return (xdr_opaque(xdrs, *objp, size));
	return TRUE;

    case XDR_FREE:
	if (*objp != NULL)
	    mem_free(*objp, size);
	*objp = NULL;
	return TRUE;
    }
    return FALSE;
}

bool_t
ldap_xdr_krb5_kvno(XDR *xdrs, krb5_kvno *objp)
{
    unsigned char tmp;

    tmp = '\0'; /* for purify, else xdr_u_char performs a umr */

    if (xdrs->x_op == XDR_ENCODE)
	tmp = (unsigned char) *objp;

    if (!xdr_u_char(xdrs, &tmp))
	return (FALSE);

    if (xdrs->x_op == XDR_DECODE)
	*objp = (krb5_kvno) tmp;
    return (TRUE);
}

bool_t
ldap_xdr_krb5_key_data(XDR *xdrs, krb5_key_data *objp)
{
    unsigned int tmp;

    if (!ldap_xdr_krb5_int16(xdrs, &objp->key_data_ver))
	return(FALSE);
    if (!ldap_xdr_krb5_ui_2(xdrs, &objp->key_data_kvno))
	return(FALSE);
    if (!ldap_xdr_krb5_int16(xdrs, &objp->key_data_type[0]))
	return(FALSE);
    if (!ldap_xdr_krb5_int16(xdrs, &objp->key_data_type[1]))
	return(FALSE);
    if (!ldap_xdr_krb5_ui_2(xdrs, &objp->key_data_length[0]))
	return(FALSE);
    if (!ldap_xdr_krb5_ui_2(xdrs, &objp->key_data_length[1]))
	return(FALSE);

    tmp = (unsigned int) objp->key_data_length[0];
    if (!xdr_bytes(xdrs, (char **) &objp->key_data_contents[0],
		   &tmp, (unsigned int) ~0))
	return FALSE;

    tmp = (unsigned int) objp->key_data_length[1];
    if (!xdr_bytes(xdrs, (char **) &objp->key_data_contents[1],
		   &tmp, (unsigned int) ~0))
	return FALSE;

    /* don't need to copy tmp out, since key_data_length will be set
       by the above encoding. */
    return(TRUE);
}

bool_t
ldap_xdr_osa_pw_hist_ent(XDR *xdrs, osa_pw_hist_ent *objp)
{
    if (!xdr_array(xdrs, (caddr_t *) &objp->key_data,
		   (u_int *) &objp->n_key_data, (unsigned int) ~0,
		   sizeof(krb5_key_data),
		   ldap_xdr_krb5_key_data))
	return (FALSE);
    return (TRUE);
}

bool_t
ldap_xdr_osa_princ_ent_rec(XDR *xdrs, osa_princ_ent_t objp)
{
    switch (xdrs->x_op) {
    case XDR_ENCODE:
	objp->version = OSA_ADB_PRINC_VERSION_1;
	/* fall through */
    case XDR_FREE:
	if (!xdr_int(xdrs, &objp->version))
	    return FALSE;
	break;
    case XDR_DECODE:
	if (!xdr_int(xdrs, &objp->version))
	    return FALSE;
	if (objp->version != OSA_ADB_PRINC_VERSION_1)
	    return FALSE;
	break;
    }

    if (!ldap_xdr_nullstring(xdrs, &objp->policy))
	return (FALSE);
    if (!xdr_long(xdrs, &objp->aux_attributes))
	return (FALSE);
    if (!xdr_u_int(xdrs, &objp->old_key_next))
	return (FALSE);
    if (!ldap_xdr_krb5_kvno(xdrs, &objp->admin_history_kvno))
	return (FALSE);
    if (!xdr_array(xdrs, (caddr_t *) &objp->old_keys,
		   (unsigned int *) &objp->old_key_len, (unsigned int) ~0,
		   sizeof(osa_pw_hist_ent),
		   ldap_xdr_osa_pw_hist_ent))
	return (FALSE);
    return (TRUE);
}

void
ldap_osa_free_princ_ent(osa_princ_ent_t val)
{
    XDR xdrs;

    xdrmem_create(&xdrs, NULL, 0, XDR_FREE);
    ldap_xdr_osa_princ_ent_rec(&xdrs, val);
    xdr_destroy(&xdrs);
}

krb5_error_code
krb5_lookup_tl_kadm_data(krb5_tl_data *tl_data, osa_princ_ent_rec *princ_entry)
{

    XDR xdrs;

    xdrmem_create(&xdrs, (caddr_t)tl_data->tl_data_contents,
		  tl_data->tl_data_length, XDR_DECODE);
    if (! ldap_xdr_osa_princ_ent_rec(&xdrs, princ_entry)) {
	xdr_destroy(&xdrs);
	return(KADM5_XDR_FAILURE);
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
    if (! ldap_xdr_osa_princ_ent_rec(&xdrs, princ_entry)) {
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
