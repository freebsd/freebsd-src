/* #pragma ident	"@(#)g_acquire_cred.c	1.22	04/02/23 SMI" */

/*
 * Copyright 1996 by Sun Microsystems, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Sun Microsystems not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. Sun Microsystems makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 *  glue routine for gss_acquire_cred
 */

#include "mglueP.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <errno.h>
#include <time.h>

static OM_uint32
val_acq_cred_args(
    OM_uint32 *minor_status,
    gss_name_t desired_name,
    OM_uint32 time_req,
    gss_OID_set desired_mechs,
    int cred_usage,
    gss_const_key_value_set_t cred_store,
    gss_cred_id_t *output_cred_handle,
    gss_OID_set *actual_mechs,
    OM_uint32 *time_rec)
{

    /* Initialize outputs. */

    if (minor_status != NULL)
	*minor_status = 0;

    if (output_cred_handle != NULL)
	*output_cred_handle = GSS_C_NO_CREDENTIAL;

    if (actual_mechs != NULL)
	*actual_mechs = GSS_C_NULL_OID_SET;

    if (time_rec != NULL)
	*time_rec = 0;

    /* Validate arguments. */

    if (minor_status == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (output_cred_handle == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (cred_usage != GSS_C_ACCEPT
	&& cred_usage != GSS_C_INITIATE
	&& cred_usage != GSS_C_BOTH) {
	if (minor_status) {
	    *minor_status = EINVAL;
	    map_errcode(minor_status);
	}
	return GSS_S_FAILURE;
    }

    return (GSS_S_COMPLETE);
}


OM_uint32 KRB5_CALLCONV
gss_acquire_cred(minor_status,
                 desired_name,
                 time_req,
                 desired_mechs,
		 cred_usage,
                 output_cred_handle,
                 actual_mechs,
                 time_rec)

OM_uint32 *		minor_status;
gss_name_t		desired_name;
OM_uint32		time_req;
gss_OID_set		desired_mechs;
int			cred_usage;
gss_cred_id_t *		output_cred_handle;
gss_OID_set *		actual_mechs;
OM_uint32 *		time_rec;

{
    return gss_acquire_cred_from(minor_status, desired_name, time_req,
				 desired_mechs, cred_usage, NULL,
				 output_cred_handle, actual_mechs, time_rec);
}

OM_uint32 KRB5_CALLCONV
gss_acquire_cred_from(minor_status,
		      desired_name,
		      time_req,
		      desired_mechs,
		      cred_usage,
		      cred_store,
		      output_cred_handle,
		      actual_mechs,
		      time_rec)

OM_uint32 *			minor_status;
gss_name_t			desired_name;
OM_uint32			time_req;
gss_OID_set			desired_mechs;
int				cred_usage;
gss_const_key_value_set_t	cred_store;
gss_cred_id_t *			output_cred_handle;
gss_OID_set *			actual_mechs;
OM_uint32 *			time_rec;

{
    OM_uint32 major = GSS_S_FAILURE, tmpMinor;
    OM_uint32 first_major = GSS_S_COMPLETE, first_minor = 0;
    OM_uint32 initTimeOut = 0, acceptTimeOut = 0, outTime = GSS_C_INDEFINITE;
    gss_OID_set mechs = GSS_C_NO_OID_SET;
    gss_OID_set_desc except_attrs;
    gss_OID_desc attr_oids[2];
    unsigned int i;
    gss_union_cred_t creds = NULL;

    major = val_acq_cred_args(minor_status,
			      desired_name,
			      time_req,
			      desired_mechs,
			      cred_usage,
			      cred_store,
			      output_cred_handle,
			      actual_mechs,
			      time_rec);
    if (major != GSS_S_COMPLETE)
	goto cleanup;

    /*
     * if desired_mechs equals GSS_C_NULL_OID_SET, then try to
     * acquire credentials for all non-deprecated mechanisms.
     */
    if (desired_mechs == GSS_C_NULL_OID_SET) {
	attr_oids[0] = *GSS_C_MA_DEPRECATED;
	attr_oids[1] = *GSS_C_MA_NOT_DFLT_MECH;
	except_attrs.count = 2;
	except_attrs.elements = attr_oids;
	major = gss_indicate_mechs_by_attrs(minor_status, GSS_C_NO_OID_SET,
					    &except_attrs, GSS_C_NO_OID_SET,
					    &mechs);
	if (major != GSS_S_COMPLETE)
	    goto cleanup;
    } else
	mechs = desired_mechs;

    if (mechs->count == 0) {
	major = GSS_S_BAD_MECH;
	goto cleanup;
    }

    /* allocate the output credential structure */
    creds = (gss_union_cred_t)calloc(1, sizeof (gss_union_cred_desc));
    if (creds == NULL) {
	major = GSS_S_FAILURE;
	*minor_status = ENOMEM;
	goto cleanup;
    }

    creds->count = 0;
    creds->loopback = creds;

    /* for each requested mech attempt to obtain a credential */
    for (i = 0, major = GSS_S_UNAVAILABLE; i < mechs->count; i++) {
	major = gss_add_cred_from(&tmpMinor, (gss_cred_id_t)creds,
				  desired_name, &mechs->elements[i],
				  cred_usage, time_req, time_req,
				  cred_store, NULL, NULL,
				  time_rec ? &initTimeOut : NULL,
				  time_rec ? &acceptTimeOut : NULL);
	if (major == GSS_S_COMPLETE) {
	    /* update the credential's time */
	    if (cred_usage == GSS_C_ACCEPT) {
		if (outTime > acceptTimeOut)
		    outTime = acceptTimeOut;
	    } else if (cred_usage == GSS_C_INITIATE) {
		if (outTime > initTimeOut)
		    outTime = initTimeOut;
	    } else {
		/*
		 * time_rec is the lesser of the
		 * init/accept times
		 */
		if (initTimeOut > acceptTimeOut)
		    outTime = (outTime > acceptTimeOut) ?
			acceptTimeOut : outTime;
		else
		    outTime = (outTime > initTimeOut) ?
			initTimeOut : outTime;
	    }
	} else if (first_major == GSS_S_COMPLETE) {
	    first_major = major;
	    first_minor = tmpMinor;
	}
    } /* for */

    /* If we didn't get any creds, return the error status from the first mech
     * (which is often the preferred one). */
    if (creds->count < 1) {
	major = first_major;
	*minor_status = first_minor;
	goto cleanup;
    }
    major = GSS_S_COMPLETE;

    /*
     * fill in output parameters
     * setup the actual mechs output parameter
     */
    if (actual_mechs != NULL) {
	major = gssint_make_public_oid_set(minor_status, creds->mechs_array,
					   creds->count, actual_mechs);
	if (GSS_ERROR(major))
	    goto cleanup;
    }

    if (time_rec)
	*time_rec = outTime;

    *output_cred_handle = (gss_cred_id_t)creds;

cleanup:
    if (GSS_ERROR(major))
	gss_release_cred(&tmpMinor, (gss_cred_id_t *)&creds);
    if (desired_mechs == GSS_C_NO_OID_SET)
        generic_gss_release_oid_set(&tmpMinor, &mechs);

    return (major);
}

static OM_uint32
val_add_cred_args(
    OM_uint32 *minor_status,
    gss_cred_id_t input_cred_handle,
    gss_name_t desired_name,
    gss_OID desired_mech,
    gss_cred_usage_t cred_usage,
    gss_const_key_value_set_t cred_store,
    OM_uint32 initiator_time_req,
    OM_uint32 acceptor_time_req,
    gss_cred_id_t *output_cred_handle,
    gss_OID_set *actual_mechs,
    OM_uint32 *initiator_time_rec,
    OM_uint32 *acceptor_time_rec)
{

    /* Initialize outputs. */

    if (minor_status != NULL)
	*minor_status = 0;

    if (output_cred_handle != NULL)
	*output_cred_handle = GSS_C_NO_CREDENTIAL;

    if (actual_mechs != NULL)
	*actual_mechs = GSS_C_NO_OID_SET;

    if (acceptor_time_rec != NULL)
	*acceptor_time_rec = 0;

    if (initiator_time_rec != NULL)
	*initiator_time_rec = 0;

    /* Validate arguments. */

    if (minor_status == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (input_cred_handle == GSS_C_NO_CREDENTIAL &&
	output_cred_handle == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE | GSS_S_NO_CRED);

    if (cred_usage != GSS_C_ACCEPT
	&& cred_usage != GSS_C_INITIATE
	&& cred_usage != GSS_C_BOTH) {
	if (minor_status) {
	    *minor_status = EINVAL;
	    map_errcode(minor_status);
	}
	return GSS_S_FAILURE;
    }

    return (GSS_S_COMPLETE);
}


/* V2 KRB5_CALLCONV */
OM_uint32 KRB5_CALLCONV
gss_add_cred(minor_status, input_cred_handle,
		  desired_name, desired_mech, cred_usage,
		  initiator_time_req, acceptor_time_req,
		  output_cred_handle, actual_mechs,
		  initiator_time_rec, acceptor_time_rec)
    OM_uint32		*minor_status;
    gss_cred_id_t	input_cred_handle;
    gss_name_t		desired_name;
    gss_OID		desired_mech;
    gss_cred_usage_t	cred_usage;
    OM_uint32		initiator_time_req;
    OM_uint32		acceptor_time_req;
    gss_cred_id_t	*output_cred_handle;
    gss_OID_set		*actual_mechs;
    OM_uint32		*initiator_time_rec;
    OM_uint32		*acceptor_time_rec;
{
    return gss_add_cred_from(minor_status, input_cred_handle, desired_name,
			     desired_mech, cred_usage, initiator_time_req,
			     acceptor_time_req, NULL, output_cred_handle,
			     actual_mechs, initiator_time_rec,
			     acceptor_time_rec);
}

OM_uint32 KRB5_CALLCONV
gss_add_cred_from(minor_status, input_cred_handle,
		  desired_name, desired_mech,
		  cred_usage,
		  initiator_time_req, acceptor_time_req,
		  cred_store,
		  output_cred_handle, actual_mechs,
		  initiator_time_rec, acceptor_time_rec)
    OM_uint32		*minor_status;
    gss_cred_id_t	input_cred_handle;
    gss_name_t		desired_name;
    gss_OID		desired_mech;
    gss_cred_usage_t	cred_usage;
    OM_uint32		initiator_time_req;
    OM_uint32		acceptor_time_req;
    gss_const_key_value_set_t  cred_store;
    gss_cred_id_t	*output_cred_handle;
    gss_OID_set		*actual_mechs;
    OM_uint32		*initiator_time_rec;
    OM_uint32		*acceptor_time_rec;
{
    OM_uint32		status, temp_minor_status;
    OM_uint32		time_req, time_rec = 0, *time_recp = NULL;
    gss_union_name_t	union_name;
    gss_union_cred_t	new_union_cred, union_cred;
    gss_name_t		internal_name = GSS_C_NO_NAME;
    gss_name_t		allocated_name = GSS_C_NO_NAME;
    gss_mechanism	mech;
    gss_cred_id_t	cred = NULL;
    gss_OID		new_mechs_array = NULL;
    gss_cred_id_t *	new_cred_array = NULL;
    gss_OID_set		target_mechs = GSS_C_NO_OID_SET;
    gss_OID		selected_mech = GSS_C_NO_OID;

    status = val_add_cred_args(minor_status,
			       input_cred_handle,
			       desired_name,
			       desired_mech,
			       cred_usage,
			       cred_store,
			       initiator_time_req,
			       acceptor_time_req,
			       output_cred_handle,
			       actual_mechs,
			       initiator_time_rec,
			       acceptor_time_rec);
    if (status != GSS_S_COMPLETE)
	return (status);

    status = gssint_select_mech_type(minor_status, desired_mech,
				     &selected_mech);
    if (status != GSS_S_COMPLETE)
	return (status);

    mech = gssint_get_mechanism(selected_mech);
    if (!mech)
	return GSS_S_BAD_MECH;
    else if (!mech->gss_acquire_cred)
	return (GSS_S_UNAVAILABLE);

    if (input_cred_handle == GSS_C_NO_CREDENTIAL) {
	union_cred = malloc(sizeof (gss_union_cred_desc));
	if (union_cred == NULL)
	    return (GSS_S_FAILURE);

	(void) memset(union_cred, 0, sizeof (gss_union_cred_desc));
    } else {
	union_cred = (gss_union_cred_t)input_cred_handle;
	if (gssint_get_mechanism_cred(union_cred, selected_mech) !=
	    GSS_C_NO_CREDENTIAL)
	    return (GSS_S_DUPLICATE_ELEMENT);
    }

    /* for default credentials we will use GSS_C_NO_NAME */
    if (input_cred_handle != GSS_C_NO_CREDENTIAL ||
        cred_store != GSS_C_NO_CRED_STORE) {
	/* may need to create a mechanism specific name */
	if (desired_name) {
	    union_name = (gss_union_name_t)desired_name;
	    if (union_name->mech_type &&
		g_OID_equal(union_name->mech_type, selected_mech))
		internal_name = union_name->mech_name;
	    else {
		if (gssint_import_internal_name(minor_status, selected_mech,
						union_name, &allocated_name) !=
		    GSS_S_COMPLETE)
		    return (GSS_S_BAD_NAME);
		internal_name = allocated_name;
	    }
	}
    }


    if (cred_usage == GSS_C_ACCEPT)
	time_req = acceptor_time_req;
    else if (cred_usage == GSS_C_INITIATE)
	time_req = initiator_time_req;
    else if (cred_usage == GSS_C_BOTH)
	time_req = (acceptor_time_req > initiator_time_req) ?
	    acceptor_time_req : initiator_time_req;
    else
	time_req = 0;

    status = gss_create_empty_oid_set(minor_status, &target_mechs);
    if (status != GSS_S_COMPLETE)
	goto errout;

    status = gss_add_oid_set_member(minor_status,
				    gssint_get_public_oid(selected_mech),
				    &target_mechs);
    if (status != GSS_S_COMPLETE)
	goto errout;

    if (initiator_time_rec != NULL || acceptor_time_rec != NULL)
	time_recp = &time_rec;

    if (mech->gss_acquire_cred_from) {
	status = mech->gss_acquire_cred_from(minor_status, internal_name,
					     time_req, target_mechs,
					     cred_usage, cred_store, &cred,
					     NULL, time_recp);
    } else if (cred_store == GSS_C_NO_CRED_STORE) {
	status = mech->gss_acquire_cred(minor_status, internal_name, time_req,
					target_mechs, cred_usage, &cred, NULL,
					time_recp);
    } else {
	return GSS_S_UNAVAILABLE;
    }

    if (status != GSS_S_COMPLETE) {
	map_error(minor_status, mech);
	goto errout;
    }

    /* now add the new credential elements */
    new_mechs_array = (gss_OID)
	malloc(sizeof (gss_OID_desc) * (union_cred->count+1));

    new_cred_array = (gss_cred_id_t *)
	malloc(sizeof (gss_cred_id_t) * (union_cred->count+1));

    if (!new_mechs_array || !new_cred_array) {
	status = GSS_S_FAILURE;
	goto errout;
    }

    if (acceptor_time_rec)
	if (cred_usage == GSS_C_ACCEPT || cred_usage == GSS_C_BOTH)
	    *acceptor_time_rec = time_rec;
    if (initiator_time_rec)
	if (cred_usage == GSS_C_INITIATE || cred_usage == GSS_C_BOTH)
	    *initiator_time_rec = time_rec;

    /*
     * OK, expand the mechanism array and the credential array
     */
    (void) memcpy(new_mechs_array, union_cred->mechs_array,
		  sizeof (gss_OID_desc) * union_cred->count);
    (void) memcpy(new_cred_array, union_cred->cred_array,
		  sizeof (gss_cred_id_t) * union_cred->count);

    new_cred_array[union_cred->count] = cred;
    if ((new_mechs_array[union_cred->count].elements =
	 malloc(selected_mech->length)) == NULL)
	goto errout;

    g_OID_copy(&new_mechs_array[union_cred->count], selected_mech);

    if (actual_mechs != NULL) {
	status = gssint_make_public_oid_set(minor_status, new_mechs_array,
					    union_cred->count + 1,
					    actual_mechs);
	if (GSS_ERROR(status)) {
	    free(new_mechs_array[union_cred->count].elements);
	    goto errout;
	}
    }

    if (output_cred_handle == NULL) {
	free(union_cred->mechs_array);
	free(union_cred->cred_array);
	new_union_cred = union_cred;
    } else {
	new_union_cred = malloc(sizeof (gss_union_cred_desc));
	if (new_union_cred == NULL) {
	    free(new_mechs_array[union_cred->count].elements);
	    goto errout;
	}
	*new_union_cred = *union_cred;
	*output_cred_handle = (gss_cred_id_t)new_union_cred;
    }

    new_union_cred->mechs_array = new_mechs_array;
    new_union_cred->cred_array = new_cred_array;
    new_union_cred->count++;
    new_union_cred->loopback = new_union_cred;

    /* We're done with the internal name. Free it if we allocated it. */

    if (allocated_name)
	(void) gssint_release_internal_name(&temp_minor_status,
					   selected_mech,
					   &allocated_name);
    (void) generic_gss_release_oid_set(&temp_minor_status, &target_mechs);

    return (GSS_S_COMPLETE);

errout:
    if (new_mechs_array)
	free(new_mechs_array);
    if (new_cred_array)
	free(new_cred_array);

    if (cred != NULL && mech->gss_release_cred)
	mech->gss_release_cred(&temp_minor_status, &cred);

    if (allocated_name)
	(void) gssint_release_internal_name(&temp_minor_status,
					   selected_mech,
					   &allocated_name);

    if (input_cred_handle == GSS_C_NO_CREDENTIAL && union_cred)
	free(union_cred);

    (void) generic_gss_release_oid_set(&temp_minor_status, &target_mechs);

    return (status);
}
