/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved.
 *
 */

#include <stdio.h>
#include <string.h>
#include <sys/errno.h>

#include <gssapi/gssapi.h>
#include <gssapi/gssapi_generic.h>
#ifdef GSSAPI_KRB5
#include <gssapi/gssapi_krb5.h>
#endif

#include <gssrpc/rpc.h>
#include <gssrpc/auth_gssapi.h>

#include "gssrpcint.h"

#ifdef __CODECENTER__
#define DEBUG_GSSAPI 1
#endif

#ifdef DEBUG_GSSAPI
int auth_debug_gssapi = DEBUG_GSSAPI;
extern void gssrpcint_printf(const char *format, ...);
#define L_PRINTF(l,args) if (auth_debug_gssapi >= l) gssrpcint_printf args
#define PRINTF(args) L_PRINTF(99, args)
#define AUTH_GSSAPI_DISPLAY_STATUS(args) \
	if (auth_debug_gssapi) auth_gssapi_display_status args
#else
#define PRINTF(args)
#define L_PRINTF(l, args)
#define AUTH_GSSAPI_DISPLAY_STATUS(args)
#endif

static void 	auth_gssapi_nextverf(AUTH *);
static bool_t 	auth_gssapi_marshall(AUTH *, XDR *);
static bool_t	auth_gssapi_validate(AUTH *, struct opaque_auth *);
static bool_t	auth_gssapi_refresh(AUTH *, struct rpc_msg *);
static bool_t	auth_gssapi_wrap(AUTH *, XDR *, xdrproc_t, caddr_t);
static bool_t	auth_gssapi_unwrap(AUTH *, XDR *, xdrproc_t, caddr_t);
static void	auth_gssapi_destroy(AUTH *);

static bool_t	marshall_new_creds(AUTH *, bool_t, gss_buffer_t);

static struct auth_ops auth_gssapi_ops = {
     auth_gssapi_nextverf,
     auth_gssapi_marshall,
     auth_gssapi_validate,
     auth_gssapi_refresh,
     auth_gssapi_destroy,
     auth_gssapi_wrap,
     auth_gssapi_unwrap,
};

/*
 * the ah_private data structure for an auth_handle
 */
struct auth_gssapi_data {
     bool_t established;
     CLIENT *clnt;
     gss_ctx_id_t context;
     gss_buffer_desc client_handle;
     uint32_t seq_num;
     int def_cred;

     /* pre-serialized ah_cred */
     unsigned char cred_buf[MAX_AUTH_BYTES];
     uint32_t cred_len;
};
#define AUTH_PRIVATE(auth) ((struct auth_gssapi_data *)auth->ah_private)

/*
 * Function: auth_gssapi_create_default
 *
 * Purpose:  Create a GSS-API style authenticator, with default
 * options, and return the handle.
 *
 * Effects: See design document, section XXX.
 */
AUTH *auth_gssapi_create_default(CLIENT *clnt, char *service_name)
{
     AUTH *auth;
     OM_uint32 gssstat, minor_stat;
     gss_buffer_desc input_name;
     gss_name_t target_name;

     input_name.value = service_name;
     input_name.length = strlen(service_name) + 1;

     gssstat = gss_import_name(&minor_stat, &input_name,
			       gss_nt_service_name, &target_name);
     if (gssstat != GSS_S_COMPLETE) {
	  AUTH_GSSAPI_DISPLAY_STATUS(("parsing name", gssstat,
				      minor_stat));
	  rpc_createerr.cf_stat = RPC_SYSTEMERROR;
	  rpc_createerr.cf_error.re_errno = ENOMEM;
	  return NULL;
     }

     auth = auth_gssapi_create(clnt,
			       &gssstat,
			       &minor_stat,
			       GSS_C_NO_CREDENTIAL,
			       target_name,
			       GSS_C_NULL_OID,
			       GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG,
			       0,
			       NULL,
			       NULL,
			       NULL);

     gss_release_name(&minor_stat, &target_name);
     return auth;
}

/*
 * Function: auth_gssapi_create
 *
 * Purpose: Create a GSS-API style authenticator, with all the
 * options, and return the handle.
 *
 * Effects: See design document, section XXX.
 */
AUTH *auth_gssapi_create(
     CLIENT *clnt,
     OM_uint32 *gssstat,
     OM_uint32 *minor_stat,
     gss_cred_id_t claimant_cred_handle,
     gss_name_t target_name,
     gss_OID mech_type,
     OM_uint32 req_flags,
     OM_uint32 time_req,
     gss_OID *actual_mech_type,
     OM_uint32 *ret_flags,
     OM_uint32 *time_rec)
{
     AUTH *auth, *save_auth;
     struct auth_gssapi_data *pdata;
     struct gss_channel_bindings_struct bindings, *bindp;
     struct sockaddr_in laddr, raddr;
     enum clnt_stat callstat;
     struct timeval timeout;
     int bindings_failed;
     rpcproc_t init_func;

     auth_gssapi_init_arg call_arg;
     auth_gssapi_init_res call_res;
     gss_buffer_desc *input_token, isn_buf;

     memset(&rpc_createerr, 0, sizeof(rpc_createerr));

     /* this timeout is only used if clnt_control(clnt, CLSET_TIMEOUT) */
     /* has not already been called.. therefore, we can just pick */
     /* something reasonable-sounding.. */
     timeout.tv_sec = 30;
     timeout.tv_usec = 0;

     auth = NULL;
     pdata = NULL;

     /* don't assume the caller will want to change clnt->cl_auth */
     save_auth = clnt->cl_auth;

     auth = (AUTH *) malloc(sizeof(*auth));
     pdata = (struct auth_gssapi_data *) malloc(sizeof(*pdata));
     if (auth == NULL || pdata == NULL) {
	  /* They needn't both have failed; clean up.  */
	  free(auth);
	  free(pdata);
	  auth = NULL;
	  pdata = NULL;
	  rpc_createerr.cf_stat = RPC_SYSTEMERROR;
	  rpc_createerr.cf_error.re_errno = ENOMEM;
	  goto cleanup;
     }
     memset(auth, 0, sizeof(*auth));
     memset(pdata, 0, sizeof(*pdata));

     auth->ah_ops = &auth_gssapi_ops;
     auth->ah_private = (caddr_t) pdata;

     /* initial creds are auth_msg TRUE and no handle */
     marshall_new_creds(auth, TRUE, NULL);

     /* initial verifier is empty */
     auth->ah_verf.oa_flavor = AUTH_GSSAPI;
     auth->ah_verf.oa_base = NULL;
     auth->ah_verf.oa_length = 0;

     AUTH_PRIVATE(auth)->established = FALSE;
     AUTH_PRIVATE(auth)->clnt = clnt;
     AUTH_PRIVATE(auth)->def_cred = (claimant_cred_handle ==
				     GSS_C_NO_CREDENTIAL);

     clnt->cl_auth = auth;

     /* start by trying latest version */
     call_arg.version = 4;
     bindings_failed = 0;

try_new_version:
     /* set state for initial call to init_sec_context */
     input_token = GSS_C_NO_BUFFER;
     AUTH_PRIVATE(auth)->context = GSS_C_NO_CONTEXT;
     init_func = AUTH_GSSAPI_INIT;

#ifdef GSSAPI_KRB5
     /*
      * OV servers up to version 3 used the old mech id.  Beta 7
      * servers used version 3 with the new mech id; however, the beta
      * 7 gss-api accept_sec_context accepts either mech id.  Thus, if
      * any server rejects version 4, we fall back to version 3 with
      * the old mech id; for the OV server it will be right, and for
      * the beta 7 server it will be accepted.  Not ideal, but it
      * works.
      */
     if (call_arg.version < 4 && (mech_type == gss_mech_krb5 ||
				  mech_type == GSS_C_NULL_OID))
	  mech_type = (gss_OID) gss_mech_krb5_old;
#endif

     if (!bindings_failed && call_arg.version >= 3) {
	  if (clnt_control(clnt, CLGET_LOCAL_ADDR, &laddr) == FALSE) {
	       PRINTF(("gssapi_create: CLGET_LOCAL_ADDR failed"));
	       goto cleanup;
	  }
	  if (clnt_control(clnt, CLGET_SERVER_ADDR, &raddr) == FALSE) {
	       PRINTF(("gssapi_create: CLGET_SERVER_ADDR failed"));
	       goto cleanup;
	  }

	  memset(&bindings, 0, sizeof(bindings));
	  bindings.application_data.length = 0;
	  bindings.initiator_addrtype = GSS_C_AF_INET;
	  bindings.initiator_address.length = 4;
	  bindings.initiator_address.value = &laddr.sin_addr.s_addr;

	  bindings.acceptor_addrtype = GSS_C_AF_INET;
	  bindings.acceptor_address.length = 4;
	  bindings.acceptor_address.value = &raddr.sin_addr.s_addr;
	  bindp = &bindings;
     } else {
	  bindp = NULL;
     }

     memset(&call_res, 0, sizeof(call_res));

next_token:
     *gssstat = gss_init_sec_context(minor_stat,
				     claimant_cred_handle,
				     &AUTH_PRIVATE(auth)->context,
				     target_name,
				     mech_type,
				     req_flags,
				     time_req,
				     bindp,
				     input_token,
				     actual_mech_type,
				     &call_arg.token,
				     ret_flags,
				     time_rec);

     if (*gssstat != GSS_S_COMPLETE && *gssstat != GSS_S_CONTINUE_NEEDED) {
	  AUTH_GSSAPI_DISPLAY_STATUS(("initializing context", *gssstat,
				      *minor_stat));
	  goto cleanup;
     }

     /* if we got a token, pass it on */
     if (call_arg.token.length != 0) {

	  /*
	   * sanity check: if we received a signed isn in the last
	   * response then there *cannot* be another token to send
	   */
	  if (call_res.signed_isn.length != 0) {
	       PRINTF(("gssapi_create: unexpected token from init_sec\n"));
	       goto cleanup;
	  }

	  PRINTF(("gssapi_create: calling GSSAPI_INIT (%d)\n", init_func));

	  xdr_free(xdr_authgssapi_init_res, &call_res);
	  memset(&call_res, 0, sizeof(call_res));
	  callstat = clnt_call(clnt, init_func,
			       xdr_authgssapi_init_arg, &call_arg,
			       xdr_authgssapi_init_res, &call_res,
			       timeout);
	  gss_release_buffer(minor_stat, &call_arg.token);

	  if (callstat != RPC_SUCCESS) {
	       struct rpc_err err;

	       clnt_geterr(clnt, &err);
	       if (callstat == RPC_AUTHERROR &&
		   (err.re_why == AUTH_BADCRED || err.re_why == AUTH_FAILED)
		   && call_arg.version >= 1) {
		    L_PRINTF(1,
			     ("call_arg protocol version %d rejected, trying %d.\n",
			    call_arg.version, call_arg.version-1));
		    call_arg.version--;
		    goto try_new_version;
	       } else {
		    PRINTF(("gssapi_create: GSSAPI_INIT (%d) failed, stat %d\n",
			    init_func, callstat));
	       }

	       goto cleanup;
	  } else if (call_res.version != call_arg.version &&
		     !(call_arg.version == 2 && call_res.version == 1)) {
	       /*
		* The Secure 1.1 servers always respond with version
		* 1.  Thus, if we just tried a version >=3, fall all
		* the way back to version 1 since that is all they
		* understand
		*/
	       if (call_arg.version > 2 && call_res.version == 1) {
		    L_PRINTF(1,
			     ("Talking to Secure 1.1 server, using version 1.\n"));
		    call_arg.version = 1;
		    goto try_new_version;
	       }

	       PRINTF(("gssapi_create: invalid call_res vers %d\n",
		       call_res.version));
	       goto cleanup;
	  } else if (call_res.gss_major != GSS_S_COMPLETE) {
	       AUTH_GSSAPI_DISPLAY_STATUS(("in response from server",
					   call_res.gss_major,
					   call_res.gss_minor));
	       goto cleanup;
	  }

	  PRINTF(("gssapi_create: GSSAPI_INIT (%d) succeeded\n", init_func));
	  init_func = AUTH_GSSAPI_CONTINUE_INIT;

	  /* check for client_handle */
	  if (AUTH_PRIVATE(auth)->client_handle.length == 0) {
	       if (call_res.client_handle.length == 0) {
		    PRINTF(("gssapi_create: expected client_handle\n"));
		    goto cleanup;
	       } else {
		    PRINTF(("gssapi_create: got client_handle %d\n",
			    *((uint32_t *)call_res.client_handle.value)));

		    GSS_DUP_BUFFER(AUTH_PRIVATE(auth)->client_handle,
				   call_res.client_handle);

		    /* auth_msg is TRUE; there may be more tokens */
		    marshall_new_creds(auth, TRUE,
				       &AUTH_PRIVATE(auth)->client_handle);
	       }
	  } else if (!GSS_BUFFERS_EQUAL(AUTH_PRIVATE(auth)->client_handle,
					call_res.client_handle)) {
	       PRINTF(("gssapi_create: got different client_handle\n"));
	       goto cleanup;
	  }

	  /* check for token */
	  if (call_res.token.length==0 && *gssstat==GSS_S_CONTINUE_NEEDED) {
	       PRINTF(("gssapi_create: expected token\n"));
	       goto cleanup;
	  } else if (call_res.token.length != 0) {
	       if (*gssstat == GSS_S_COMPLETE) {
		    PRINTF(("gssapi_create: got unexpected token\n"));
		    goto cleanup;
	       } else {
		    /* assumes call_res is safe until init_sec_context */
		    input_token = &call_res.token;
		    PRINTF(("gssapi_create: got new token\n"));
	       }
	  }
     }

     /* check for isn */
     if (*gssstat == GSS_S_COMPLETE) {
	  if (call_res.signed_isn.length == 0) {
	       PRINTF(("gssapi_created: expected signed isn\n"));
	       goto cleanup;
	  } else {
	       PRINTF(("gssapi_create: processing signed isn\n"));

	       /* don't check conf (integ only) or qop (accpet default) */
	       *gssstat = gss_unseal(minor_stat,
				     AUTH_PRIVATE(auth)->context,
				     &call_res.signed_isn,
				     &isn_buf, NULL, NULL);

	       if (*gssstat != GSS_S_COMPLETE) {
		    AUTH_GSSAPI_DISPLAY_STATUS(("unsealing isn",
						*gssstat, *minor_stat));
		    goto cleanup;
	       } else if (isn_buf.length != sizeof(uint32_t)) {
		    PRINTF(("gssapi_create: gss_unseal gave %d bytes\n",
			    (int) isn_buf.length));
		    goto cleanup;
	       }

	       AUTH_PRIVATE(auth)->seq_num = (uint32_t)
		    ntohl(*((uint32_t*)isn_buf.value));
	       *gssstat = gss_release_buffer(minor_stat, &isn_buf);
	       if (*gssstat != GSS_S_COMPLETE) {
		    AUTH_GSSAPI_DISPLAY_STATUS(("releasing unsealed isn",
						*gssstat, *minor_stat));
		    goto cleanup;
	       }

	       PRINTF(("gssapi_create: isn is %d\n",
		       AUTH_PRIVATE(auth)->seq_num));
	  }
     } else if (call_res.signed_isn.length != 0) {
	  PRINTF(("gssapi_create: got signed isn, can't check yet\n"));
     }

     /* results were okay.. continue if necessary */
     if (*gssstat == GSS_S_CONTINUE_NEEDED) {
	  PRINTF(("gssapi_create: not done, continuing\n"));
	  goto next_token;
     }

     /*
      * Done!  Context is established, we have client_handle and isn.
      */
     AUTH_PRIVATE(auth)->established = TRUE;

     marshall_new_creds(auth, FALSE,
			&AUTH_PRIVATE(auth)->client_handle);

     PRINTF(("gssapi_create: done. client_handle %#x, isn %d\n\n",
	     *((uint32_t *)AUTH_PRIVATE(auth)->client_handle.value),
	     AUTH_PRIVATE(auth)->seq_num));

     /* don't assume the caller will want to change clnt->cl_auth */
     clnt->cl_auth = save_auth;

     xdr_free(xdr_authgssapi_init_res, &call_res);
     return auth;

     /******************************************************************/

cleanup:
     PRINTF(("gssapi_create: bailing\n\n"));

     if (auth) {
	 if (AUTH_PRIVATE(auth))
	     auth_gssapi_destroy(auth);
	 else
	     free(auth);
	 auth = NULL;
     }

     /* don't assume the caller will want to change clnt->cl_auth */
     clnt->cl_auth = save_auth;

     if (rpc_createerr.cf_stat == 0)
	  rpc_createerr.cf_stat = RPC_AUTHERROR;

     xdr_free(xdr_authgssapi_init_res, &call_res);
     return auth;
}

/*
 * Function: marshall_new_creds
 *
 * Purpose: (pre-)serialize auth_msg and client_handle fields of
 * auth_gssapi_creds into auth->cred_buf
 *
 * Arguments:
 *
 * 	auth		(r/w) the AUTH structure to modify
 * 	auth_msg	(r) the auth_msg field to serialize
 * 	client_handle	(r) the client_handle field to serialize, or
 * 			NULL
 *
 * Returns: TRUE if successful, FALSE if not
 *
 * Requires: auth must point to a valid GSS-API auth structure, auth_msg
 * must be TRUE or FALSE, client_handle must be a gss_buffer_t with a valid
 * value and length field or NULL.
 *
 * Effects: auth->ah_cred is set to the serialized auth_gssapi_creds
 * version 2 structure (stored in the cred_buf field of private data)
 * containing version, auth_msg and client_handle.
 * auth->ah_cred.oa_flavor is set to AUTH_GSSAPI.  If cliend_handle is
 * NULL, it is treated as if it had a length of 0 and a value of NULL.
 *
 * Modifies: auth
 */
static bool_t marshall_new_creds(
     AUTH *auth,
     bool_t auth_msg,
     gss_buffer_t client_handle)
{
     auth_gssapi_creds creds;
     XDR xdrs;

     PRINTF(("marshall_new_creds: starting\n"));

     creds.version = 2;

     creds.auth_msg = auth_msg;
     if (client_handle)
	  GSS_COPY_BUFFER(creds.client_handle, *client_handle)
     else {
	  creds.client_handle.length = 0;
	  creds.client_handle.value = NULL;
     }

     xdrmem_create(&xdrs, (caddr_t) AUTH_PRIVATE(auth)->cred_buf,
		   MAX_AUTH_BYTES, XDR_ENCODE);
     if (! xdr_authgssapi_creds(&xdrs, &creds)) {
	  PRINTF(("marshall_new_creds: failed encoding auth_gssapi_creds\n"));
	  XDR_DESTROY(&xdrs);
	  return FALSE;
     }
     AUTH_PRIVATE(auth)->cred_len = xdr_getpos(&xdrs);
     XDR_DESTROY(&xdrs);

     PRINTF(("marshall_new_creds: auth_gssapi_creds is %d bytes\n",
	     AUTH_PRIVATE(auth)->cred_len));

     auth->ah_cred.oa_flavor = AUTH_GSSAPI;
     auth->ah_cred.oa_base = (char *) AUTH_PRIVATE(auth)->cred_buf;
     auth->ah_cred.oa_length = AUTH_PRIVATE(auth)->cred_len;

     PRINTF(("marshall_new_creds: succeeding\n"));

     return TRUE;
}


/*
 * Function: auth_gssapi_nextverf
 *
 * Purpose: None.
 *
 * Effects: None.  Never called.
 */
static void auth_gssapi_nextverf(AUTH *auth)
{
}

/*
 * Function: auth_gssapi_marhsall
 *
 * Purpose: Marshall RPC credentials and verifier onto xdr stream.
 *
 * Arguments:
 *
 * 	auth		(r/w) AUTH structure for client
 * 	xdrs		(r/w) XDR stream to marshall to
 *
 * Returns: boolean indicating success/failure
 *
 * Effects:
 *
 * The pre-serialized credentials in cred_buf are serialized.  If the
 * context is established, the sealed sequence number is serialized as
 * the verifier.  If the context is not established, an empty verifier
 * is serialized.  The sequence number is *not* incremented, because
 * this function is called multiple times if retransmission is required.
 *
 * If this took all the header fields as arguments, it could sign
 * them.
 */
static bool_t auth_gssapi_marshall(
     AUTH *auth,
     XDR *xdrs)
{
     OM_uint32 minor_stat;
     gss_buffer_desc out_buf;
     uint32_t seq_num;

     if (AUTH_PRIVATE(auth)->established == TRUE)  {
	  PRINTF(("gssapi_marshall: starting\n"));

	  seq_num = AUTH_PRIVATE(auth)->seq_num + 1;

	  PRINTF(("gssapi_marshall: sending seq_num %d\n", seq_num));

	  if (auth_gssapi_seal_seq(AUTH_PRIVATE(auth)->context, seq_num,
				   &out_buf) == FALSE) {
	       PRINTF(("gssapi_marhshall: seal failed\n"));
	  }

	  auth->ah_verf.oa_base = out_buf.value;
	  auth->ah_verf.oa_length = out_buf.length;

	  if (! xdr_opaque_auth(xdrs, &auth->ah_cred) ||
	      ! xdr_opaque_auth(xdrs, &auth->ah_verf)) {
	       (void) gss_release_buffer(&minor_stat, &out_buf);
	       return FALSE;
	  }
	  (void) gss_release_buffer(&minor_stat, &out_buf);
     } else {
	  PRINTF(("gssapi_marshall: not established, sending null verf\n"));

	  auth->ah_verf.oa_base = NULL;
	  auth->ah_verf.oa_length = 0;

	  if (! xdr_opaque_auth(xdrs, &auth->ah_cred) ||
	      ! xdr_opaque_auth(xdrs, &auth->ah_verf)) {
	       return FALSE;
	  }
     }

     return TRUE;
}

/*
 * Function: auth_gssapi_validate
 *
 * Purpose: Validate RPC response verifier from server.
 *
 * Effects: See design document, section XXX.
 */
static bool_t auth_gssapi_validate(
     AUTH *auth,
     struct opaque_auth *verf)
{
     gss_buffer_desc in_buf;
     uint32_t seq_num;

     if (AUTH_PRIVATE(auth)->established == FALSE) {
	  PRINTF(("gssapi_validate: not established, noop\n"));
	  return TRUE;
     }

     PRINTF(("gssapi_validate: starting\n"));

     in_buf.length = verf->oa_length;
     in_buf.value = verf->oa_base;
     if (auth_gssapi_unseal_seq(AUTH_PRIVATE(auth)->context, &in_buf,
				&seq_num) == FALSE) {
	  PRINTF(("gssapi_validate: failed unsealing verifier\n"));
	  return FALSE;
     }

     /* we sent seq_num+1, so we should get back seq_num+2 */
     if (AUTH_PRIVATE(auth)->seq_num+2 != seq_num) {
	  PRINTF(("gssapi_validate: expecting seq_num %d, got %d (%#x)\n",
		  AUTH_PRIVATE(auth)->seq_num + 2, seq_num, seq_num));
	  return FALSE;
     }
     PRINTF(("gssapi_validate: seq_num %d okay\n", seq_num));

     /* +1 for successful transmission, +1 for successful validation */
     AUTH_PRIVATE(auth)->seq_num += 2;

     PRINTF(("gssapi_validate: succeeding\n"));

     return TRUE;
}

/*
 * Function: auth_gssapi_refresh
 *
 * Purpose: Attempts to resyncrhonize the sequence number.
 *
 * Effects:
 *
 * When the server receives a properly authenticated RPC call, it
 * increments the sequence number it is expecting from the client.
 * But if the server's response is lost for any reason, the client
 * can't know whether the server ever received it, assumes it didn't,
 * and does *not* increment its sequence number.  Thus, the client's
 * next call will fail with AUTH_REJECTEDCRED because the server will
 * think it is a replay attack.
 *
 * When an AUTH_REJECTEDCRED error arrives, this function attempts to
 * resyncrhonize by incrementing the client's sequence number and
 * returning TRUE.  If any other error arrives, it returns FALSE.
 */
static bool_t auth_gssapi_refresh(
     AUTH *auth,
     struct rpc_msg *msg)
{
     if (msg->rm_reply.rp_rjct.rj_stat == AUTH_ERROR &&
	 msg->rm_reply.rp_rjct.rj_why == AUTH_REJECTEDVERF) {
	  PRINTF(("gssapi_refresh: rejected verifier, incrementing\n"));
	  AUTH_PRIVATE(auth)->seq_num++;
	  return TRUE;
     } else {
	  PRINTF(("gssapi_refresh: failing\n"));
	  return FALSE;
     }
}

/*
 * Function: auth_gssapi_destroy
 *
 * Purpose: Destroy a GSS-API authentication structure.
 *
 * Effects:  This function destroys the GSS-API authentication
 * context, and sends a message to the server instructing it to
 * invokte gss_process_token() and thereby destroy its corresponding
 * context.  Since the client doesn't really care whether the server
 * gets this message, no failures are reported.
 */
static void auth_gssapi_destroy(AUTH *auth)
{
     struct timeval timeout;
     OM_uint32 gssstat, minor_stat;
     gss_cred_id_t cred;
     int callstat;

     if (AUTH_PRIVATE(auth)->client_handle.length == 0) {
	  PRINTF(("gssapi_destroy: no client_handle, not calling destroy\n"));
	  goto skip_call;
     }

     PRINTF(("gssapi_destroy: marshalling new creds\n"));
     if (!marshall_new_creds(auth, TRUE, &AUTH_PRIVATE(auth)->client_handle)) {
	  PRINTF(("gssapi_destroy: marshall_new_creds failed\n"));
	  goto skip_call;
     }

     PRINTF(("gssapi_destroy: calling GSSAPI_DESTROY\n"));
     timeout.tv_sec = 1;
     timeout.tv_usec = 0;
     callstat = clnt_call(AUTH_PRIVATE(auth)->clnt, AUTH_GSSAPI_DESTROY,
			  xdr_void, NULL, xdr_void, NULL, timeout);
     if (callstat != RPC_SUCCESS)
	  clnt_sperror(AUTH_PRIVATE(auth)->clnt,
		       "gssapi_destroy: GSSAPI_DESTROY failed");

skip_call:
     PRINTF(("gssapi_destroy: deleting context\n"));
     gssstat = gss_delete_sec_context(&minor_stat,
				      &AUTH_PRIVATE(auth)->context,
				      NULL);
     if (gssstat != GSS_S_COMPLETE)
	  AUTH_GSSAPI_DISPLAY_STATUS(("deleting context", gssstat,
				      minor_stat));
     if (AUTH_PRIVATE(auth)->def_cred) {
	  cred = GSS_C_NO_CREDENTIAL;
	  gssstat = gss_release_cred(&minor_stat, &cred);
	  if (gssstat != GSS_S_COMPLETE)
	       AUTH_GSSAPI_DISPLAY_STATUS(("deleting default credential",
					   gssstat, minor_stat));
     }

     free(AUTH_PRIVATE(auth)->client_handle.value);

#if 0
     PRINTF(("gssapi_destroy: calling GSSAPI_EXIT\n"));
     AUTH_PRIVATE(auth)->established = FALSE;
     callstat = clnt_call(AUTH_PRIVATE(auth)->clnt, AUTH_GSSAPI_EXIT,
			  xdr_void, NULL, xdr_void, NULL, timeout);
#endif

     free(auth->ah_private);
     free(auth);
     PRINTF(("gssapi_destroy: done\n"));
}

/*
 * Function: auth_gssapi_wrap
 *
 * Purpose: encrypt the serialized arguments from xdr_func applied to
 * xdr_ptr and write the result to xdrs.
 *
 * Effects: See design doc, section XXX.
 */
static bool_t auth_gssapi_wrap(
     AUTH *auth,
     XDR *out_xdrs,
     bool_t (*xdr_func)(),
     caddr_t xdr_ptr)
{
     OM_uint32 gssstat, minor_stat;

     if (! AUTH_PRIVATE(auth)->established) {
	  PRINTF(("gssapi_wrap: context not established, noop\n"));
	  return (*xdr_func)(out_xdrs, xdr_ptr);
     } else if (! auth_gssapi_wrap_data(&gssstat, &minor_stat,
					AUTH_PRIVATE(auth)->context,
					AUTH_PRIVATE(auth)->seq_num+1,
					out_xdrs, xdr_func, xdr_ptr)) {
	  if (gssstat != GSS_S_COMPLETE)
	       AUTH_GSSAPI_DISPLAY_STATUS(("encrypting function arguments",
					   gssstat, minor_stat));
	  return FALSE;
     } else
	  return TRUE;
}

/*
 * Function: auth_gssapi_unwrap
 *
 * Purpose: read encrypted arguments from xdrs, decrypt, and
 * deserialize with xdr_func into xdr_ptr.
 *
 * Effects: See design doc, section XXX.
 */
static bool_t auth_gssapi_unwrap(
     AUTH *auth,
     XDR *in_xdrs,
     bool_t (*xdr_func)(),
     caddr_t xdr_ptr)
{
     OM_uint32 gssstat, minor_stat;

     if (! AUTH_PRIVATE(auth)->established) {
	  PRINTF(("gssapi_unwrap: context not established, noop\n"));
	  return (*xdr_func)(in_xdrs, xdr_ptr);
     } else if (! auth_gssapi_unwrap_data(&gssstat, &minor_stat,
					  AUTH_PRIVATE(auth)->context,
					  AUTH_PRIVATE(auth)->seq_num,
					  in_xdrs, xdr_func, xdr_ptr)) {
	  if (gssstat != GSS_S_COMPLETE)
	       AUTH_GSSAPI_DISPLAY_STATUS(("decrypting function arguments",
					   gssstat, minor_stat));
	  return FALSE;
     } else
	  return TRUE;
}
