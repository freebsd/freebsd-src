#include "rpc_test.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> /* getenv, exit */
#include <sys/types.h>
#include <syslog.h>

/* States a server can be in wrt request */

#define	_IDLE 0
#define	_SERVED 1

static int _rpcsvcstate = _IDLE;	/* Set when a request is serviced */
static int _rpcsvccount = 0;		/* Number of requests being serviced */

void
rpc_test_prog_1_svc(struct svc_req *rqstp, SVCXPRT *transp)
{
	union {
		char *rpc_test_echo_1_arg;
	} argument;
	char *result;
	xdrproc_t xdr_argument, xdr_result;
	char *(*local)(char *, struct svc_req *);

	_rpcsvccount++;
	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply(transp, xdr_void,
			(char *)NULL);
		_rpcsvccount--;
		_rpcsvcstate = _SERVED;
		return;

	case RPC_TEST_ECHO:
		xdr_argument = (xdrproc_t)xdr_wrapstring;
		xdr_result = (xdrproc_t)xdr_wrapstring;
		local = (char *(*)(char *, struct svc_req *)) rpc_test_echo_1_svc;
		break;

	default:
		svcerr_noproc(transp);
		_rpcsvccount--;
		_rpcsvcstate = _SERVED;
		return;
	}
	(void) memset(&argument, 0, sizeof (argument));
	if (!svc_getargs(transp, xdr_argument, &argument)) {
		svcerr_decode(transp);
		_rpcsvccount--;
		_rpcsvcstate = _SERVED;
		return;
	}
	result = (*local)((char *)&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, &argument)) {
		syslog(LOG_ERR, "unable to free arguments");
		exit(1);
	}
	_rpcsvccount--;
	_rpcsvcstate = _SERVED;
	return;
}
