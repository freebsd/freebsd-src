/* lib/rpc/svc_auth_gss.c */
/*
  Copyright (c) 2000 The Regents of the University of Michigan.
  All rights reserved.

  Copyright (c) 2000 Dug Song <dugsong@UMICH.EDU>.
  All rights reserved, all wrongs reversed.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of the University nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  Id: svc_auth_gss.c,v 1.28 2002/10/15 21:29:36 kwc Exp
 */

#include "k5-platform.h"
#include <gssrpc/rpc.h>
#include <gssrpc/auth_gssapi.h>
#ifdef HAVE_HEIMDAL
#include <gssapi.h>
#define gss_nt_service_name GSS_C_NT_HOSTBASED_SERVICE
#else
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_generic.h>
#endif

#ifdef DEBUG_GSSAPI
int svc_debug_gss = DEBUG_GSSAPI;
#endif

#ifdef SPKM

#ifndef OID_EQ
#define g_OID_equal(o1,o2) \
   (((o1)->length == (o2)->length) && \
    ((o1)->elements != 0) && ((o2)->elements != 0) && \
    (memcmp((o1)->elements,(o2)->elements,(int) (o1)->length) == 0))
#define OID_EQ 1
#endif /* OID_EQ */

extern const gss_OID_desc * const gss_mech_spkm3;

#endif /* SPKM */

extern SVCAUTH svc_auth_none;

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

static bool_t	svcauth_gss_destroy(SVCAUTH *);
static bool_t   svcauth_gss_wrap(SVCAUTH *, XDR *, xdrproc_t, caddr_t);
static bool_t   svcauth_gss_unwrap(SVCAUTH *, XDR *, xdrproc_t, caddr_t);

static bool_t	svcauth_gss_nextverf(struct svc_req *, u_int);

struct svc_auth_ops svc_auth_gss_ops = {
	svcauth_gss_wrap,
	svcauth_gss_unwrap,
	svcauth_gss_destroy
};

struct svc_rpc_gss_data {
	bool_t			established;	/* context established */
	gss_cred_id_t		cred;		/* credential */
	gss_ctx_id_t		ctx;		/* context id */
	struct rpc_gss_sec	sec;		/* security triple */
	gss_buffer_desc		cname;		/* GSS client name */
	u_int			seq;		/* sequence number */
	u_int			win;		/* sequence window */
	u_int			seqlast;	/* last sequence number */
	uint32_t		seqmask;	/* bitmask of seqnums */
	gss_name_t		client_name;	/* unparsed name string */
	gss_buffer_desc		checksum;	/* so we can free it */
};

#define SVCAUTH_PRIVATE(auth) \
	(*(struct svc_rpc_gss_data **)&(auth)->svc_ah_private)

/* Global server credentials. */
static gss_name_t	svcauth_gss_name = NULL;

bool_t
svcauth_gss_set_svc_name(gss_name_t name)
{
	OM_uint32	maj_stat, min_stat;

	log_debug("in svcauth_gss_set_svc_name()");

	if (svcauth_gss_name != NULL) {
		maj_stat = gss_release_name(&min_stat, &svcauth_gss_name);

		if (maj_stat != GSS_S_COMPLETE) {
			log_status("gss_release_name", maj_stat, min_stat);
			return (FALSE);
		}
		svcauth_gss_name = NULL;
	}
	if (svcauth_gss_name == GSS_C_NO_NAME)
		return (TRUE);

	maj_stat = gss_duplicate_name(&min_stat, name, &svcauth_gss_name);

	if (maj_stat != GSS_S_COMPLETE) {
		log_status("gss_duplicate_name", maj_stat, min_stat);
		return (FALSE);
	}

	return (TRUE);
}

static bool_t
svcauth_gss_acquire_cred(struct svc_rpc_gss_data *gd)
{
	OM_uint32	maj_stat, min_stat;

	log_debug("in svcauth_gss_acquire_cred()");

	/* We don't need to acquire a credential if using the default name. */
	if (svcauth_gss_name == GSS_C_NO_NAME)
		return (TRUE);

	/* Only acquire a credential once per authentication. */
	if (gd->cred != GSS_C_NO_CREDENTIAL)
		return (TRUE);

	maj_stat = gss_acquire_cred(&min_stat, svcauth_gss_name, 0,
				    GSS_C_NULL_OID_SET, GSS_C_ACCEPT,
				    &gd->cred, NULL, NULL);

	if (maj_stat != GSS_S_COMPLETE) {
		log_status("gss_acquire_cred", maj_stat, min_stat);
		return (FALSE);
	}
	return (TRUE);
}

/* Invoke log_badauth callbacks for an authentication failure. */
static void
badauth(OM_uint32 maj, OM_uint32 minor, SVCXPRT *xprt)
{
	if (log_badauth != NULL)
		(*log_badauth)(maj, minor, &xprt->xp_raddr, log_badauth_data);
	if (log_badauth2 != NULL)
		(*log_badauth2)(maj, minor, xprt, log_badauth2_data);
}

static bool_t
svcauth_gss_accept_sec_context(struct svc_req *rqst,
			       struct rpc_gss_init_res *gr)
{
	struct svc_rpc_gss_data	*gd;
	struct rpc_gss_cred	*gc;
	gss_buffer_desc		 recv_tok, seqbuf;
	gss_OID			 mech;
	OM_uint32		 maj_stat = 0, min_stat = 0, ret_flags, seq;

	log_debug("in svcauth_gss_accept_context()");

	gd = SVCAUTH_PRIVATE(rqst->rq_xprt->xp_auth);
	gc = (struct rpc_gss_cred *)rqst->rq_clntcred;
	memset(gr, 0, sizeof(*gr));

	/* Deserialize arguments. */
	memset(&recv_tok, 0, sizeof(recv_tok));

	if (!svc_getargs(rqst->rq_xprt, xdr_rpc_gss_init_args,
			 (caddr_t)&recv_tok))
		return (FALSE);

	gr->gr_major = gss_accept_sec_context(&gr->gr_minor,
					      &gd->ctx,
					      gd->cred,
					      &recv_tok,
					      GSS_C_NO_CHANNEL_BINDINGS,
					      &gd->client_name,
					      &mech,
					      &gr->gr_token,
					      &ret_flags,
					      NULL,
					      NULL);

	svc_freeargs(rqst->rq_xprt, xdr_rpc_gss_init_args, (caddr_t)&recv_tok);

	log_status("accept_sec_context", gr->gr_major, gr->gr_minor);
	if (gr->gr_major != GSS_S_COMPLETE &&
	    gr->gr_major != GSS_S_CONTINUE_NEEDED) {
		badauth(gr->gr_major, gr->gr_minor, rqst->rq_xprt);
		gd->ctx = GSS_C_NO_CONTEXT;
		goto errout;
	}
	gr->gr_ctx.value = "xxxx";
	gr->gr_ctx.length = 4;

	/* gr->gr_win = 0x00000005; ANDROS: for debugging linux kernel version...  */
	gr->gr_win = sizeof(gd->seqmask) * 8;

	/* Save client info. */
	gd->sec.mech = mech;
	gd->sec.qop = GSS_C_QOP_DEFAULT;
	gd->sec.svc = gc->gc_svc;
	gd->seq = gc->gc_seq;
	gd->win = gr->gr_win;

	if (gr->gr_major == GSS_S_COMPLETE) {
#ifdef SPKM
		/* spkm3: no src_name (anonymous) */
		if(!g_OID_equal(gss_mech_spkm3, mech)) {
#endif
		    maj_stat = gss_display_name(&min_stat, gd->client_name,
					    &gd->cname, &gd->sec.mech);
#ifdef SPKM
		}
#endif
		if (maj_stat != GSS_S_COMPLETE) {
			log_status("display_name", maj_stat, min_stat);
			goto errout;
		}
#ifdef DEBUG
#ifdef HAVE_HEIMDAL
		log_debug("accepted context for %.*s with "
			  "<mech {}, qop %d, svc %d>",
			  gd->cname.length, (char *)gd->cname.value,
			  gd->sec.qop, gd->sec.svc);
#else
		{
			gss_buffer_desc mechname;

			gss_oid_to_str(&min_stat, mech, &mechname);

			log_debug("accepted context for %.*s with "
				  "<mech %.*s, qop %d, svc %d>",
				  gd->cname.length, (char *)gd->cname.value,
				  mechname.length, (char *)mechname.value,
				  gd->sec.qop, gd->sec.svc);

			gss_release_buffer(&min_stat, &mechname);
		}
#endif
#endif /* DEBUG */
		seq = htonl(gr->gr_win);
		seqbuf.value = &seq;
		seqbuf.length = sizeof(seq);

		gss_release_buffer(&min_stat, &gd->checksum);
		maj_stat = gss_sign(&min_stat, gd->ctx, GSS_C_QOP_DEFAULT,
				    &seqbuf, &gd->checksum);

		if (maj_stat != GSS_S_COMPLETE) {
			goto errout;
		}


		rqst->rq_xprt->xp_verf.oa_flavor = RPCSEC_GSS;
		rqst->rq_xprt->xp_verf.oa_base = gd->checksum.value;
		rqst->rq_xprt->xp_verf.oa_length = gd->checksum.length;
	}
	return (TRUE);
errout:
	gss_release_buffer(&min_stat, &gr->gr_token);
	return (FALSE);
}

static bool_t
svcauth_gss_validate(struct svc_req *rqst, struct svc_rpc_gss_data *gd, struct rpc_msg *msg)
{
	struct opaque_auth	*oa;
	gss_buffer_desc		 rpcbuf, checksum;
	OM_uint32		 maj_stat, min_stat, qop_state;
	u_char			 rpchdr[128];
	int32_t			*buf;

	log_debug("in svcauth_gss_validate()");

	memset(rpchdr, 0, sizeof(rpchdr));

	/* XXX - Reconstruct RPC header for signing (from xdr_callmsg). */
	oa = &msg->rm_call.cb_cred;
	if (oa->oa_length > MAX_AUTH_BYTES)
		return (FALSE);

	/* 8 XDR units from the IXDR macro calls. */
	if (sizeof(rpchdr) < (8 * BYTES_PER_XDR_UNIT +
			      RNDUP(oa->oa_length)))
		return (FALSE);

	buf = (int32_t *)(void *)rpchdr;
	IXDR_PUT_LONG(buf, msg->rm_xid);
	IXDR_PUT_ENUM(buf, msg->rm_direction);
	IXDR_PUT_LONG(buf, msg->rm_call.cb_rpcvers);
	IXDR_PUT_LONG(buf, msg->rm_call.cb_prog);
	IXDR_PUT_LONG(buf, msg->rm_call.cb_vers);
	IXDR_PUT_LONG(buf, msg->rm_call.cb_proc);
	IXDR_PUT_ENUM(buf, oa->oa_flavor);
	IXDR_PUT_LONG(buf, oa->oa_length);
	if (oa->oa_length) {
		memcpy((caddr_t)buf, oa->oa_base, oa->oa_length);
		buf += RNDUP(oa->oa_length) / sizeof(int32_t);
	}
	rpcbuf.value = rpchdr;
	rpcbuf.length = (u_char *)buf - rpchdr;

	checksum.value = msg->rm_call.cb_verf.oa_base;
	checksum.length = msg->rm_call.cb_verf.oa_length;

	maj_stat = gss_verify_mic(&min_stat, gd->ctx, &rpcbuf, &checksum,
				  &qop_state);

	if (maj_stat != GSS_S_COMPLETE) {
		log_status("gss_verify_mic", maj_stat, min_stat);
		if (log_badverf != NULL)
			(*log_badverf)(gd->client_name,
			       svcauth_gss_name,
			       rqst, msg, log_badverf_data);
		return (FALSE);
	}
	return (TRUE);
}

static bool_t
svcauth_gss_nextverf(struct svc_req *rqst, u_int num)
{
	struct svc_rpc_gss_data	*gd;
	gss_buffer_desc		 signbuf;
	OM_uint32		 maj_stat, min_stat;

	log_debug("in svcauth_gss_nextverf()");

	if (rqst->rq_xprt->xp_auth == NULL)
		return (FALSE);

	gd = SVCAUTH_PRIVATE(rqst->rq_xprt->xp_auth);

	gss_release_buffer(&min_stat, &gd->checksum);

	signbuf.value = &num;
	signbuf.length = sizeof(num);

	maj_stat = gss_get_mic(&min_stat, gd->ctx, gd->sec.qop,
			       &signbuf, &gd->checksum);

	if (maj_stat != GSS_S_COMPLETE) {
		log_status("gss_get_mic", maj_stat, min_stat);
		return (FALSE);
	}
	rqst->rq_xprt->xp_verf.oa_flavor = RPCSEC_GSS;
	rqst->rq_xprt->xp_verf.oa_base = (caddr_t)gd->checksum.value;
	rqst->rq_xprt->xp_verf.oa_length = (u_int)gd->checksum.length;

	return (TRUE);
}

enum auth_stat
gssrpc__svcauth_gss(struct svc_req *rqst, struct rpc_msg *msg,
	bool_t *no_dispatch)
{
	enum auth_stat		 retstat;
	XDR	 		 xdrs;
	SVCAUTH			*auth;
	struct svc_rpc_gss_data	*gd;
	struct rpc_gss_cred	*gc;
	struct rpc_gss_init_res	 gr;
	int			 call_stat, offset;
	OM_uint32		 min_stat;

	log_debug("in svcauth_gss()");

	/* Initialize reply. */
	rqst->rq_xprt->xp_verf = gssrpc__null_auth;

	/* Allocate and set up server auth handle. */
	if (rqst->rq_xprt->xp_auth == NULL ||
	    rqst->rq_xprt->xp_auth == &svc_auth_none) {
		if ((auth = calloc(sizeof(*auth), 1)) == NULL) {
			fprintf(stderr, "svcauth_gss: out_of_memory\n");
			return (AUTH_FAILED);
		}
		if ((gd = calloc(sizeof(*gd), 1)) == NULL) {
			fprintf(stderr, "svcauth_gss: out_of_memory\n");
			return (AUTH_FAILED);
		}
		auth->svc_ah_ops = &svc_auth_gss_ops;
		SVCAUTH_PRIVATE(auth) = gd;
		rqst->rq_xprt->xp_auth = auth;
	}
	else gd = SVCAUTH_PRIVATE(rqst->rq_xprt->xp_auth);

	log_debug("xp_auth=%p, gd=%p", rqst->rq_xprt->xp_auth, gd);

	/* Deserialize client credentials. */
	if (rqst->rq_cred.oa_length <= 0)
		return (AUTH_BADCRED);

	gc = (struct rpc_gss_cred *)rqst->rq_clntcred;
	memset(gc, 0, sizeof(*gc));

	log_debug("calling xdrmem_create()");
	log_debug("oa_base=%p, oa_length=%u", rqst->rq_cred.oa_base,
		  rqst->rq_cred.oa_length);
	xdrmem_create(&xdrs, rqst->rq_cred.oa_base,
		      rqst->rq_cred.oa_length, XDR_DECODE);
	log_debug("xdrmem_create() returned");

	if (!xdr_rpc_gss_cred(&xdrs, gc)) {
		log_debug("xdr_rpc_gss_cred() failed");
		XDR_DESTROY(&xdrs);
		return (AUTH_BADCRED);
	}
	XDR_DESTROY(&xdrs);

	retstat = AUTH_FAILED;

#define ret_freegc(code) do { retstat = code; goto freegc; } while (0)

	/* Check version. */
	if (gc->gc_v != RPCSEC_GSS_VERSION)
		ret_freegc (AUTH_BADCRED);

	/* Check RPCSEC_GSS service. */
	if (gc->gc_svc != RPCSEC_GSS_SVC_NONE &&
	    gc->gc_svc != RPCSEC_GSS_SVC_INTEGRITY &&
	    gc->gc_svc != RPCSEC_GSS_SVC_PRIVACY)
		ret_freegc (AUTH_BADCRED);

	/* Check sequence number. */
	if (gd->established) {
		if (gc->gc_seq > MAXSEQ)
			ret_freegc (RPCSEC_GSS_CTXPROBLEM);

		if ((offset = gd->seqlast - gc->gc_seq) < 0) {
			gd->seqlast = gc->gc_seq;
			offset = 0 - offset;
			gd->seqmask <<= offset;
			offset = 0;
		} else if ((u_int)offset >= gd->win ||
			   (gd->seqmask & (1 << offset))) {
			*no_dispatch = 1;
			ret_freegc (RPCSEC_GSS_CTXPROBLEM);
		}
		gd->seq = gc->gc_seq;
		gd->seqmask |= (1 << offset);
	}

	if (gd->established) {
		rqst->rq_clntname = (char *)gd->client_name;
		rqst->rq_svccred = (char *)gd->ctx;
	}

	/* Handle RPCSEC_GSS control procedure. */
	switch (gc->gc_proc) {

	case RPCSEC_GSS_INIT:
	case RPCSEC_GSS_CONTINUE_INIT:
		if (rqst->rq_proc != NULLPROC)
			ret_freegc (AUTH_FAILED);		/* XXX ? */

		if (!svcauth_gss_acquire_cred(gd))
			ret_freegc (AUTH_FAILED);

		if (!svcauth_gss_accept_sec_context(rqst, &gr))
			ret_freegc (AUTH_REJECTEDCRED);

		if (!svcauth_gss_nextverf(rqst, htonl(gr.gr_win))) {
			gss_release_buffer(&min_stat, &gr.gr_token);
			ret_freegc (AUTH_FAILED);
		}
		*no_dispatch = TRUE;

		call_stat = svc_sendreply(rqst->rq_xprt, xdr_rpc_gss_init_res,
					  (caddr_t)&gr);

		gss_release_buffer(&min_stat, &gr.gr_token);
		gss_release_buffer(&min_stat, &gd->checksum);
		if (!call_stat)
			ret_freegc (AUTH_FAILED);

		if (gr.gr_major == GSS_S_COMPLETE)
			gd->established = TRUE;

		break;

	case RPCSEC_GSS_DATA:
		if (!svcauth_gss_validate(rqst, gd, msg))
			ret_freegc (RPCSEC_GSS_CREDPROBLEM);

		if (!svcauth_gss_nextverf(rqst, htonl(gc->gc_seq)))
 			ret_freegc (AUTH_FAILED);
		break;

	case RPCSEC_GSS_DESTROY:
		if (rqst->rq_proc != NULLPROC)
			ret_freegc (AUTH_FAILED);		/* XXX ? */

		if (!svcauth_gss_validate(rqst, gd, msg))
			ret_freegc (RPCSEC_GSS_CREDPROBLEM);

		if (!svcauth_gss_nextverf(rqst, htonl(gc->gc_seq)))
			ret_freegc (AUTH_FAILED);

		*no_dispatch = TRUE;

		call_stat = svc_sendreply(rqst->rq_xprt,
					  xdr_void, (caddr_t)NULL);

		log_debug("sendreply in destroy: %d", call_stat);

		SVCAUTH_DESTROY(rqst->rq_xprt->xp_auth);
		rqst->rq_xprt->xp_auth = &svc_auth_none;

		break;

	default:
		ret_freegc (AUTH_REJECTEDCRED);
		break;
	}
	retstat = AUTH_OK;
freegc:
	xdr_free(xdr_rpc_gss_cred, gc);
	log_debug("returning %d from svcauth_gss()", retstat);
	return (retstat);
}

static bool_t
svcauth_gss_destroy(SVCAUTH *auth)
{
	struct svc_rpc_gss_data	*gd;
	OM_uint32		 min_stat;

	log_debug("in svcauth_gss_destroy()");

	gd = SVCAUTH_PRIVATE(auth);

	gss_delete_sec_context(&min_stat, &gd->ctx, GSS_C_NO_BUFFER);
	gss_release_cred(&min_stat, &gd->cred);
	gss_release_buffer(&min_stat, &gd->cname);
	gss_release_buffer(&min_stat, &gd->checksum);

	if (gd->client_name)
		gss_release_name(&min_stat, &gd->client_name);

	mem_free(gd, sizeof(*gd));
	mem_free(auth, sizeof(*auth));

	return (TRUE);
}

static bool_t
svcauth_gss_wrap(SVCAUTH *auth, XDR *xdrs, xdrproc_t xdr_func, caddr_t xdr_ptr)
{
	struct svc_rpc_gss_data	*gd;

	log_debug("in svcauth_gss_wrap()");

	gd = SVCAUTH_PRIVATE(auth);

	if (!gd->established || gd->sec.svc == RPCSEC_GSS_SVC_NONE) {
		return ((*xdr_func)(xdrs, xdr_ptr));
	}
	return (xdr_rpc_gss_data(xdrs, xdr_func, xdr_ptr,
				 gd->ctx, gd->sec.qop,
				 gd->sec.svc, gd->seq));
}

static bool_t
svcauth_gss_unwrap(SVCAUTH *auth, XDR *xdrs, xdrproc_t xdr_func, caddr_t xdr_ptr)
{
	struct svc_rpc_gss_data	*gd;

	log_debug("in svcauth_gss_unwrap()");

	gd = SVCAUTH_PRIVATE(auth);

	if (!gd->established || gd->sec.svc == RPCSEC_GSS_SVC_NONE) {
		return ((*xdr_func)(xdrs, xdr_ptr));
	}
	return (xdr_rpc_gss_data(xdrs, xdr_func, xdr_ptr,
				 gd->ctx, gd->sec.qop,
				 gd->sec.svc, gd->seq));
}

char *
svcauth_gss_get_principal(SVCAUTH *auth)
{
	struct svc_rpc_gss_data *gd;
	char *pname;

	gd = SVCAUTH_PRIVATE(auth);

	if (gd->cname.length == 0 || gd->cname.length >= SIZE_MAX)
		return (NULL);

	if ((pname = malloc(gd->cname.length + 1)) == NULL)
		return (NULL);

	memcpy(pname, gd->cname.value, gd->cname.length);
	pname[gd->cname.length] = '\0';

	return (pname);
}

/*
 * Function: svcauth_gss_set_log_badauth_func
 *
 * Purpose: sets the logging function called when an invalid RPC call
 * arrives
 *
 * See functional specifications.
 */
void svcauth_gss_set_log_badauth_func(
	auth_gssapi_log_badauth_func func,
	caddr_t data)
{
	log_badauth = func;
	log_badauth_data = data;
}

void
svcauth_gss_set_log_badauth2_func(auth_gssapi_log_badauth2_func func,
				  caddr_t data)
{
	log_badauth2 = func;
	log_badauth2_data = data;
}

/*
 * Function: svcauth_gss_set_log_badverf_func
 *
 * Purpose: sets the logging function called when an invalid RPC call
 * arrives
 *
 * See functional specifications.
 */
void svcauth_gss_set_log_badverf_func(
	auth_gssapi_log_badverf_func func,
	caddr_t data)
{
	log_badverf = func;
	log_badverf_data = data;
}

/*
 * Function: svcauth_gss_set_log_miscerr_func
 *
 * Purpose: sets the logging function called when a miscellaneous
 * AUTH_GSSAPI error occurs
 *
 * See functional specifications.
 */
void svcauth_gss_set_log_miscerr_func(
	auth_gssapi_log_miscerr_func func,
	caddr_t data)
{
	log_miscerr = func;
	log_miscerr_data = data;
}
