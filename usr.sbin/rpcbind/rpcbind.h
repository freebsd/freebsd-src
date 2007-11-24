/*	$NetBSD: rpcbind.h,v 1.1 2000/06/03 00:47:21 fvdl Exp $	*/
/*	$FreeBSD$ */

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
/*
 * Copyright (c) 1986 - 1991 by Sun Microsystems, Inc.
 */

/* #ident	"@(#)rpcbind.h 1.4 90/04/12 SMI" */

/*
 * rpcbind.h
 * The common header declarations
 */

#ifndef rpcbind_h
#define	rpcbind_h

#ifdef PORTMAP
#include <rpc/pmap_prot.h>
#endif
#include <rpc/rpcb_prot.h>

/*
 * Stuff for the rmtcall service
 */
struct encap_parms {
	u_int32_t arglen;
	char *args;
};

struct r_rmtcall_args {
	u_int32_t  rmt_prog;
	u_int32_t  rmt_vers;
	u_int32_t  rmt_proc;
	int     rmt_localvers;  /* whether to send port # or uaddr */
	char    *rmt_uaddr;
	struct encap_parms rmt_args;
};

extern int debugging;
extern int doabort;
extern int verboselog;
extern int insecure;
extern int oldstyle_local;
extern rpcblist_ptr list_rbl;	/* A list of version 3 & 4 rpcbind services */

#ifdef PORTMAP
extern struct pmaplist *list_pml; /* A list of version 2 rpcbind services */
extern char *udptrans;		/* Name of UDP transport */
extern char *tcptrans;		/* Name of TCP transport */
extern char *udp_uaddr;		/* Universal UDP address */
extern char *tcp_uaddr;		/* Universal TCP address */
#endif

int add_bndlist __P((struct netconfig *, struct netbuf *));
bool_t is_bound __P((char *, char *));
char *mergeaddr __P((SVCXPRT *, char *, char *, char *));
struct netconfig *rpcbind_get_conf __P((char *));

void rpcbs_init __P((void)); 
void rpcbs_procinfo __P((rpcvers_t, rpcproc_t));
void rpcbs_set __P((rpcvers_t, bool_t));
void rpcbs_unset __P((rpcvers_t, bool_t));
void rpcbs_getaddr __P((rpcvers_t, rpcprog_t, rpcvers_t, char *, char *));
void rpcbs_rmtcall __P((rpcvers_t, rpcproc_t, rpcprog_t, rpcvers_t, rpcproc_t,
			char *, rpcblist_ptr));
void *rpcbproc_getstat __P((void *, struct svc_req *, SVCXPRT *, rpcvers_t));

void rpcb_service_3 __P((struct svc_req *, SVCXPRT *));
void rpcb_service_4 __P((struct svc_req *, SVCXPRT *));

/* Common functions shared between versions */
void *rpcbproc_set_com __P((void *, struct svc_req *, SVCXPRT *, rpcvers_t));
void *rpcbproc_unset_com __P((void *, struct svc_req *, SVCXPRT *, rpcvers_t));
bool_t map_set __P((RPCB *, char *));
bool_t map_unset __P((RPCB *, char *));
void delete_prog __P((unsigned int));
void *rpcbproc_getaddr_com __P((RPCB *, struct svc_req *, SVCXPRT *, rpcvers_t,
				 rpcvers_t));
void *rpcbproc_gettime_com __P((void *, struct svc_req *, SVCXPRT *,
				rpcvers_t));
void *rpcbproc_uaddr2taddr_com __P((void *, struct svc_req *,
					     SVCXPRT *, rpcvers_t));
void *rpcbproc_taddr2uaddr_com __P((void *, struct svc_req *, SVCXPRT *,
				    rpcvers_t));
int create_rmtcall_fd __P((struct netconfig *));
void rpcbproc_callit_com __P((struct svc_req *, SVCXPRT *, rpcvers_t,
			      rpcvers_t));
void my_svc_run __P((void));

void rpcbind_abort __P((void));
void reap __P((int));
void toggle_verboselog __P((int));

int check_access __P((SVCXPRT *, rpcproc_t, void *, unsigned int));
int check_callit __P((SVCXPRT *, struct r_rmtcall_args *, int));
void logit __P((int, struct sockaddr *, rpcproc_t, rpcprog_t, const char *));
int is_loopback __P((struct netbuf *));

#ifdef PORTMAP
extern void pmap_service __P((struct svc_req *, SVCXPRT *));
#endif

void write_warmstart __P((void));
void read_warmstart __P((void));

char *addrmerge __P((struct netbuf *caller, char *serv_uaddr, char *clnt_uaddr,
		     char *netid));
void network_init __P((void));
struct sockaddr *local_sa __P((int));

/* For different getaddr semantics */
#define	RPCB_ALLVERS 0
#define	RPCB_ONEVERS 1

#endif /* rpcbind_h */
