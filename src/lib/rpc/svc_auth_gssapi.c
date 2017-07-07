/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved.
 *
 * $Id$
 *
 */

/*
 * svc_auth_gssapi.c
 * Handles the GSS-API flavor authentication parameters on the service
 * side of RPC.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <gssrpc/rpc.h>
#include <sys/stat.h>

#include <gssapi/gssapi_generic.h>
#include <gssrpc/auth_gssapi.h>

#ifdef GSS_BACKWARD_HACK
#include <gssapi/gssapi_krb5.h>
#endif

#include "gssrpcint.h"

#ifdef GSSAPI_KRB5
/* This is here for the krb5_error_code typedef and the
 * KRB5KRB_AP_ERR_NOT_US #define.*/
#include <krb5.h>
#endif

#include <sys/file.h>
#include <fcntl.h>
#include <time.h>

#define INITIATION_TIMEOUT 60*15 /* seconds until partially created */
				 /* context is destroed */
#define INDEF_EXPIRE 60*60*24	/* seconds until an context with no */
                                /* expiration time is expired */

#ifdef __CODECENTER__
#define DEBUG_GSSAPI 1
#endif

#ifdef DEBUG_GSSAPI
int svc_debug_gssapi = DEBUG_GSSAPI;
void gssrpcint_printf(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
#if 1
    vprintf(format, ap);
#else
    {
	static FILE *f;
	if (f == NULL)
	    f = fopen("/dev/pts/4", "a");
	if (f) {
	    vfprintf(f, format, ap);
	    fflush(f);
	}
    }
#endif
    va_end(ap);
}
#define L_PRINTF(l,args) if (svc_debug_gssapi >= l) gssrpcint_printf args
#define PRINTF(args) L_PRINTF(99, args)
#define AUTH_GSSAPI_DISPLAY_STATUS(args) \
	if (svc_debug_gssapi) auth_gssapi_display_status args
#else
#define PRINTF(args)
#define L_PRINTF(l, args)
#define AUTH_GSSAPI_DISPLAY_STATUS(args)
#endif

typedef struct _svc_auth_gssapi_data {
     bool_t established;

     gss_ctx_id_t context;
     gss_name_t client_name, server_name;
     gss_cred_id_t server_creds;

     uint32_t expiration;
     uint32_t seq_num;
     uint32_t key;

     SVCAUTH svcauth;

     /* kludge to free verifiers on next call */
     gss_buffer_desc prev_verf;
} svc_auth_gssapi_data;

#define SVCAUTH_PRIVATE(auth) \
     ((svc_auth_gssapi_data *)(auth)->svc_ah_private)

static bool_t	svc_auth_gssapi_wrap(SVCAUTH *, XDR *, xdrproc_t, caddr_t);
static bool_t	svc_auth_gssapi_unwrap(SVCAUTH *, XDR *, xdrproc_t, caddr_t);
static bool_t	svc_auth_gssapi_destroy(SVCAUTH *);

static svc_auth_gssapi_data *create_client(void);
static svc_auth_gssapi_data *get_client
       (gss_buffer_t client_handle);
static void destroy_client
       (svc_auth_gssapi_data *client_data);
static void clean_client(void), cleanup(void);
static void client_expire
       (svc_auth_gssapi_data *client_data, uint32_t exp);
static void dump_db (char *msg);

struct svc_auth_ops svc_auth_gssapi_ops = {
     svc_auth_gssapi_wrap,
     svc_auth_gssapi_unwrap,
     svc_auth_gssapi_destroy
};

/*
 * Globals!  Eeek!  Run for the hills!
 */
static gss_cred_id_t *server_creds_list = NULL;
static gss_name_t *server_name_list = NULL;
static int server_creds_count = 0;

static auth_gssapi_log_badauth_func log_badauth = NULL;
static caddr_t log_badauth_data = NULL;
static auth_gssapi_log_badauth2_func log_badauth2 = NULL;
static caddr_t log_badauth2_data = NULL;
static auth_gssapi_log_badverf_func log_badverf = NULL;
static caddr_t log_badverf_data = NULL;
static auth_gssapi_log_miscerr_func log_miscerr = NULL;
static caddr_t log_miscerr_data = NULL;

#define LOG_MISCERR(arg) if (log_miscerr) \
	(*log_miscerr)(rqst, msg, arg, log_miscerr_data)

typedef struct _client_list {
     svc_auth_gssapi_data *client;
     struct _client_list *next;
} client_list;

static client_list *clients = NULL;


/* Invoke log_badauth callbacks for an authentication failure. */
static void
badauth(OM_uint32 maj, OM_uint32 minor, SVCXPRT *xprt)
{
     if (log_badauth != NULL)
	  (*log_badauth)(maj, minor, &xprt->xp_raddr, log_badauth_data);
     if (log_badauth2 != NULL)
	  (*log_badauth2)(maj, minor, xprt, log_badauth2_data);
}

enum auth_stat gssrpc__svcauth_gssapi(
     register struct svc_req *rqst,
     register struct rpc_msg *msg,
     bool_t *no_dispatch)
{
     XDR xdrs;
     auth_gssapi_creds creds;
     auth_gssapi_init_arg call_arg;
     auth_gssapi_init_res call_res;
     gss_buffer_desc output_token, in_buf, out_buf;
     gss_cred_id_t server_creds;
     struct gss_channel_bindings_struct bindings, *bindp;
     OM_uint32 gssstat, minor_stat, time_rec;
     struct opaque_auth *cred, *verf;
     svc_auth_gssapi_data *client_data;
     int i;
     enum auth_stat ret;
     OM_uint32 ret_flags;
     uint32_t seq_num;

     PRINTF(("svcauth_gssapi: starting\n"));

     /* clean up expired entries */
     clean_client();

     /* use AUTH_NONE until there is a client_handle */
     rqst->rq_xprt->xp_auth = &svc_auth_none;

     memset(&call_res, 0, sizeof(call_res));
     creds.client_handle.length = 0;
     creds.client_handle.value = NULL;

     cred = &msg->rm_call.cb_cred;
     verf = &msg->rm_call.cb_verf;

     if (cred->oa_length == 0) {
	  PRINTF(("svcauth_gssapi: empty creds, failing\n"));
	  LOG_MISCERR("empty client credentials");
	  ret = AUTH_BADCRED;
	  goto error;
     }

     PRINTF(("svcauth_gssapi: decoding credentials\n"));
     xdrmem_create(&xdrs, cred->oa_base, cred->oa_length, XDR_DECODE);
     memset(&creds, 0, sizeof(creds));
     if (! xdr_authgssapi_creds(&xdrs, &creds)) {
	  PRINTF(("svcauth_gssapi: failed decoding creds\n"));
	  LOG_MISCERR("protocol error in client credentials");
	  xdr_free(xdr_authgssapi_creds, &creds);
	  XDR_DESTROY(&xdrs);
	  ret = AUTH_BADCRED;
	  goto error;
     }
     XDR_DESTROY(&xdrs);

     PRINTF(("svcauth_gssapi: got credentials, version %d, client_handle len %d\n",
	     creds.version, (int) creds.client_handle.length));

     if (creds.version != 2) {
 	  PRINTF(("svcauth_gssapi: bad credential version\n"));
 	  LOG_MISCERR("unsupported client credentials version");
 	  ret = AUTH_BADCRED;
 	  goto error;
     }

#ifdef DEBUG_GSSAPI
     if (svc_debug_gssapi) {
	  if (creds.auth_msg && rqst->rq_proc == AUTH_GSSAPI_EXIT) {
	       PRINTF(("svcauth_gssapi: GSSAPI_EXIT, cleaning up\n"));
	       svc_sendreply(rqst->rq_xprt, xdr_void, NULL);
	       xdr_free(xdr_authgssapi_creds, &creds);
	       cleanup();
	       exit(0);
	  }
     }
#endif

     /*
      * If this is an auth_msg and proc is GSSAPI_INIT, then create a
      * client handle for this client.  Otherwise, look up the
      * existing handle.
      */
     if (creds.auth_msg && rqst->rq_proc == AUTH_GSSAPI_INIT) {
	  if (creds.client_handle.length != 0) {
	       PRINTF(("svcauth_gssapi: non-empty handle on GSSAPI_INIT\n"));
	       LOG_MISCERR("protocol error in client handle");
	       ret = AUTH_FAILED;
	       goto error;
	  }

	  PRINTF(("svcauth_gssapi: GSSAPI_INIT, creating client.\n"));

	  client_data = create_client();
	  if (client_data == NULL) {
	       PRINTF(("svcauth_gssapi: create_client failed\n"));
	       LOG_MISCERR("internal error creating client record");
	       ret = AUTH_FAILED;
	       goto error;
	  }
     } else {
	  if (creds.client_handle.length == 0) {
	       PRINTF(("svcauth_gssapi: expected non-empty creds\n"));
	       LOG_MISCERR("protocol error in client credentials");
	       ret = AUTH_FAILED;
	       goto error;
	  }

	  PRINTF(("svcauth_gssapi: incoming client_handle %d, len %d\n",
		  *((uint32_t *) creds.client_handle.value),
		  (int) creds.client_handle.length));

	  client_data = get_client(&creds.client_handle);
	  if (client_data == NULL) {
	       PRINTF(("svcauth_gssapi: client_handle lookup failed\n"));
	       LOG_MISCERR("invalid client handle received");
	       ret = AUTH_BADCRED;
	       goto error;
	  }
	  PRINTF(("svcauth_gssapi: client_handle lookup succeeded\n"));
     }

     /* any response we send will use client_handle, so set it now */
     call_res.client_handle.length = sizeof(client_data->key);
     call_res.client_handle.value = (char *) &client_data->key;

     /* mark this call as using AUTH_GSSAPI via client_data's SVCAUTH */
     rqst->rq_xprt->xp_auth = &client_data->svcauth;

     if (client_data->established == FALSE) {
	  PRINTF(("svcauth_gssapi: context is not established\n"));

	  if (creds.auth_msg == FALSE) {
	       PRINTF(("svcauth_gssapi: expected auth_msg TRUE\n"));
	       LOG_MISCERR("protocol error on incomplete connection");
	       ret = AUTH_REJECTEDCRED;
	       goto error;
	  }

	  /*
	   * If the context is not established, then only GSSAPI_INIT
	   * and _CONTINUE requests are valid.
	   */
	  if (rqst->rq_proc != AUTH_GSSAPI_INIT && rqst->rq_proc !=
	      AUTH_GSSAPI_CONTINUE_INIT) {
	       PRINTF(("svcauth_gssapi: unacceptable procedure %d\n",
		       rqst->rq_proc));
	       LOG_MISCERR("protocol error on incomplete connection");
	       ret = AUTH_FAILED;
	       goto error;
	  }

	  /* call is for us, deserialize arguments */
	  memset(&call_arg, 0, sizeof(call_arg));
	  if (! svc_getargs(rqst->rq_xprt, xdr_authgssapi_init_arg,
			    &call_arg)) {
	       PRINTF(("svcauth_gssapi: cannot decode args\n"));
	       LOG_MISCERR("protocol error in procedure arguments");
	       ret = AUTH_BADCRED;
	       goto error;
	  }

	  /*
	   * Process the call arg version number.
	   *
	   * Set the krb5_gss backwards-compatibility mode based on client
	   * version.  This controls whether the AP_REP message is
	   * encrypted with the session key (version 2+, correct) or the
	   * session subkey (version 1, incorrect).  This function can
	   * never fail, so we don't bother checking its return value.
	   */
	  switch (call_arg.version) {
	  case 1:
	  case 2:
	       LOG_MISCERR("Warning: Accepted old RPC protocol request");
	       call_res.version = 1;
	       break;
	  case 3:
	  case 4:
	       /* 3 and 4 are essentially the same, don't bother warning */
	       call_res.version = call_arg.version;
	       break;
	  default:
	       PRINTF(("svcauth_gssapi: bad GSSAPI_INIT version\n"));
	       LOG_MISCERR("unsupported GSSAPI_INIT version");
	       ret = AUTH_BADCRED;
	       goto error;
	  }

#ifdef GSS_BACKWARD_HACK
	  krb5_gss_set_backward_mode(&minor_stat, call_arg.version == 1);
#endif

	  if (call_arg.version >= 3) {
	       memset(&bindings, 0, sizeof(bindings));
	       bindings.application_data.length = 0;
	       bindings.initiator_addrtype = GSS_C_AF_INET;
	       bindings.initiator_address.length = 4;
	       bindings.initiator_address.value =
		    &svc_getcaller(rqst->rq_xprt)->sin_addr.s_addr;

	       if (rqst->rq_xprt->xp_laddrlen > 0) {
		    bindings.acceptor_addrtype = GSS_C_AF_INET;
		    bindings.acceptor_address.length = 4;
		    bindings.acceptor_address.value =
			 &rqst->rq_xprt->xp_laddr.sin_addr.s_addr;
	       } else {
		    LOG_MISCERR("cannot get local address");
		    ret = AUTH_FAILED;
		    goto error;
	       }


	       bindp = &bindings;
	  } else {
	       bindp = GSS_C_NO_CHANNEL_BINDINGS;
	  }

	  /*
	   * If the client's server_creds is already set, use it.
	   * Otherwise, try each credential in server_creds_list until
	   * one of them succeedes, then set the client server_creds
	   * to that.  If all fail, the client's server_creds isn't
	   * set (which is fine, because the client will be gc'ed
	   * anyway).
	   *
	   * If accept_sec_context returns something other than
	   * success and GSS_S_FAILURE, then assume different
	   * credentials won't help and stop looping.
	   *
	   * Note that there are really two cases here: (1) the client
	   * has a server_creds already, and (2) it does not.  They
	   * are both written in the same loop so that there is only
	   * one textual call to gss_accept_sec_context; in fact, in
	   * case (1), the loop is executed exactly once.
	   */
	  for (i = 0; i < server_creds_count; i++) {
	       if (client_data->server_creds != NULL) {
		    PRINTF(("svcauth_gssapi: using's clients server_creds\n"));
		    server_creds = client_data->server_creds;
	       } else {
		    PRINTF(("svcauth_gssapi: trying creds %d\n", i));
		    server_creds = server_creds_list[i];
	       }

	       /* Free previous output_token from loop */
	       if(i != 0) gss_release_buffer(&minor_stat, &output_token);

	       call_res.gss_major =
		    gss_accept_sec_context(&call_res.gss_minor,
					   &client_data->context,
					   server_creds,
					   &call_arg.token,
					   bindp,
					   &client_data->client_name,
					   NULL,
					   &output_token,
					   &ret_flags,
					   &time_rec,
					   NULL);

	       if (server_creds == client_data->server_creds)
		    break;

	       PRINTF(("accept_sec_context returned 0x%x 0x%x not-us=%#x\n",
		       call_res.gss_major, call_res.gss_minor,
		       (int) KRB5KRB_AP_ERR_NOT_US));
	       if (call_res.gss_major == GSS_S_COMPLETE ||
		   call_res.gss_major == GSS_S_CONTINUE_NEEDED) {
		    /* server_creds was right, set it! */
		    PRINTF(("svcauth_gssapi: creds are correct, storing\n"));
		    client_data->server_creds = server_creds;
		    client_data->server_name = server_name_list[i];
		    break;
	       } else if (call_res.gss_major != GSS_S_FAILURE
#ifdef GSSAPI_KRB5
			  /*
			   * hard-coded because there is no other way
			   * to prevent all GSS_S_FAILURES from
			   * returning a "wrong principal in request"
			   * error
			   */
			  || ((krb5_error_code) call_res.gss_minor !=
			      (krb5_error_code) KRB5KRB_AP_ERR_NOT_US)
#endif
			  ) {
		    break;
	       }
	  }

	  gssstat = call_res.gss_major;
	  minor_stat = call_res.gss_minor;

	  /* done with call args */
	  xdr_free(xdr_authgssapi_init_arg, &call_arg);

	  PRINTF(("svcauth_gssapi: accept_sec_context returned %#x %#x\n",
		  call_res.gss_major, call_res.gss_minor));
	  if (call_res.gss_major != GSS_S_COMPLETE &&
	      call_res.gss_major != GSS_S_CONTINUE_NEEDED) {
	       AUTH_GSSAPI_DISPLAY_STATUS(("accepting context",
					   call_res.gss_major,
					   call_res.gss_minor));

	       badauth(call_res.gss_major, call_res.gss_minor, rqst->rq_xprt);

	       gss_release_buffer(&minor_stat, &output_token);
	       svc_sendreply(rqst->rq_xprt, xdr_authgssapi_init_res,
			     (caddr_t) &call_res);
	       *no_dispatch = TRUE;
	       ret = AUTH_OK;
	       goto error;
	  }

	  if (output_token.length != 0) {
	       PRINTF(("svcauth_gssapi: got new output token\n"));
	       GSS_COPY_BUFFER(call_res.token, output_token);
	  }

	  if (gssstat == GSS_S_COMPLETE) {
	       client_data->seq_num = rand();
	       client_expire(client_data,
			     (time_rec == GSS_C_INDEFINITE ?
			      INDEF_EXPIRE : time_rec) + time(0));

	       PRINTF(("svcauth_gssapi: context established, isn %d\n",
		       client_data->seq_num));

	       if (auth_gssapi_seal_seq(client_data->context,
					client_data->seq_num,
					&call_res.signed_isn) ==
		   FALSE) {
		    ret = AUTH_FAILED;
		    LOG_MISCERR("internal error sealing sequence number");
		    gss_release_buffer(&minor_stat, &output_token);
		    goto error;
	       }
	  }

	  PRINTF(("svcauth_gssapi: sending reply\n"));
	  svc_sendreply(rqst->rq_xprt, xdr_authgssapi_init_res,
			(caddr_t) &call_res);
	  *no_dispatch = TRUE;

	  /*
	   * If appropriate, set established to TRUE *after* sending
	   * response (otherwise, the client will receive the final
	   * token encrypted)
	   */
	  if (gssstat == GSS_S_COMPLETE) {
	       gss_release_buffer(&minor_stat, &call_res.signed_isn);
	       client_data->established = TRUE;
	  }
	  gss_release_buffer(&minor_stat, &output_token);
     } else {
	  PRINTF(("svcauth_gssapi: context is established\n"));

	  /* check the verifier */
	  PRINTF(("svcauth_gssapi: checking verifier, len %d\n",
		  verf->oa_length));

	  in_buf.length = verf->oa_length;
	  in_buf.value = verf->oa_base;

	  if (auth_gssapi_unseal_seq(client_data->context, &in_buf,
				     &seq_num) == FALSE) {
	       ret = AUTH_BADVERF;
	       LOG_MISCERR("internal error unsealing sequence number");
	       goto error;
	  }

	  if (seq_num != client_data->seq_num + 1) {
	       PRINTF(("svcauth_gssapi: expected isn %d, got %d\n",
		       client_data->seq_num + 1, seq_num));
	       if (log_badverf != NULL)
		    (*log_badverf)(client_data->client_name,
				   client_data->server_name,
				   rqst, msg, log_badverf_data);

	       ret = AUTH_REJECTEDVERF;
	       goto error;
	  }
	  client_data->seq_num++;

	  PRINTF(("svcauth_gssapi: seq_num %d okay\n", seq_num));

	  /* free previous response verifier, if any */
	  if (client_data->prev_verf.length != 0) {
	       gss_release_buffer(&minor_stat, &client_data->prev_verf);
	       client_data->prev_verf.length = 0;
	  }

	  /* prepare response verifier */
	  seq_num = client_data->seq_num + 1;
	  if (auth_gssapi_seal_seq(client_data->context, seq_num,
				   &out_buf) == FALSE) {
	       ret = AUTH_FAILED;
	       LOG_MISCERR("internal error sealing sequence number");
	       goto error;
	  }

	  client_data->seq_num++;

	  PRINTF(("svcauth_gssapi; response seq_num %d\n", seq_num));

	  rqst->rq_xprt->xp_verf.oa_flavor = AUTH_GSSAPI;
	  rqst->rq_xprt->xp_verf.oa_base = out_buf.value;
	  rqst->rq_xprt->xp_verf.oa_length = out_buf.length;

	  /* save verifier so it can be freed next time */
	  client_data->prev_verf.value = out_buf.value;
	  client_data->prev_verf.length = out_buf.length;

	  /*
	   * Message is authentic.  If auth_msg if true, process the
	   * call; otherwise, return AUTH_OK so it will be dispatched
	   * to the application server.
	   */

	  if (creds.auth_msg == TRUE) {
	       /*
		* If process_token fails, then the token probably came
		* from an attacker.  No response (error or otherwise)
		* should be returned to the client, since it won't be
		* accepting one.
		*/

	       switch (rqst->rq_proc) {
	       case AUTH_GSSAPI_MSG:
		    PRINTF(("svcauth_gssapi: GSSAPI_MSG, getting args\n"));
		    memset(&call_arg, 0, sizeof(call_arg));
		    if (! svc_getargs(rqst->rq_xprt, xdr_authgssapi_init_arg,
				      &call_arg)) {
			 PRINTF(("svcauth_gssapi: cannot decode args\n"));
			 LOG_MISCERR("protocol error in call arguments");
			 xdr_free(xdr_authgssapi_init_arg, &call_arg);
			 ret = AUTH_BADCRED;
			 goto error;
		    }

		    PRINTF(("svcauth_gssapi: processing token\n"));
		    gssstat = gss_process_context_token(&minor_stat,
							client_data->context,
							&call_arg.token);

		    /* done with call args */
		    xdr_free(xdr_authgssapi_init_arg, &call_arg);

		    if (gssstat != GSS_S_COMPLETE) {
			 AUTH_GSSAPI_DISPLAY_STATUS(("processing token",
						     gssstat, minor_stat));
			 ret = AUTH_FAILED;
			 goto error;
		    }

		    svc_sendreply(rqst->rq_xprt, xdr_void, NULL);
		    *no_dispatch = TRUE;
		    break;

	       case AUTH_GSSAPI_DESTROY:
		    PRINTF(("svcauth_gssapi: GSSAPI_DESTROY\n"));

		    PRINTF(("svcauth_gssapi: sending reply\n"));
		    svc_sendreply(rqst->rq_xprt, xdr_void, NULL);
		    *no_dispatch = TRUE;

		    destroy_client(client_data);
		    rqst->rq_xprt->xp_auth = NULL;
		    break;

	       default:
		    PRINTF(("svcauth_gssapi: unacceptable procedure %d\n",
			    rqst->rq_proc));
		    LOG_MISCERR("invalid call procedure number");
		    ret = AUTH_FAILED;
		    goto error;
	       }
	  } else {
	       /* set credentials for app server; comment in svc.c */
	       /* seems to imply this is incorrect, but I don't see */
	       /* any problem with it... */
	       rqst->rq_clntcred = (char *)client_data->client_name;
	       rqst->rq_svccred = (char *)client_data->context;
	  }
     }

     if (creds.client_handle.length != 0) {
	  PRINTF(("svcauth_gssapi: freeing client_handle len %d\n",
		  (int) creds.client_handle.length));
	  xdr_free(xdr_authgssapi_creds, &creds);
     }

     PRINTF(("\n"));
     return AUTH_OK;

error:
     if (creds.client_handle.length != 0) {
	  PRINTF(("svcauth_gssapi: freeing client_handle len %d\n",
		  (int) creds.client_handle.length));
	  xdr_free(xdr_authgssapi_creds, &creds);
     }

     PRINTF(("\n"));
     return ret;
}

static void cleanup(void)
{
     client_list *c, *c2;

     PRINTF(("cleanup_and_exit: starting\n"));

     c = clients;
     while (c) {
	  c2 = c;
	  c = c->next;
	  destroy_client(c2->client);
	  free(c2);
     }

     exit(0);
}

/*
 * Function: create_client
 *
 * Purpose: Creates an new client_data structure and stores it in the
 * database.
 *
 * Returns: the new client_data structure, or NULL on failure.
 *
 * Effects:
 *
 * A new client_data is created and stored in the hash table and
 * b-tree.  A new key that is unique in the current database is
 * chosen; this key should be used as the client's client_handle.
 */
static svc_auth_gssapi_data *create_client(void)
{
     client_list *c;
     svc_auth_gssapi_data *client_data;
     static int client_key = 1;

     PRINTF(("svcauth_gssapi: empty creds, creating\n"));

     client_data = (svc_auth_gssapi_data *) malloc(sizeof(*client_data));
     if (client_data == NULL)
	  return NULL;
     memset(client_data, 0, sizeof(*client_data));
     L_PRINTF(2, ("create_client: new client_data = %p\n",
		  (void *) client_data));

     /* set up client data structure */
     client_data->established = 0;
     client_data->context = GSS_C_NO_CONTEXT;
     client_data->expiration = time(0) + INITIATION_TIMEOUT;

     /* set up psycho-recursive SVCAUTH hack */
     client_data->svcauth.svc_ah_ops = &svc_auth_gssapi_ops;
     client_data->svcauth.svc_ah_private = (caddr_t) client_data;

     client_data->key = client_key++;

     c = (client_list *) malloc(sizeof(client_list));
     if (c == NULL)
	  return NULL;
     c->client = client_data;
     c->next = NULL;


     if (clients == NULL)
	  clients = c;
     else {
	  c->next = clients;
	  clients = c;
     }

     PRINTF(("svcauth_gssapi: new handle %d\n", client_data->key));
     L_PRINTF(2, ("create_client: done\n"));

     return client_data;
}

/*
 * Function: client_expire
 *
 * Purpose: change the expiration time of a client in the database
 *
 * Arguments:
 *
 * 	client_data	(r) the client_data to expire
 * 	exp		(r) the new expiration time
 *
 * Effects:
 *
 * client_data->expiration = exp
 *
 * This function used to remove client_data from the database, change
 * its expiration time, and re-add it, which was necessary because the
 * database was sorted by expiration time so a simple modification
 * would break the rep invariant.  Now the database is an unsorted
 * linked list, so it doesn't matter.
 */
static void client_expire(
     svc_auth_gssapi_data *client_data,
     uint32_t exp)
{
     client_data->expiration = exp;
}

/*
 * Function get_client
 *
 * Purpose: retrieve a client_data structure from the database based
 * on its client handle (key)
 *
 * Arguments:
 *
 *	client_handle	(r) the handle (key) to retrieve
 *
 * Effects:
 *
 * Searches the list and returns the client_data whose key field
 * matches the contents of client_handle, or returns NULL if none was
 * found.
 */
static svc_auth_gssapi_data *get_client(gss_buffer_t client_handle)
{
     client_list *c;
     uint32_t handle;

     memcpy(&handle, client_handle->value, 4);

     L_PRINTF(2, ("get_client: looking for client %d\n", handle));

     c = clients;
     while (c) {
	  if (c->client->key == handle)
	       return c->client;
	  c = c->next;
     }

     L_PRINTF(2, ("get_client: client_handle lookup failed\n"));
     return NULL;
}

/*
 * Function: destroy_client
 *
 * Purpose: destroys a client entry and removes it from the database
 *
 * Arguments:
 *
 *	client_data	(r) the client to be destroyed
 *
 * Effects:
 *
 * client_data->context is deleted with gss_delete_sec_context.
 * client_data's entry in the database is destroyed.  client_data is
 * freed.
 */
static void destroy_client(svc_auth_gssapi_data *client_data)
{
     OM_uint32 gssstat, minor_stat;
     gss_buffer_desc out_buf;
     client_list *c, *c2;

     PRINTF(("destroy_client: destroying client_data\n"));
     L_PRINTF(2, ("destroy_client: client_data = %p\n", (void *) client_data));

#ifdef DEBUG_GSSAPI
     if (svc_debug_gssapi >= 3)
	  dump_db("before frees");
#endif

     /* destroy client struct even if error occurs */

     gssstat = gss_delete_sec_context(&minor_stat, &client_data->context,
				      &out_buf);
     if (gssstat != GSS_S_COMPLETE)
	  AUTH_GSSAPI_DISPLAY_STATUS(("deleting context", gssstat,
				      minor_stat));

     gss_release_buffer(&minor_stat, &out_buf);
     gss_release_name(&minor_stat, &client_data->client_name);
     if (client_data->prev_verf.length != 0)
	  gss_release_buffer(&minor_stat, &client_data->prev_verf);

     if (clients == NULL) {
	  PRINTF(("destroy_client: called on empty database\n"));
	  abort();
     } else if (clients->client == client_data) {
	  c = clients;
	  clients = clients->next;
	  free(c);
     } else {
	  c2 = clients;
	  c = clients->next;
	  while (c) {
	       if (c->client == client_data) {
		    c2->next = c->next;
		    free(c);
		    goto done;
	       } else {
		    c2 = c;
		    c = c->next;
	       }
	  }
	  PRINTF(("destroy_client: client_handle delete failed\n"));
	  abort();
     }

done:

     L_PRINTF(2, ("destroy_client: client %d destroyed\n", client_data->key));

     free(client_data);

#if 0 /*ifdef PURIFY*/
     purify_watch_n(client_data, sizeof(*client_data), "rw");
#endif
}

static void dump_db(char *msg)
{
     svc_auth_gssapi_data *client_data;
     client_list *c;

     L_PRINTF(3, ("dump_db: %s:\n", msg));

     c = clients;
     while (c) {
	  client_data = c->client;
	  L_PRINTF(3, ("\tclient_data = %p, exp = %d\n",
		       (void *) client_data, client_data->expiration));
	  c = c->next;
     }

     L_PRINTF(3, ("\n"));
}

static void clean_client(void)
{
     svc_auth_gssapi_data *client_data;
     client_list *c;

     PRINTF(("clean_client: starting\n"));

     c = clients;
     while (c) {
	  client_data = c->client;

	  L_PRINTF(2, ("clean_client: client_data = %p\n",
		       (void *) client_data));

	  if (client_data->expiration < time(0)) {
	       PRINTF(("clean_client: client %d expired\n",
		       client_data->key));
	       destroy_client(client_data);
	       c = clients; /* start over, just to be safe */
	  } else {
	       c = c->next;
	  }
     }

     PRINTF(("clean_client: done\n"));
}

/*
 * Function: svcauth_gssapi_set_names
 *
 * Purpose: Sets the list of service names for which incoming
 * authentication requests should be honored.
 *
 * See functional specifications.
 */
bool_t svcauth_gssapi_set_names(
     auth_gssapi_name *names,
     int num)
{
     OM_uint32 gssstat, minor_stat;
     gss_buffer_desc in_buf;
     int i;

     if (num == 0)
	  for (; names[num].name != NULL; num++)
	       ;

     server_creds_list = NULL;
     server_name_list = NULL;

     server_creds_list = (gss_cred_id_t *) malloc(num*sizeof(gss_cred_id_t));
     if (server_creds_list == NULL)
	  goto fail;
     server_name_list = (gss_name_t *) malloc(num*sizeof(gss_name_t));
     if (server_name_list == NULL)
	  goto fail;

     for (i = 0; i < num; i++) {
	  server_name_list[i] = 0;
	  server_creds_list[i] = 0;
     }

     server_creds_count = num;

     for (i = 0; i < num; i++) {
	  in_buf.value = names[i].name;
	  in_buf.length = strlen(in_buf.value) + 1;

	  PRINTF(("svcauth_gssapi_set_names: importing %s\n", names[i].name));

	  gssstat = gss_import_name(&minor_stat, &in_buf, names[i].type,
				    &server_name_list[i]);

	  if (gssstat != GSS_S_COMPLETE) {
	       AUTH_GSSAPI_DISPLAY_STATUS(("importing name", gssstat,
					   minor_stat));
	       goto fail;
	  }

	  gssstat = gss_acquire_cred(&minor_stat, server_name_list[i], 0,
				     GSS_C_NULL_OID_SET, GSS_C_ACCEPT,
				     &server_creds_list[i], NULL, NULL);
	  if (gssstat != GSS_S_COMPLETE) {
	       AUTH_GSSAPI_DISPLAY_STATUS(("acquiring credentials",
					   gssstat, minor_stat));
	       goto fail;
	  }
     }

     return TRUE;

fail:
     svcauth_gssapi_unset_names();

     return FALSE;
}

/* Function: svcauth_gssapi_unset_names
 *
 * Purpose: releases the names and credentials allocated by
 * svcauth_gssapi_set_names
 */

void svcauth_gssapi_unset_names(void)
{
     int i;
     OM_uint32 minor_stat;

     if (server_creds_list) {
	  for (i = 0; i < server_creds_count; i++)
	       if (server_creds_list[i])
		    gss_release_cred(&minor_stat, &server_creds_list[i]);
	  free(server_creds_list);
	  server_creds_list = NULL;
     }

     if (server_name_list) {
	  for (i = 0; i < server_creds_count; i++)
	       if (server_name_list[i])
		    gss_release_name(&minor_stat, &server_name_list[i]);
	  free(server_name_list);
	  server_name_list = NULL;
     }
     server_creds_count = 0;
}


/*
 * Function: svcauth_gssapi_set_log_badauth_func
 *
 * Purpose: sets the logging function called when an invalid RPC call
 * arrives
 *
 * See functional specifications.
 */
void svcauth_gssapi_set_log_badauth_func(
     auth_gssapi_log_badauth_func func,
     caddr_t data)
{
     log_badauth = func;
     log_badauth_data = data;
}

void
svcauth_gssapi_set_log_badauth2_func(auth_gssapi_log_badauth2_func func,
				     caddr_t data)
{
     log_badauth2 = func;
     log_badauth2_data = data;
}

/*
 * Function: svcauth_gssapi_set_log_badverf_func
 *
 * Purpose: sets the logging function called when an invalid RPC call
 * arrives
 *
 * See functional specifications.
 */
void svcauth_gssapi_set_log_badverf_func(
     auth_gssapi_log_badverf_func func,
     caddr_t data)
{
     log_badverf = func;
     log_badverf_data = data;
}

/*
 * Function: svcauth_gssapi_set_log_miscerr_func
 *
 * Purpose: sets the logging function called when a miscellaneous
 * AUTH_GSSAPI error occurs
 *
 * See functional specifications.
 */
void svcauth_gssapi_set_log_miscerr_func(
     auth_gssapi_log_miscerr_func func,
     caddr_t data)
{
     log_miscerr = func;
     log_miscerr_data = data;
}

/*
 * Encrypt the serialized arguments from xdr_func applied to xdr_ptr
 * and write the result to xdrs.
 */
static bool_t svc_auth_gssapi_wrap(
     SVCAUTH *auth,
     XDR *out_xdrs,
     bool_t (*xdr_func)(),
     caddr_t xdr_ptr)
{
     OM_uint32 gssstat, minor_stat;

     if (! SVCAUTH_PRIVATE(auth)->established) {
	  PRINTF(("svc_gssapi_wrap: not established, noop\n"));
	  return (*xdr_func)(out_xdrs, xdr_ptr);
     } else if (! auth_gssapi_wrap_data(&gssstat, &minor_stat,
					SVCAUTH_PRIVATE(auth)->context,
					SVCAUTH_PRIVATE(auth)->seq_num,
					out_xdrs, xdr_func, xdr_ptr)) {
	  if (gssstat != GSS_S_COMPLETE)
	       AUTH_GSSAPI_DISPLAY_STATUS(("encrypting function arguments",
					   gssstat, minor_stat));
	  return FALSE;
     } else
	  return TRUE;
}

static bool_t svc_auth_gssapi_unwrap(
     SVCAUTH *auth,
     XDR *in_xdrs,
     bool_t (*xdr_func)(),
     caddr_t xdr_ptr)
{
     svc_auth_gssapi_data *client_data = SVCAUTH_PRIVATE(auth);
     OM_uint32 gssstat, minor_stat;

     if (! client_data->established) {
	  PRINTF(("svc_gssapi_unwrap: not established, noop\n"));
	  return (*xdr_func)(in_xdrs, (auth_gssapi_init_arg *)(void *) xdr_ptr);
     } else if (! auth_gssapi_unwrap_data(&gssstat, &minor_stat,
					  client_data->context,
					  client_data->seq_num-1,
					  in_xdrs, xdr_func, xdr_ptr)) {
	  if (gssstat != GSS_S_COMPLETE)
	       AUTH_GSSAPI_DISPLAY_STATUS(("decrypting function arguments",
					   gssstat, minor_stat));
	  return FALSE;
     } else
	  return TRUE;
}

static bool_t svc_auth_gssapi_destroy(SVCAUTH *auth)
{
     svc_auth_gssapi_data *client_data = SVCAUTH_PRIVATE(auth);

     destroy_client(client_data);
     return TRUE;
}
