/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved.
 *
 * $Id$
 *
 */

#include "autoconf.h"
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <gssrpc/rpc.h>
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_krb5.h>
#include <gssrpc/rpc.h>
#include <gssrpc/auth_gssapi.h>
#include <gssrpc/auth_gss.h>
#include "rpc_test.h"

#define BIG_BUF 4096
/* copied from auth_gssapi.c for hackery */
struct auth_gssapi_data {
     bool_t established;
     CLIENT *clnt;
     gss_ctx_id_t context;
     gss_buffer_desc client_handle;
     OM_uint32 seq_num;
     int def_cred;

     /* pre-serialized ah_cred */
     u_char cred_buf[MAX_AUTH_BYTES];
     int32_t cred_len;
};
#define AUTH_PRIVATE(auth) ((struct auth_gssapi_data *)auth->ah_private)

extern int auth_debug_gssapi;
char *whoami;

#ifdef __GNUC__
__attribute__((noreturn))
#endif
static void usage(void)
{
     fprintf(stderr, "usage: %s {-t|-u} [-a] [-g] [-s num] [-m num] host service [count]\n",
	     whoami);
     exit(1);
}

/*
 * Establish an RPCSEC_GSS context using the service init_svc, then make
 * an echo call with each service level.  Return the number of failed
 * calls.
 */
static int
rpcsec_gss_svc_test(CLIENT *clnt, char *target, rpc_gss_svc_t init_svc)
{
     static const rpc_gss_svc_t svcs[] = {
	  RPCSEC_GSS_SVC_NONE, RPCSEC_GSS_SVC_INTEGRITY,
	  RPCSEC_GSS_SVC_PRIVACY
     };
     struct rpc_gss_sec sec = { 0 };
     char buf[64], *echo_arg = buf, **echo_resp;
     int failures = 0;
     size_t i;

     sec.mech = (gss_OID)gss_mech_krb5;
     sec.qop = GSS_C_QOP_DEFAULT;
     sec.svc = init_svc;
     sec.cred = GSS_C_NO_CREDENTIAL;
     sec.req_flags = GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG;
     clnt->cl_auth = authgss_create_default(clnt, target, &sec);
     if (clnt->cl_auth == NULL) {
	  clnt_pcreateerror(whoami);
	  exit(2);
     }

     for (i = 0; i < sizeof(svcs) / sizeof(*svcs); i++) {
	  if (!authgss_service(clnt->cl_auth, svcs[i])) {
	       fprintf(stderr, "authgss_service %d failed\n", (int)svcs[i]);
	       failures++;
	       continue;
	  }
	  snprintf(buf, sizeof(buf), "init svc %d, call svc %d",
		   (int)init_svc, (int)svcs[i]);
	  echo_resp = rpc_test_echo_1(&echo_arg, clnt);
	  if (echo_resp == NULL) {
	       fprintf(stderr, "RPCSEC_GSS echo (%s)%s", buf,
		       clnt_sperror(clnt, ""));
	       failures++;
	       continue;
	  }
	  if (strncmp(*echo_resp, "Echo: ", 6) != 0 ||
	      strcmp(echo_arg, *echo_resp + 6) != 0) {
	       fprintf(stderr, "RPCSEC_GSS echo (%s) response wrong: %s\n",
		       buf, *echo_resp);
	       failures++;
	  }
	  gssrpc_xdr_free((xdrproc_t)xdr_wrapstring, echo_resp);
     }

     AUTH_DESTROY(clnt->cl_auth);
     clnt->cl_auth = NULL;
     return failures;
}

/* Check that each RPCSEC_GSS call is protected with the service level
 * it names, whatever level was used to establish the context. */
static int
rpcsec_gss_test(CLIENT *clnt, char *target)
{
     int failures = 0;

     failures += rpcsec_gss_svc_test(clnt, target, RPCSEC_GSS_SVC_NONE);
     failures += rpcsec_gss_svc_test(clnt, target, RPCSEC_GSS_SVC_INTEGRITY);
     failures += rpcsec_gss_svc_test(clnt, target, RPCSEC_GSS_SVC_PRIVACY);
     return failures;
}

int
main(int argc, char **argv)
{
     char        *host, *port, *target, *echo_arg, **echo_resp, buf[BIG_BUF];
     CLIENT      *clnt;
     AUTH	 *tmp_auth;
     struct rpc_err e;
     int auth_once, sock, use_tcp, use_rpcsec_gss;
     unsigned int count, i;
     extern int optind;
     extern char *optarg;
     extern int svc_debug_gssapi, misc_debug_gssapi, auth_debug_gssapi;
     int c;
     struct sockaddr_in sin;
     struct hostent *h;
     struct timeval tv;

     extern int krb5_gss_dbg_client_expcreds;
     krb5_gss_dbg_client_expcreds = 1;

     whoami = argv[0];
     count = 1026;
     auth_once = 0;
     use_tcp = -1;
     use_rpcsec_gss = 0;

     while ((c = getopt(argc, argv, "a:gm:os:tu")) != -1) {
	  switch (c) {
	  case 'a':
	       auth_debug_gssapi = atoi(optarg);
	       break;
	  case 'g':
	       use_rpcsec_gss = 1;
	       break;
	  case 'm':
	       misc_debug_gssapi = atoi(optarg);
	       break;
	  case 'o':
	       auth_once++;
	       break;
	  case 's':
	       svc_debug_gssapi = atoi(optarg);
	       break;
	  case 't':
	       use_tcp = 1;
	       break;
	  case 'u':
	       use_tcp = 0;
	       break;
	  case '?':
	       usage();
	       break;
	  }
     }
     if (use_tcp == -1)
	  usage();

     argv += optind;
     argc -= optind;

     switch (argc) {
     case 4:
	  count = atoi(argv[3]);
	  if (count > BIG_BUF-1) {
	    fprintf(stderr, "Test count cannot exceed %d.\n", BIG_BUF-1);
	    usage();
	  }
     case 3:
	  host = argv[0];
	  port = argv[1];
	  target = argv[2];
	  break;
     default:
	  usage();
     }

     /* get server address */
     h = gethostbyname(host);
     if (h == NULL) {
	 fprintf(stderr, "Can't resolve hostname %s\n", host);
	 exit(1);
     }
     memset(&sin, 0, sizeof(sin));
     sin.sin_family = h->h_addrtype;
     sin.sin_port = ntohs(atoi(port));
     memmove(&sin.sin_addr, h->h_addr, sizeof(sin.sin_addr));

     /* client handle to rstat */
     sock = RPC_ANYSOCK;
     if (use_tcp) {
	 clnt = clnttcp_create(&sin, RPC_TEST_PROG, RPC_TEST_VERS_1, &sock, 0,
			       0);
     } else {
	 tv.tv_sec = 5;
	 tv.tv_usec = 0;
	 clnt = clntudp_create(&sin, RPC_TEST_PROG, RPC_TEST_VERS_1, tv,
			       &sock);
     }
     if (clnt == NULL) {
	  clnt_pcreateerror(whoami);
	  exit(1);
     }

     if (use_rpcsec_gss) {
	  if (rpcsec_gss_test(clnt, target) != 0)
	       exit(1);
	  CLNT_DESTROY(clnt);
	  exit(0);
     }

     clnt->cl_auth = auth_gssapi_create_default(clnt, target);
     if (clnt->cl_auth == NULL) {
	  clnt_pcreateerror(whoami);
	  exit(2);
     }

     /*
      * Call the echo service multiple times.
      */
     echo_arg = buf;
     for (i = 0; i < 3; i++) {
	  snprintf(buf, sizeof(buf), "testing %d\n", i);

	  echo_resp = rpc_test_echo_1(&echo_arg, clnt);
	  if (echo_resp == NULL) {
	       fprintf(stderr, "RPC_TEST_ECHO call %d%s", i,
		       clnt_sperror(clnt, ""));
	       break;
	  }
	  if (strncmp(*echo_resp, "Echo: ", 6) &&
	      strcmp(echo_arg, (*echo_resp) + 6) != 0)
	       fprintf(stderr, "RPC_TEST_ECHO call %d response wrong: "
		       "arg = %s, resp = %s\n", i, echo_arg, *echo_resp);
	  gssrpc_xdr_free((xdrproc_t)xdr_wrapstring, echo_resp);
     }

     /*
      * Make a call with an invalid verifier and check for error;
      * server should log error message.  It is important to
      *increment* seq_num here, since a decrement would be fixed (see
      * below).  Note that seq_num will be incremented (by
      * authg_gssapi_refresh) twice, so we need to decrement by three
      * to reset.
      */
     AUTH_PRIVATE(clnt->cl_auth)->seq_num++;

     echo_arg = "testing with bad verf";

     echo_resp = rpc_test_echo_1(&echo_arg, clnt);
     if (echo_resp == NULL) {
	  CLNT_GETERR(clnt, &e);
	  if (e.re_status != RPC_AUTHERROR || e.re_why != AUTH_REJECTEDVERF)
	       clnt_perror(clnt, whoami);
     } else {
	  fprintf(stderr, "bad seq didn't cause failure\n");
	  gssrpc_xdr_free((xdrproc_t)xdr_wrapstring, echo_resp);
     }

     AUTH_PRIVATE(clnt->cl_auth)->seq_num -= 3;

     /*
      * Make sure we're resyncronized.
      */
     echo_arg = "testing for reset";
     echo_resp = rpc_test_echo_1(&echo_arg, clnt);
     if (echo_resp == NULL)
	  clnt_perror(clnt, "Sequence number improperly reset");
     else
	  gssrpc_xdr_free((xdrproc_t)xdr_wrapstring, echo_resp);

     /*
      * Now simulate a lost server response, and see if
      * auth_gssapi_refresh recovers.
      */
     AUTH_PRIVATE(clnt->cl_auth)->seq_num--;
     echo_arg = "forcing auto-resynchronization";
     echo_resp = rpc_test_echo_1(&echo_arg, clnt);
     if (echo_resp == NULL)
	  clnt_perror(clnt, "Auto-resynchronization failed");
     else
	  gssrpc_xdr_free((xdrproc_t)xdr_wrapstring, echo_resp);

     /*
      * Now make sure auto-resyncrhonization actually worked
      */
     echo_arg = "testing for resynchronization";
     echo_resp = rpc_test_echo_1(&echo_arg, clnt);
     if (echo_resp == NULL)
	  clnt_perror(clnt, "Auto-resynchronization did not work");
     else
	  gssrpc_xdr_free((xdrproc_t)xdr_wrapstring, echo_resp);

     if (! auth_once) {
	  tmp_auth = clnt->cl_auth;
	  clnt->cl_auth = auth_gssapi_create_default(clnt, target);
	  if (clnt->cl_auth == NULL) {
	       clnt_pcreateerror(whoami);
	       exit(2);
	  }
	  AUTH_DESTROY(clnt->cl_auth);
	  clnt->cl_auth = tmp_auth;
     }

     /*
      * Try RPC calls with argument/result lengths [0, 1025].  Do
      * this last, since it takes a while..
      */
     echo_arg = buf;
     memset(buf, 0, count+1);
     for (i = 0; i < count; i++) {
	  echo_resp = rpc_test_echo_1(&echo_arg, clnt);
	  if (echo_resp == NULL) {
	       fprintf(stderr, "RPC_TEST_LENGTHS call %d%s", i,
		       clnt_sperror(clnt, ""));
	       break;
	  } else {
	       if (strncmp(*echo_resp, "Echo: ", 6) &&
		   strcmp(echo_arg, (*echo_resp) + 6) != 0)
		    fprintf(stderr,
			    "RPC_TEST_LENGTHS call %d response wrong\n", i);
	       gssrpc_xdr_free((xdrproc_t)xdr_wrapstring, echo_resp);
	  }

	  /* cycle from 1 to 255 */
	  buf[i] = (i % 255) + 1;

	  if (i % 100 == 0) {
	       fputc('.', stdout);
	       fflush(stdout);
	  }
     }
     fputc('\n', stdout);

     AUTH_DESTROY(clnt->cl_auth);
     CLNT_DESTROY(clnt);
     exit(0);
}
