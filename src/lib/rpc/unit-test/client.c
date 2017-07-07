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
static void usage()
{
     fprintf(stderr, "usage: %s {-t|-u} [-a] [-s num] [-m num] host service [count]\n",
	     whoami);
     exit(1);
}

int
main(argc, argv)
   int argc;
   char **argv;
{
     char        *host, *port, *target, *echo_arg, **echo_resp, buf[BIG_BUF];
     CLIENT      *clnt;
     AUTH	 *tmp_auth;
     struct rpc_err e;
     int auth_once, sock, use_tcp;
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

     while ((c = getopt(argc, argv, "a:m:os:tu")) != -1) {
	  switch (c) {
	  case 'a':
	       auth_debug_gssapi = atoi(optarg);
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
	  }
	  if (strncmp(*echo_resp, "Echo: ", 6) &&
	      strcmp(echo_arg, (*echo_resp) + 6) != 0)
	       fprintf(stderr, "RPC_TEST_ECHO call %d response wrong: "
		       "arg = %s, resp = %s\n", i, echo_arg, *echo_resp);
	  gssrpc_xdr_free(xdr_wrapstring, echo_resp);
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
	  gssrpc_xdr_free(xdr_wrapstring, echo_resp);
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
	  gssrpc_xdr_free(xdr_wrapstring, echo_resp);

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
	  gssrpc_xdr_free(xdr_wrapstring, echo_resp);

     /*
      * Now make sure auto-resyncrhonization actually worked
      */
     echo_arg = "testing for resynchronization";
     echo_resp = rpc_test_echo_1(&echo_arg, clnt);
     if (echo_resp == NULL)
	  clnt_perror(clnt, "Auto-resynchronization did not work");
     else
	  gssrpc_xdr_free(xdr_wrapstring, echo_resp);

     /*
      * Test fix for secure-rpc/586, part 1: btree keys must be
      * unique.  Create another context from the same credentials; it
      * should have the same expiration time and will cause the server
      * to abort if the clients are not differentiated.
      *
      * Test fix for secure-rpc/586, part 2: btree keys cannot be
      * mutated in place.  To test this: a second client, *with a
      * later expiration time*, must be run.  The second client should
      * destroy itself *after* the first one; if the key-mutating bug
      * is not fixed, the second client_data will be in the btree
      * before the first, but its key will be larger; thus, when the
      * first client calls AUTH_DESTROY, the server won't find it in
      * the btree and call abort.
      *
      * For unknown reasons, running just a second client didn't
      * tickle the bug; the btree code seemed to guess which node to
      * look at first.  Running a total of three clients does ticket
      * the bug.  Thus, the full test sequence looks like this:
      *
      * 	kinit -l 20m user && client server test@ddn 200
      * 	sleep 1
      * 	kini -l 30m user && client server test@ddn 300
      * 	sleep 1
      * 	kinit -l 40m user && client server test@ddn 400
      */
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
	       gssrpc_xdr_free(xdr_wrapstring, echo_resp);
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
