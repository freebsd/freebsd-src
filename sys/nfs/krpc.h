/*	$NetBSD: krpc.h,v 1.4 1995/12/19 23:07:11 cgd Exp $	*/
/*	$Id: krpc.h,v 1.4 1997/08/16 19:15:52 wollman Exp $	*/

#include <sys/cdefs.h>

struct mbuf;
struct proc;
struct sockaddr;
struct sockaddr_in;

int krpc_call __P((struct sockaddr_in *_sin,
	u_int prog, u_int vers, u_int func,
	struct mbuf **data, struct sockaddr **from, struct proc *procp));

int krpc_portmap __P((struct sockaddr_in *_sin,
	u_int prog, u_int vers, u_int16_t *portp,struct proc *procp));

struct mbuf *xdr_string_encode __P((char *str, int len));

/*
 * RPC definitions for the portmapper
 */
#define	PMAPPORT		111
#define	PMAPPROG		100000
#define	PMAPVERS		2
#define	PMAPPROC_NULL		0
#define	PMAPPROC_SET		1
#define	PMAPPROC_UNSET		2
#define	PMAPPROC_GETPORT	3
#define	PMAPPROC_DUMP		4
#define	PMAPPROC_CALLIT		5
