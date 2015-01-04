/*	$NetBSD: rpcbind.h,v 1.1 2010/07/26 15:53:00 pooka Exp $	*/

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
extern const char *udptrans;	/* Name of UDP transport */
extern const char *tcptrans;	/* Name of TCP transport */
extern const char *udp_uaddr;	/* Universal UDP address */
extern const char *tcp_uaddr;	/* Universal TCP address */
#endif

extern const char rpcbind_superuser[];
extern const char rpcbind_unknown[];

int add_bndlist(struct netconfig *, struct netbuf *);
bool_t is_bound(const char *, const char *);
char *mergeaddr(SVCXPRT *, char *, char *, char *);
struct netconfig *rpcbind_get_conf(const char *);

void rpcbs_init(void); 
void rpcbs_procinfo(rpcvers_t, rpcproc_t);
void rpcbs_set(rpcvers_t, bool_t);
void rpcbs_unset(rpcvers_t, bool_t);
void rpcbs_getaddr(rpcvers_t, rpcprog_t, rpcvers_t, const char *, const char *);
void rpcbs_rmtcall(rpcvers_t, rpcproc_t, rpcprog_t, rpcvers_t, rpcproc_t,
			char *, rpcblist_ptr);
void *rpcbproc_getstat(void *, struct svc_req *, SVCXPRT *, rpcvers_t);

void rpcb_service_3(struct svc_req *, SVCXPRT *);
void rpcb_service_4(struct svc_req *, SVCXPRT *);

/* Common functions shared between versions */
void *rpcbproc_set_com(void *, struct svc_req *, SVCXPRT *, rpcvers_t);
void *rpcbproc_unset_com(void *, struct svc_req *, SVCXPRT *, rpcvers_t);
bool_t map_set(RPCB *, char *);
bool_t map_unset(RPCB *, const char *);
void delete_prog(int);
void *rpcbproc_getaddr_com(RPCB *, struct svc_req *, SVCXPRT *, rpcvers_t,
				 rpcvers_t);
void *rpcbproc_gettime_com(void *, struct svc_req *, SVCXPRT *,
				rpcvers_t);
void *rpcbproc_uaddr2taddr_com(void *, struct svc_req *,
					     SVCXPRT *, rpcvers_t);
void *rpcbproc_taddr2uaddr_com(void *, struct svc_req *, SVCXPRT *,
				    rpcvers_t);
int create_rmtcall_fd(struct netconfig *);
void rpcbproc_callit_com(struct svc_req *, SVCXPRT *, rpcvers_t,
			      rpcvers_t);
void my_svc_run(void);

void rpcbind_abort(void);
void reap(int);
void toggle_verboselog(int);

int check_access(SVCXPRT *, rpcproc_t, void *, int);
int check_callit(SVCXPRT *, struct r_rmtcall_args *, int);
void logit(int, struct sockaddr *, rpcproc_t, rpcprog_t, const char *);
int is_loopback(struct netbuf *);

#ifdef PORTMAP
extern void pmap_service(struct svc_req *, SVCXPRT *);
#endif

void write_warmstart(void);
void read_warmstart(void);

char *addrmerge(struct netbuf *caller, char *serv_uaddr, char *clnt_uaddr,
		     char *netid);
void network_init(void);
struct sockaddr *local_sa(int);

/* For different getaddr semantics */
#define	RPCB_ALLVERS 0
#define	RPCB_ONEVERS 1

#endif /* rpcbind_h */
