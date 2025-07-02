/* -*- mode: c; c-file-style: "bsd"; indent-tabs-mode: t -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 */

#include <gssrpc/rpc.h>
#include <krb5.h>
#include <errno.h>
#include <kadm5/admin.h>
#include <kadm5/kadm_rpc.h>
#include <kadm5/admin_xdr.h>
#include <stdlib.h>
#include <string.h>

static bool_t
_xdr_kadm5_principal_ent_rec(XDR *xdrs, kadm5_principal_ent_rec *objp,
			     int v);
static bool_t
_xdr_kadm5_policy_ent_rec(XDR *xdrs, kadm5_policy_ent_rec *objp, int vers);

/*
 * Function: xdr_ui_4
 *
 * Purpose: XDR function which serves as a wrapper for xdr_u_int32,
 * to prevent compiler warnings about type clashes between u_int32
 * and krb5_ui_4.
 */
bool_t xdr_ui_4(XDR *xdrs, krb5_ui_4 *objp)
{
  /* Assumes that krb5_ui_4 and u_int32 are both four bytes long.
     This should not be a harmful assumption. */
  return xdr_u_int32(xdrs, (uint32_t *) objp);
}


/*
 * Function: xdr_nullstring
 *
 * Purpose: XDR function for "strings" that are either NULL-terminated
 * or NULL.
 */
bool_t xdr_nullstring(XDR *xdrs, char **objp)
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
		    errno = ENOMEM;
		    return FALSE;
	       }
	  }
	  if (!xdr_opaque(xdrs, *objp, size))
		  return FALSE;
	  /* Check that the unmarshalled bytes are a C string. */
	  if ((*objp)[size - 1] != '\0')
		  return FALSE;
	  if (memchr(*objp, '\0', size - 1) != NULL)
		  return FALSE;
	  return TRUE;

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

/*
 * Function: xdr_nulltype
 *
 * Purpose: XDR function for arbitrary pointer types that are either
 * NULL or contain data.
 */
bool_t xdr_nulltype(XDR *xdrs, void **objp, xdrproc_t proc)
{
     bool_t null;

     switch (xdrs->x_op) {
     case XDR_DECODE:
	  if (!xdr_bool(xdrs, &null))
	      return FALSE;
	  if (null) {
	       *objp = NULL;
	       return TRUE;
	  }
	  return (*proc)(xdrs, objp);

     case XDR_ENCODE:
	  if (*objp == NULL)
	       null = TRUE;
	  else
	       null = FALSE;
	  if (!xdr_bool(xdrs, &null))
	       return FALSE;
	  if (null == FALSE)
	       return (*proc)(xdrs, objp);
	  return TRUE;

     case XDR_FREE:
	  if (*objp)
	       return (*proc)(xdrs, objp);
	  return TRUE;
     }

     return FALSE;
}

bool_t
xdr_krb5_timestamp(XDR *xdrs, krb5_timestamp *objp)
{
  /* This assumes that int32 and krb5_timestamp are the same size.
     This shouldn't be a problem, since we've got a unit test which
     checks for this. */
	if (!xdr_int32(xdrs, (int32_t *) objp)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_krb5_kvno(XDR *xdrs, krb5_kvno *objp)
{
	return xdr_u_int(xdrs, objp);
}

bool_t
xdr_krb5_deltat(XDR *xdrs, krb5_deltat *objp)
{
  /* This assumes that int32 and krb5_deltat are the same size.
     This shouldn't be a problem, since we've got a unit test which
     checks for this. */
	if (!xdr_int32(xdrs, (int32_t *) objp)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_krb5_flags(XDR *xdrs, krb5_flags *objp)
{
  /* This assumes that int32 and krb5_flags are the same size.
     This shouldn't be a problem, since we've got a unit test which
     checks for this. */
	if (!xdr_int32(xdrs, (int32_t *) objp)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_krb5_ui_4(XDR *xdrs, krb5_ui_4 *objp)
{
	if (!xdr_u_int32(xdrs, (uint32_t *) objp)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_krb5_int16(XDR *xdrs, krb5_int16 *objp)
{
    int tmp;

    tmp = (int) *objp;

    if (!xdr_int(xdrs, &tmp))
	return(FALSE);

    *objp = (krb5_int16) tmp;

    return(TRUE);
}

/*
 * Function: xdr_krb5_ui_2
 *
 * Purpose: XDR function which serves as a wrapper for xdr_u_int,
 * to prevent compiler warnings about type clashes between u_int
 * and krb5_ui_2.
 */
bool_t
xdr_krb5_ui_2(XDR *xdrs, krb5_ui_2 *objp)
{
    unsigned int tmp;

    tmp = (unsigned int) *objp;

    if (!xdr_u_int(xdrs, &tmp))
	return(FALSE);

    *objp = (krb5_ui_2) tmp;

    return(TRUE);
}



static bool_t xdr_krb5_boolean(XDR *xdrs, krb5_boolean *kbool)
{
	bool_t val;

	switch (xdrs->x_op) {
	case XDR_DECODE:
	     if (!xdr_bool(xdrs, &val))
		     return FALSE;

	     *kbool = (val == FALSE) ? FALSE : TRUE;
	     return TRUE;

	case XDR_ENCODE:
	     val = *kbool ? TRUE : FALSE;
	     return xdr_bool(xdrs, &val);

	case XDR_FREE:
	     return TRUE;
	}

	return FALSE;
}

bool_t xdr_krb5_key_data_nocontents(XDR *xdrs, krb5_key_data *objp)
{
     /*
      * Note that this function intentionally DOES NOT transfer key
      * length or contents!  xdr_krb5_key_data in adb_xdr.c does, but
      * that is only for use within the server-side library.
      */
     unsigned int tmp;

     if (xdrs->x_op == XDR_DECODE)
	  memset(objp, 0, sizeof(krb5_key_data));

     if (!xdr_krb5_int16(xdrs, &objp->key_data_ver)) {
	  return (FALSE);
     }
     if (!xdr_krb5_ui_2(xdrs, &objp->key_data_kvno)) {
	  return (FALSE);
     }
     if (!xdr_krb5_int16(xdrs, &objp->key_data_type[0])) {
	  return (FALSE);
     }
     if (objp->key_data_ver > 1) {
	  if (!xdr_krb5_int16(xdrs, &objp->key_data_type[1])) {
	       return (FALSE);
	  }
     }
     /*
      * kadm5_get_principal on the server side allocates and returns
      * key contents when asked.  Even though this function refuses to
      * transmit that data, it still has to *free* the data at the
      * appropriate time to avoid a memory leak.
      */
     if (xdrs->x_op == XDR_FREE) {
	  tmp = (unsigned int) objp->key_data_length[0];
	  if (!xdr_bytes(xdrs, (char **) &objp->key_data_contents[0],
			 &tmp, ~0))
	       return FALSE;

	  tmp = (unsigned int) objp->key_data_length[1];
	  if (!xdr_bytes(xdrs, (char **) &objp->key_data_contents[1],
			 &tmp, ~0))
	       return FALSE;
     }

     return (TRUE);
}


bool_t
xdr_krb5_key_salt_tuple(XDR *xdrs, krb5_key_salt_tuple *objp)
{
    if (!xdr_krb5_enctype(xdrs, &objp->ks_enctype))
	return FALSE;
    if (!xdr_krb5_salttype(xdrs, &objp->ks_salttype))
	return FALSE;
    return TRUE;
}

bool_t xdr_krb5_tl_data(XDR *xdrs, krb5_tl_data **tl_data_head)
{
     krb5_tl_data *tl, *tl2;
     bool_t more;
     unsigned int len;

     switch (xdrs->x_op) {
     case XDR_FREE:
	  tl = tl2 = *tl_data_head;
	  while (tl) {
	       tl2 = tl->tl_data_next;
	       free(tl->tl_data_contents);
	       free(tl);
	       tl = tl2;
	  }
	  *tl_data_head = NULL;
	  break;

     case XDR_ENCODE:
	  tl = *tl_data_head;
	  while (1) {
	       more = (tl != NULL);
	       if (!xdr_bool(xdrs, &more))
		    return FALSE;
	       if (tl == NULL)
		    break;
	       if (!xdr_krb5_int16(xdrs, &tl->tl_data_type))
		    return FALSE;
	       len = tl->tl_data_length;
	       if (!xdr_bytes(xdrs, (char **) &tl->tl_data_contents, &len, ~0))
		    return FALSE;
	       tl = tl->tl_data_next;
	  }
	  break;

     case XDR_DECODE:
	  tl = NULL;
	  while (1) {
	       if (!xdr_bool(xdrs, &more))
		    return FALSE;
	       if (more == FALSE)
		    break;
	       tl2 = (krb5_tl_data *) malloc(sizeof(krb5_tl_data));
	       if (tl2 == NULL)
		    return FALSE;
	       memset(tl2, 0, sizeof(krb5_tl_data));
	       if (!xdr_krb5_int16(xdrs, &tl2->tl_data_type))
		    return FALSE;
	       if (!xdr_bytes(xdrs, (char **)&tl2->tl_data_contents, &len, ~0))
		    return FALSE;
	       tl2->tl_data_length = len;

	       tl2->tl_data_next = tl;
	       tl = tl2;
	  }

	  *tl_data_head = tl;
	  break;
     }

     return TRUE;
}

bool_t
xdr_kadm5_ret_t(XDR *xdrs, kadm5_ret_t *objp)
{
	uint32_t tmp;

	if (xdrs->x_op == XDR_ENCODE)
		tmp = (uint32_t) *objp;

	if (!xdr_u_int32(xdrs, &tmp))
		return (FALSE);

	if (xdrs->x_op == XDR_DECODE)
		*objp = (kadm5_ret_t) tmp;

	return (TRUE);
}

bool_t xdr_kadm5_principal_ent_rec(XDR *xdrs,
				   kadm5_principal_ent_rec *objp)
{
     return _xdr_kadm5_principal_ent_rec(xdrs, objp, KADM5_API_VERSION_3);
}

static bool_t
_xdr_kadm5_principal_ent_rec(XDR *xdrs, kadm5_principal_ent_rec *objp,
			     int v)
{
	unsigned int n;
	bool_t r;

	if (!xdr_krb5_principal(xdrs, &objp->principal)) {
		return (FALSE);
	}
	if (!xdr_krb5_timestamp(xdrs, &objp->princ_expire_time)) {
		return (FALSE);
	}
	if (!xdr_krb5_timestamp(xdrs, &objp->last_pwd_change)) {
		return (FALSE);
	}
	if (!xdr_krb5_timestamp(xdrs, &objp->pw_expiration)) {
		return (FALSE);
	}
	if (!xdr_krb5_deltat(xdrs, &objp->max_life)) {
		return (FALSE);
	}
	if (!xdr_nulltype(xdrs, (void **) &objp->mod_name,
			  xdr_krb5_principal)) {
		return (FALSE);
	}
	if (!xdr_krb5_timestamp(xdrs, &objp->mod_date)) {
		return (FALSE);
	}
	if (!xdr_krb5_flags(xdrs, &objp->attributes)) {
		return (FALSE);
	}
	if (!xdr_krb5_kvno(xdrs, &objp->kvno)) {
		return (FALSE);
	}
	if (!xdr_krb5_kvno(xdrs, &objp->mkvno)) {
		return (FALSE);
	}
	if (!xdr_nullstring(xdrs, &objp->policy)) {
		return (FALSE);
	}
	if (!xdr_long(xdrs, &objp->aux_attributes)) {
		return (FALSE);
	}
	if (!xdr_krb5_deltat(xdrs, &objp->max_renewable_life)) {
		return (FALSE);
	}
	if (!xdr_krb5_timestamp(xdrs, &objp->last_success)) {
		return (FALSE);
	}
	if (!xdr_krb5_timestamp(xdrs, &objp->last_failed)) {
		return (FALSE);
	}
	if (!xdr_krb5_kvno(xdrs, &objp->fail_auth_count)) {
		return (FALSE);
	}
	if (!xdr_krb5_int16(xdrs, &objp->n_key_data)) {
		return (FALSE);
	}
	if (xdrs->x_op == XDR_DECODE && objp->n_key_data < 0) {
		return (FALSE);
	}
	if (!xdr_krb5_int16(xdrs, &objp->n_tl_data)) {
		return (FALSE);
	}
	if (!xdr_nulltype(xdrs, (void **) &objp->tl_data,
			  xdr_krb5_tl_data)) {
		return FALSE;
	}
	n = objp->n_key_data;
	r = xdr_array(xdrs, (caddr_t *) &objp->key_data, &n, objp->n_key_data,
		      sizeof(krb5_key_data), xdr_krb5_key_data_nocontents);
	objp->n_key_data = n;
	if (!r) {
		return (FALSE);
	}

	return (TRUE);
}

static bool_t
_xdr_kadm5_policy_ent_rec(XDR *xdrs, kadm5_policy_ent_rec *objp, int vers)
{
	if (!xdr_nullstring(xdrs, &objp->policy)) {
		return (FALSE);
	}
	/* these all used to be u_int32, but it's stupid for sized types
	   to be exposed at the api, and they're the same as longs on the
	   wire. */
	if (!xdr_long(xdrs, &objp->pw_min_life)) {
		return (FALSE);
	}
	if (!xdr_long(xdrs, &objp->pw_max_life)) {
		return (FALSE);
	}
	if (!xdr_long(xdrs, &objp->pw_min_length)) {
		return (FALSE);
	}
	if (!xdr_long(xdrs, &objp->pw_min_classes)) {
		return (FALSE);
	}
	if (!xdr_long(xdrs, &objp->pw_history_num)) {
		return (FALSE);
	}
	if (!xdr_long(xdrs, &objp->policy_refcnt)) {
		return (FALSE);
	}
	if (xdrs->x_op == XDR_DECODE) {
		objp->pw_max_fail = 0;
		objp->pw_failcnt_interval = 0;
		objp->pw_lockout_duration = 0;
		objp->attributes = 0;
		objp->max_life = 0;
		objp->max_renewable_life = 0;
		objp->allowed_keysalts = NULL;
		objp->n_tl_data = 0;
		objp->tl_data = NULL;
	}
	if (vers >= KADM5_API_VERSION_3) {
		if (!xdr_krb5_kvno(xdrs, &objp->pw_max_fail))
			return (FALSE);
		if (!xdr_krb5_deltat(xdrs, &objp->pw_failcnt_interval))
			return (FALSE);
		if (!xdr_krb5_deltat(xdrs, &objp->pw_lockout_duration))
			return (FALSE);
	}
	if (vers >= KADM5_API_VERSION_4) {
		if (!xdr_krb5_flags(xdrs, &objp->attributes)) {
			return (FALSE);
		}
		if (!xdr_krb5_deltat(xdrs, &objp->max_life)) {
			return (FALSE);
		}
		if (!xdr_krb5_deltat(xdrs, &objp->max_renewable_life)) {
			return (FALSE);
		}
		if (!xdr_nullstring(xdrs, &objp->allowed_keysalts)) {
			return (FALSE);
		}
		if (!xdr_krb5_int16(xdrs, &objp->n_tl_data)) {
			return (FALSE);
		}
		if (!xdr_nulltype(xdrs, (void **) &objp->tl_data,
				  xdr_krb5_tl_data)) {
			return FALSE;
		}
	}
	return (TRUE);
}

bool_t
xdr_kadm5_policy_ent_rec(XDR *xdrs, kadm5_policy_ent_rec *objp)
{
	return _xdr_kadm5_policy_ent_rec(xdrs, objp, KADM5_API_VERSION_4);
}

bool_t
xdr_cprinc_arg(XDR *xdrs, cprinc_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!_xdr_kadm5_principal_ent_rec(xdrs, &objp->rec,
					  objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_long(xdrs, &objp->mask)) {
		return (FALSE);
	}
	if (!xdr_nullstring(xdrs, &objp->passwd)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_cprinc3_arg(XDR *xdrs, cprinc3_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!_xdr_kadm5_principal_ent_rec(xdrs, &objp->rec,
					  objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_long(xdrs, &objp->mask)) {
		return (FALSE);
	}
	if (!xdr_array(xdrs, (caddr_t *)&objp->ks_tuple,
		       (unsigned int *)&objp->n_ks_tuple, ~0,
		       sizeof(krb5_key_salt_tuple),
		       xdr_krb5_key_salt_tuple)) {
		return (FALSE);
	}
	if (!xdr_nullstring(xdrs, &objp->passwd)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_generic_ret(XDR *xdrs, generic_ret *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_kadm5_ret_t(xdrs, &objp->code)) {
		return (FALSE);
	}

	return(TRUE);
}

bool_t
xdr_dprinc_arg(XDR *xdrs, dprinc_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_krb5_principal(xdrs, &objp->princ)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_mprinc_arg(XDR *xdrs, mprinc_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!_xdr_kadm5_principal_ent_rec(xdrs, &objp->rec,
					  objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_long(xdrs, &objp->mask)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_rprinc_arg(XDR *xdrs, rprinc_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_krb5_principal(xdrs, &objp->src)) {
		return (FALSE);
	}
	if (!xdr_krb5_principal(xdrs, &objp->dest)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_gprincs_arg(XDR *xdrs, gprincs_arg *objp)
{
     if (!xdr_ui_4(xdrs, &objp->api_version)) {
	  return (FALSE);
     }
     if (!xdr_nullstring(xdrs, &objp->exp)) {
	  return (FALSE);
     }
     return (TRUE);
}

bool_t
xdr_gprincs_ret(XDR *xdrs, gprincs_ret *objp)
{
     if (!xdr_ui_4(xdrs, &objp->api_version)) {
	  return (FALSE);
     }
     if (!xdr_kadm5_ret_t(xdrs, &objp->code)) {
	  return (FALSE);
     }
     if (objp->code == KADM5_OK) {
	  if (!xdr_int(xdrs, &objp->count)) {
	       return (FALSE);
	  }
	  if (!xdr_array(xdrs, (caddr_t *) &objp->princs,
			 (unsigned int *) &objp->count, ~0,
			 sizeof(char *), xdr_nullstring)) {
	       return (FALSE);
	  }
     }

     return (TRUE);
}

bool_t
xdr_chpass_arg(XDR *xdrs, chpass_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_krb5_principal(xdrs, &objp->princ)) {
		return (FALSE);
	}
	if (!xdr_nullstring(xdrs, &objp->pass)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_chpass3_arg(XDR *xdrs, chpass3_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_krb5_principal(xdrs, &objp->princ)) {
		return (FALSE);
	}
	if (!xdr_krb5_boolean(xdrs, &objp->keepold)) {
		return (FALSE);
	}
	if (!xdr_array(xdrs, (caddr_t *)&objp->ks_tuple,
		       (unsigned int*)&objp->n_ks_tuple, ~0,
		       sizeof(krb5_key_salt_tuple),
		       xdr_krb5_key_salt_tuple)) {
		return (FALSE);
	}
	if (!xdr_nullstring(xdrs, &objp->pass)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_setkey_arg(XDR *xdrs, setkey_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_krb5_principal(xdrs, &objp->princ)) {
		return (FALSE);
	}
	if (!xdr_array(xdrs, (caddr_t *) &objp->keyblocks,
		       (unsigned int *) &objp->n_keys, ~0,
		       sizeof(krb5_keyblock), xdr_krb5_keyblock)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_setkey3_arg(XDR *xdrs, setkey3_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_krb5_principal(xdrs, &objp->princ)) {
		return (FALSE);
	}
	if (!xdr_krb5_boolean(xdrs, &objp->keepold)) {
		return (FALSE);
	}
	if (!xdr_array(xdrs, (caddr_t *) &objp->ks_tuple,
		       (unsigned int *) &objp->n_ks_tuple, ~0,
		       sizeof(krb5_key_salt_tuple), xdr_krb5_key_salt_tuple)) {
		return (FALSE);
	}
	if (!xdr_array(xdrs, (caddr_t *) &objp->keyblocks,
		       (unsigned int *) &objp->n_keys, ~0,
		       sizeof(krb5_keyblock), xdr_krb5_keyblock)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_setkey4_arg(XDR *xdrs, setkey4_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return FALSE;
	}
	if (!xdr_krb5_principal(xdrs, &objp->princ)) {
		return FALSE;
	}
	if (!xdr_krb5_boolean(xdrs, &objp->keepold)) {
		return FALSE;
	}
	if (!xdr_array(xdrs, (caddr_t *) &objp->key_data,
		       (unsigned int *) &objp->n_key_data, ~0,
		       sizeof(kadm5_key_data), xdr_kadm5_key_data)) {
		return FALSE;
	}
	return TRUE;
}

bool_t
xdr_chrand_arg(XDR *xdrs, chrand_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_krb5_principal(xdrs, &objp->princ)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_chrand3_arg(XDR *xdrs, chrand3_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_krb5_principal(xdrs, &objp->princ)) {
		return (FALSE);
	}
	if (!xdr_krb5_boolean(xdrs, &objp->keepold)) {
		return (FALSE);
	}
	if (!xdr_array(xdrs, (caddr_t *)&objp->ks_tuple,
		       (unsigned int*)&objp->n_ks_tuple, ~0,
		       sizeof(krb5_key_salt_tuple),
		       xdr_krb5_key_salt_tuple)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_chrand_ret(XDR *xdrs, chrand_ret *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_kadm5_ret_t(xdrs, &objp->code)) {
		return (FALSE);
	}
	if (objp->code == KADM5_OK) {
		if (!xdr_array(xdrs, (char **)&objp->keys,
			       (unsigned int *)&objp->n_keys, ~0,
			       sizeof(krb5_keyblock), xdr_krb5_keyblock))
			return FALSE;
	}

	return (TRUE);
}

bool_t
xdr_gprinc_arg(XDR *xdrs, gprinc_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_krb5_principal(xdrs, &objp->princ)) {
		return (FALSE);
	}
	if (!xdr_long(xdrs, &objp->mask)) {
	     return FALSE;
	}

	return (TRUE);
}

bool_t
xdr_gprinc_ret(XDR *xdrs, gprinc_ret *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_kadm5_ret_t(xdrs, &objp->code)) {
		return (FALSE);
	}
	if(objp->code == KADM5_OK)  {
		if (!_xdr_kadm5_principal_ent_rec(xdrs, &objp->rec,
						  objp->api_version)) {
			return (FALSE);
		}
	}

	return (TRUE);
}

bool_t
xdr_cpol_arg(XDR *xdrs, cpol_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!_xdr_kadm5_policy_ent_rec(xdrs, &objp->rec,
				       objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_long(xdrs, &objp->mask)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_dpol_arg(XDR *xdrs, dpol_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_nullstring(xdrs, &objp->name)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_mpol_arg(XDR *xdrs, mpol_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!_xdr_kadm5_policy_ent_rec(xdrs, &objp->rec,
				       objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_long(xdrs, &objp->mask)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_gpol_arg(XDR *xdrs, gpol_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_nullstring(xdrs, &objp->name)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_gpol_ret(XDR *xdrs, gpol_ret *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_kadm5_ret_t(xdrs, &objp->code)) {
		return (FALSE);
	}
	if(objp->code == KADM5_OK) {
	    if (!_xdr_kadm5_policy_ent_rec(xdrs, &objp->rec,
					   objp->api_version))
		return (FALSE);
	}

	return (TRUE);
}

bool_t
xdr_gpols_arg(XDR *xdrs, gpols_arg *objp)
{
     if (!xdr_ui_4(xdrs, &objp->api_version)) {
	  return (FALSE);
     }
     if (!xdr_nullstring(xdrs, &objp->exp)) {
	  return (FALSE);
     }
     return (TRUE);
}

bool_t
xdr_gpols_ret(XDR *xdrs, gpols_ret *objp)
{
     if (!xdr_ui_4(xdrs, &objp->api_version)) {
	  return (FALSE);
     }
     if (!xdr_kadm5_ret_t(xdrs, &objp->code)) {
	  return (FALSE);
     }
     if (objp->code == KADM5_OK) {
	  if (!xdr_int(xdrs, &objp->count)) {
	       return (FALSE);
	  }
	  if (!xdr_array(xdrs, (caddr_t *) &objp->pols,
			 (unsigned int *) &objp->count, ~0,
			 sizeof(char *), xdr_nullstring)) {
	       return (FALSE);
	  }
     }

     return (TRUE);
}

bool_t xdr_getprivs_ret(XDR *xdrs, getprivs_ret *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
     if (! xdr_kadm5_ret_t(xdrs, &objp->code) ||
	 ! xdr_long(xdrs, &objp->privs))
	  return FALSE;

     return TRUE;
}

bool_t
xdr_purgekeys_arg(XDR *xdrs, purgekeys_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_krb5_principal(xdrs, &objp->princ)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->keepkvno)) {
	     return FALSE;
	}

	return (TRUE);
}

bool_t
xdr_gstrings_arg(XDR *xdrs, gstrings_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_krb5_principal(xdrs, &objp->princ)) {
		return (FALSE);
	}

	return (TRUE);
}

bool_t
xdr_gstrings_ret(XDR *xdrs, gstrings_ret *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_kadm5_ret_t(xdrs, &objp->code)) {
		return (FALSE);
	}
	if (objp->code == KADM5_OK) {
		if (!xdr_int(xdrs, &objp->count)) {
			return (FALSE);
		}
		if (!xdr_array(xdrs, (caddr_t *) &objp->strings,
			       (unsigned int *) &objp->count, ~0,
			       sizeof(krb5_string_attr),
			       xdr_krb5_string_attr)) {
			return (FALSE);
		}
	}

	return (TRUE);
}

bool_t
xdr_sstring_arg(XDR *xdrs, sstring_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return (FALSE);
	}
	if (!xdr_krb5_principal(xdrs, &objp->princ)) {
		return (FALSE);
	}
	if (!xdr_nullstring(xdrs, &objp->key)) {
		return (FALSE);
	}
	if (!xdr_nullstring(xdrs, &objp->value)) {
		return (FALSE);
	}

	return (TRUE);
}

bool_t
xdr_krb5_principal(XDR *xdrs, krb5_principal *objp)
{
    int	    ret;
    char	    *p = NULL;
    krb5_principal  pr = NULL;
    static krb5_context context = NULL;

    /* using a static context here is ugly, but should work
       ok, and the other solutions are even uglier */

    if (!context &&
	kadm5_init_krb5_context(&context))
       return(FALSE);

    switch(xdrs->x_op) {
    case XDR_ENCODE:
	if (*objp) {
	     if((ret = krb5_unparse_name(context, *objp, &p)) != 0)
		  return FALSE;
	}
	if(!xdr_nullstring(xdrs, &p))
	    return FALSE;
	if (p) free(p);
	break;
    case XDR_DECODE:
	if(!xdr_nullstring(xdrs, &p))
	    return FALSE;
	if (p) {
	     ret = krb5_parse_name(context, p, &pr);
	     if(ret != 0)
		  return FALSE;
	     *objp = pr;
	     free(p);
	} else
	     *objp = NULL;
	break;
    case XDR_FREE:
	if(*objp != NULL)
	    krb5_free_principal(context, *objp);
	*objp = NULL;
	break;
    }
    return TRUE;
}

bool_t
xdr_krb5_octet(XDR *xdrs, krb5_octet *objp)
{
   if (!xdr_u_char(xdrs, objp))
	return (FALSE);
   return (TRUE);
}

bool_t
xdr_krb5_enctype(XDR *xdrs, krb5_enctype *objp)
{
   if (!xdr_int32(xdrs, (int32_t *) objp))
	return (FALSE);
   return (TRUE);
}

bool_t
xdr_krb5_salttype(XDR *xdrs, krb5_int32 *objp)
{
    if (!xdr_int32(xdrs, (int32_t *) objp))
	return FALSE;
    return TRUE;
}

bool_t
xdr_krb5_keyblock(XDR *xdrs, krb5_keyblock *objp)
{
   char *cp;

   /* XXX This only works because free_keyblock assumes ->contents
      is allocated by malloc() */
   if(!xdr_krb5_enctype(xdrs, &objp->enctype))
      return FALSE;
   cp = (char *)objp->contents;
   if(!xdr_bytes(xdrs, &cp, &objp->length, ~0))
      return FALSE;
   objp->contents = (uint8_t *)cp;
   return TRUE;
}

bool_t
xdr_krb5_string_attr(XDR *xdrs, krb5_string_attr *objp)
{
	if (!xdr_nullstring(xdrs, &objp->key))
		return FALSE;
	if (!xdr_nullstring(xdrs, &objp->value))
		return FALSE;
	if (xdrs->x_op == XDR_DECODE &&
	    (objp->key == NULL || objp->value == NULL))
		return FALSE;
	return TRUE;
}

bool_t
xdr_kadm5_key_data(XDR *xdrs, kadm5_key_data *objp)
{
	if (!xdr_krb5_kvno(xdrs, &objp->kvno))
		return FALSE;
	if (!xdr_krb5_keyblock(xdrs, &objp->key))
		return FALSE;
	if (!xdr_krb5_int16(xdrs, &objp->salt.type))
		return FALSE;
	if (!xdr_bytes(xdrs, &objp->salt.data.data,
		       &objp->salt.data.length, ~0))
		return FALSE;
	return TRUE;
}

bool_t
xdr_getpkeys_arg(XDR *xdrs, getpkeys_arg *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return FALSE;
	}
	if (!xdr_krb5_principal(xdrs, &objp->princ)) {
		return FALSE;
	}
	if (!xdr_krb5_kvno(xdrs, &objp->kvno)) {
		return FALSE;
	}
	return TRUE;
}

bool_t
xdr_getpkeys_ret(XDR *xdrs, getpkeys_ret *objp)
{
	if (!xdr_ui_4(xdrs, &objp->api_version)) {
		return FALSE;
	}
	if (!xdr_kadm5_ret_t(xdrs, &objp->code)) {
		return FALSE;
	}
	if (objp->code == KADM5_OK) {
		if (!xdr_array(xdrs, (caddr_t *) &objp->key_data,
			       (unsigned int *) &objp->n_key_data, ~0,
			       sizeof(kadm5_key_data), xdr_kadm5_key_data)) {
		    return FALSE;
		}
	}
	return TRUE;
}
