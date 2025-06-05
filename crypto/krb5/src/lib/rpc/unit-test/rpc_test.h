#ifndef _RPC_TEST_H_RPCGEN
#define	_RPC_TEST_H_RPCGEN

#include <gssrpc/rpc.h>

#define	RPC_TEST_PROG ((unsigned long)(1000001))
#define	RPC_TEST_VERS_1 ((unsigned long)(1))
#define	RPC_TEST_ECHO ((unsigned long)(1))
extern char ** rpc_test_echo_1_svc(char **, struct svc_req *);
extern char ** rpc_test_echo_1(char **, CLIENT *);
extern void rpc_test_prog_1_svc(struct svc_req *, SVCXPRT *);

#endif /* !_RPC_TEST_H_RPCGEN */
