/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved.
 *
 */

#include <gssrpc/rpc.h>
#include <stdio.h>

#include <gssapi/gssapi.h>
#include <gssrpc/auth_gssapi.h>

#include "gssrpcint.h"

#ifdef __CODECENTER__
#define DEBUG_GSSAPI 1
#endif

#ifdef DEBUG_GSSAPI
int misc_debug_gssapi = DEBUG_GSSAPI;
extern void gssrpcint_printf(const char *, ...);
#define L_PRINTF(l,args) if (misc_debug_gssapi >= l) gssrpcint_printf args
#define PRINTF(args) L_PRINTF(99, args)
#define AUTH_GSSAPI_DISPLAY_STATUS(args) \
	if (misc_debug_gssapi) auth_gssapi_display_status args
#else
#define PRINTF(args)
#define L_PRINTF(l, args)
#define AUTH_GSSAPI_DISPLAY_STATUS(args)
#endif

static void auth_gssapi_display_status_1
	(char *, OM_uint32, int, int);

bool_t xdr_gss_buf(
     XDR *xdrs,
     gss_buffer_t buf)
{
     /*
      * On decode, xdr_bytes will only allocate buf->value if the
      * length read in is < maxsize (last arg).  This is dumb, because
      * the whole point of allocating memory is so that I don't *have*
      * to know the maximum length.  -1 effectively disables this
      * braindamage.
      */
     bool_t result;
     /* Fix type mismatches between APIs.  */
     unsigned int length = buf->length;
     char *cp = buf->value;
     result = xdr_bytes(xdrs, &cp, &length,
			(xdrs->x_op == XDR_DECODE && buf->value == NULL)
			? (unsigned int) -1 : (unsigned int) buf->length);
     buf->value = cp;
     buf->length = length;
     return result;
}

bool_t xdr_authgssapi_creds(
     XDR *xdrs,
     auth_gssapi_creds *creds)
{
     if (! xdr_u_int32(xdrs, &creds->version) ||
	 ! xdr_bool(xdrs, &creds->auth_msg) ||
	 ! xdr_gss_buf(xdrs, &creds->client_handle))
       return FALSE;
     return TRUE;
}

bool_t xdr_authgssapi_init_arg(
     XDR *xdrs,
     auth_gssapi_init_arg *init_arg)
{
     if (! xdr_u_int32(xdrs, &init_arg->version) ||
	 ! xdr_gss_buf(xdrs, &init_arg->token))
	  return FALSE;
     return TRUE;
}

bool_t xdr_authgssapi_init_res(
     XDR *xdrs,
     auth_gssapi_init_res *init_res)
{
     if (! xdr_u_int32(xdrs, &init_res->version) ||
	 ! xdr_gss_buf(xdrs, &init_res->client_handle) ||
	 ! xdr_u_int32(xdrs, &init_res->gss_major) ||
	 ! xdr_u_int32(xdrs, &init_res->gss_minor) ||
	 ! xdr_gss_buf(xdrs, &init_res->token) ||
	 ! xdr_gss_buf(xdrs, &init_res->signed_isn))
	  return FALSE;
     return TRUE;
}

bool_t auth_gssapi_seal_seq(
     gss_ctx_id_t context,
     uint32_t seq_num,
     gss_buffer_t out_buf)
{
     gss_buffer_desc in_buf;
     OM_uint32 gssstat, minor_stat;
     uint32_t nl_seq_num;

     nl_seq_num = htonl(seq_num);

     in_buf.length = sizeof(uint32_t);
     in_buf.value = (char *) &nl_seq_num;
     gssstat = gss_seal(&minor_stat, context, 0, GSS_C_QOP_DEFAULT,
			&in_buf, NULL, out_buf);
     if (gssstat != GSS_S_COMPLETE) {
	  PRINTF(("gssapi_seal_seq: failed\n"));
	  AUTH_GSSAPI_DISPLAY_STATUS(("sealing sequence number",
				      gssstat, minor_stat));
	  return FALSE;
     }
     return TRUE;
}

bool_t auth_gssapi_unseal_seq(
     gss_ctx_id_t context,
     gss_buffer_t in_buf,
     uint32_t *seq_num)
{
     gss_buffer_desc out_buf;
     OM_uint32 gssstat, minor_stat;
     uint32_t nl_seq_num;

     gssstat = gss_unseal(&minor_stat, context, in_buf, &out_buf,
			  NULL, NULL);
     if (gssstat != GSS_S_COMPLETE) {
	  PRINTF(("gssapi_unseal_seq: failed\n"));
	  AUTH_GSSAPI_DISPLAY_STATUS(("unsealing sequence number",
				      gssstat, minor_stat));
	  return FALSE;
     } else if (out_buf.length != sizeof(uint32_t)) {
	  PRINTF(("gssapi_unseal_seq: unseal gave %d bytes\n",
		  (int) out_buf.length));
	  gss_release_buffer(&minor_stat, &out_buf);
	  return FALSE;
     }

     nl_seq_num = *((uint32_t *) out_buf.value);
     *seq_num = (uint32_t) ntohl(nl_seq_num);
     gss_release_buffer(&minor_stat, &out_buf);

     return TRUE;
}

void auth_gssapi_display_status(
     char *msg,
     OM_uint32 major,
     OM_uint32 minor)
{
     auth_gssapi_display_status_1(msg, major, GSS_C_GSS_CODE, 0);
     auth_gssapi_display_status_1(msg, minor, GSS_C_MECH_CODE, 0);
}

static void auth_gssapi_display_status_1(
     char *m,
     OM_uint32 code,
     int type,
     int rec)
{
     OM_uint32 gssstat, minor_stat;
     gss_buffer_desc msg;
     OM_uint32 msg_ctx;

     msg_ctx = 0;
     while (1) {
	  gssstat = gss_display_status(&minor_stat, code,
				       type, GSS_C_NULL_OID,
				       &msg_ctx, &msg);
	  if (gssstat != GSS_S_COMPLETE) {
 	       if (!rec) {
		    auth_gssapi_display_status_1(m,gssstat,GSS_C_GSS_CODE,1);
		    auth_gssapi_display_status_1(m, minor_stat,
						 GSS_C_MECH_CODE, 1);
	       } else {
		   fputs ("GSS-API authentication error ", stderr);
		   fwrite (msg.value, msg.length, 1, stderr);
		   fputs (": recursive failure!\n", stderr);
	       }
	       return;
	  }

	  fprintf (stderr, "GSS-API authentication error %s: ", m);
	  fwrite (msg.value, msg.length, 1, stderr);
	  putc ('\n', stderr);
	  if (misc_debug_gssapi)
	      gssrpcint_printf("GSS-API authentication error %s: %*s\n",
			       m, (int)msg.length, (char *) msg.value);
	  (void) gss_release_buffer(&minor_stat, &msg);

	  if (!msg_ctx)
	       break;
     }
}

bool_t auth_gssapi_wrap_data(
     OM_uint32 *major,
     OM_uint32 *minor,
     gss_ctx_id_t context,
     uint32_t seq_num,
     XDR *out_xdrs,
     bool_t (*xdr_func)(),
     caddr_t xdr_ptr)
{
     gss_buffer_desc in_buf, out_buf;
     XDR temp_xdrs;
     int conf_state;
     unsigned int length;
     char *cp;

     PRINTF(("gssapi_wrap_data: starting\n"));

     *major = GSS_S_COMPLETE;
     *minor = 0; /* assumption */

     xdralloc_create(&temp_xdrs, XDR_ENCODE);

     /* serialize the sequence number into local memory */
     PRINTF(("gssapi_wrap_data: encoding seq_num %d\n", seq_num));
     if (! xdr_u_int32(&temp_xdrs, &seq_num)) {
	  PRINTF(("gssapi_wrap_data: serializing seq_num failed\n"));
	  XDR_DESTROY(&temp_xdrs);
	  return FALSE;
     }

     /* serialize the arguments into local memory */
     if (!(*xdr_func)(&temp_xdrs, xdr_ptr)) {
	  PRINTF(("gssapi_wrap_data: serializing arguments failed\n"));
	  XDR_DESTROY(&temp_xdrs);
	  return FALSE;
     }

     in_buf.length = xdr_getpos(&temp_xdrs);
     in_buf.value = xdralloc_getdata(&temp_xdrs);

     *major = gss_seal(minor, context, 1,
		       GSS_C_QOP_DEFAULT, &in_buf, &conf_state,
		       &out_buf);
     if (*major != GSS_S_COMPLETE) {
	  XDR_DESTROY(&temp_xdrs);
	  return FALSE;
     }

     PRINTF(("gssapi_wrap_data: %d bytes data, %d bytes sealed\n",
	     (int) in_buf.length, (int) out_buf.length));

     /* write the token */
     length = out_buf.length;
     cp = out_buf.value;
     if (! xdr_bytes(out_xdrs, &cp, &length, out_buf.length)) {
	  PRINTF(("gssapi_wrap_data: serializing encrypted data failed\n"));
	  XDR_DESTROY(&temp_xdrs);
	  return FALSE;
     }
     out_buf.value = cp;

     *major = gss_release_buffer(minor, &out_buf);

     PRINTF(("gssapi_wrap_data: succeeding\n\n"));
     XDR_DESTROY(&temp_xdrs);
     return TRUE;
}

bool_t auth_gssapi_unwrap_data(
     OM_uint32 *major,
     OM_uint32 *minor,
     gss_ctx_id_t context,
     uint32_t seq_num,
     XDR *in_xdrs,
     bool_t (*xdr_func)(),
     caddr_t xdr_ptr)
{
     gss_buffer_desc in_buf, out_buf;
     XDR temp_xdrs;
     uint32_t verf_seq_num;
     int conf, qop;
     unsigned int length;
     char *cp;

     PRINTF(("gssapi_unwrap_data: starting\n"));

     *major = GSS_S_COMPLETE;
     *minor = 0; /* assumption */

     in_buf.value = NULL;
     out_buf.value = NULL;
     cp = in_buf.value;
     if (! xdr_bytes(in_xdrs, &cp, &length, (unsigned int) -1)) {
	 PRINTF(("gssapi_unwrap_data: deserializing encrypted data failed\n"));
	 temp_xdrs.x_op = XDR_FREE;
	 (void)xdr_bytes(&temp_xdrs, &cp, &length, (unsigned int) -1);
	 in_buf.value = NULL;
	 return FALSE;
     }
     in_buf.value = cp;
     in_buf.length = length;

     *major = gss_unseal(minor, context, &in_buf, &out_buf, &conf,
			 &qop);
     free(in_buf.value);
     if (*major != GSS_S_COMPLETE)
	  return FALSE;

     PRINTF(("gssapi_unwrap_data: %llu bytes data, %llu bytes sealed\n",
	     (unsigned long long)out_buf.length,
	     (unsigned long long)in_buf.length));

     xdrmem_create(&temp_xdrs, out_buf.value, out_buf.length, XDR_DECODE);

     /* deserialize the sequence number */
     if (! xdr_u_int32(&temp_xdrs, &verf_seq_num)) {
	  PRINTF(("gssapi_unwrap_data: deserializing verf_seq_num failed\n"));
	  gss_release_buffer(minor, &out_buf);
	  XDR_DESTROY(&temp_xdrs);
	  return FALSE;
     }
     if (verf_seq_num != seq_num) {
	  PRINTF(("gssapi_unwrap_data: seq %d specified, read %d\n",
		  seq_num, verf_seq_num));
	  gss_release_buffer(minor, &out_buf);
	  XDR_DESTROY(&temp_xdrs);
	  return FALSE;
     }
     PRINTF(("gssapi_unwrap_data: unwrap seq_num %d okay\n", verf_seq_num));

     /* deserialize the arguments into xdr_ptr */
     if (! (*xdr_func)(&temp_xdrs, xdr_ptr)) {
	  PRINTF(("gssapi_unwrap_data: deserializing arguments failed\n"));
	  gss_release_buffer(minor, &out_buf);
	  XDR_DESTROY(&temp_xdrs);
	  return FALSE;
     }

     PRINTF(("gssapi_unwrap_data: succeeding\n\n"));

     gss_release_buffer(minor, &out_buf);
     XDR_DESTROY(&temp_xdrs);
     return TRUE;
}
