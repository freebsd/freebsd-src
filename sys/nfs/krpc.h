/*	$NetBSD: krpc.h,v 1.4 1995/12/19 23:07:11 cgd Exp $	*/
/*	$Id: krpc.h,v 1.1.2.1 1997/05/11 17:55:58 tegge Exp $	*/

#include <sys/cdefs.h>

int krpc_call __P((struct sockaddr_in *sin,
	u_int prog, u_int vers, u_int func,
	struct mbuf **data, struct mbuf **from));

int krpc_portmap __P((struct sockaddr_in *sin,
	u_int prog, u_int vers, u_int16_t *portp));

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
