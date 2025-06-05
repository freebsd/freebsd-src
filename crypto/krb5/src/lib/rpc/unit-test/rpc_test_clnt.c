#include "rpc_test.h"
#include <string.h>

/* Default timeout can be changed using clnt_control() */
static struct timeval TIMEOUT = { 25, 0 };

char **
rpc_test_echo_1(argp, clnt)
	char **argp;
	CLIENT *clnt;
{
	static char *clnt_res;

	memset(&clnt_res, 0, sizeof (clnt_res));
	if (clnt_call(clnt, RPC_TEST_ECHO,
		(xdrproc_t) xdr_wrapstring, (caddr_t) argp,
		(xdrproc_t) xdr_wrapstring, (caddr_t) &clnt_res,
		TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}
