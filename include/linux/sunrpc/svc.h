/*
 * linux/include/linux/sunrpc/svc.h
 *
 * RPC server declarations.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */


#ifndef SUNRPC_SVC_H
#define SUNRPC_SVC_H

#include <linux/in.h>
#include <linux/sunrpc/types.h>
#include <linux/sunrpc/xdr.h>
#include <linux/sunrpc/svcauth.h>

/*
 * RPC service.
 *
 * An RPC service is a ``daemon,'' possibly multithreaded, which
 * receives and processes incoming RPC messages.
 * It has one or more transport sockets associated with it, and maintains
 * a list of idle threads waiting for input.
 *
 * We currently do not support more than one RPC program per daemon.
 */
struct svc_serv {
	struct list_head	sv_threads;	/* idle server threads */
	struct list_head	sv_sockets;	/* pending sockets */
	struct svc_program *	sv_program;	/* RPC program */
	struct svc_stat *	sv_stats;	/* RPC statistics */
	spinlock_t		sv_lock;
	unsigned int		sv_nrthreads;	/* # of server threads */
	unsigned int		sv_bufsz;	/* datagram buffer size */
	unsigned int		sv_xdrsize;	/* XDR buffer size */

	struct list_head	sv_permsocks;	/* all permanent sockets */
	struct list_head	sv_tempsocks;	/* all temporary sockets */
	int			sv_tmpcnt;	/* count of temporary sockets */

	char *			sv_name;	/* service name */
};

/*
 * Maximum payload size supported by a kernel RPC server.
 * This is use to determine the max number of pages nfsd is
 * willing to return in a single READ operation.
 */
#define RPCSVC_MAXPAYLOAD	16384u

/*
 * Buffer to store RPC requests or replies in.
 * Each server thread has one of these beasts.
 *
 * Area points to the allocated memory chunk currently owned by the
 * buffer. Base points to the buffer containing the request, which is
 * different from area when directly reading from an sk_buff. buf is
 * the current read/write position while processing an RPC request.
 *
 * The array of iovecs can hold additional data that the server process
 * may not want to copy into the RPC reply buffer, but pass to the 
 * network sendmsg routines directly. The prime candidate for this
 * will of course be NFS READ operations, but one might also want to
 * do something about READLINK and READDIR. It might be worthwhile
 * to implement some generic readdir cache in the VFS layer...
 *
 * On the receiving end of the RPC server, the iovec may be used to hold
 * the list of IP fragments once we get to process fragmented UDP
 * datagrams directly.
 */
#define RPCSVC_MAXIOV		((RPCSVC_MAXPAYLOAD+PAGE_SIZE-1)/PAGE_SIZE + 1)
struct svc_buf {
	u32 *			area;	/* allocated memory */
	u32 *			base;	/* base of RPC datagram */
	int			buflen;	/* total length of buffer */
	u32 *			buf;	/* read/write pointer */
	int			len;	/* current end of buffer */

	/* iovec for zero-copy NFS READs */
	struct iovec		iov[RPCSVC_MAXIOV];
	int			nriov;
};
#define svc_getlong(argp, val)	{ (val) = *(argp)->buf++; (argp)->len--; }
#define svc_putlong(resp, val)	{ *(resp)->buf++ = (val); (resp)->len++; }

/*
 * The context of a single thread, including the request currently being
 * processed.
 * NOTE: First two items must be prev/next.
 */
struct svc_rqst {
	struct list_head	rq_list;	/* idle list */
	struct svc_sock *	rq_sock;	/* socket */
	struct sockaddr_in	rq_addr;	/* peer address */
	int			rq_addrlen;

	struct svc_serv *	rq_server;	/* RPC service definition */
	struct svc_procedure *	rq_procinfo;	/* procedure info */
	struct svc_cred		rq_cred;	/* auth info */
	struct sk_buff *	rq_skbuff;	/* fast recv inet buffer */
	struct svc_buf		rq_defbuf;	/* default buffer */
	struct svc_buf		rq_argbuf;	/* argument buffer */
	struct svc_buf		rq_resbuf;	/* result buffer */
	u32			rq_xid;		/* transmission id */
	u32			rq_prog;	/* program number */
	u32			rq_vers;	/* program version */
	u32			rq_proc;	/* procedure number */
	u32			rq_prot;	/* IP protocol */
	unsigned short		rq_verfed  : 1,	/* reply has verifier */
				rq_userset : 1,	/* auth->setuser OK */
				rq_secure  : 1,	/* secure port */
				rq_auth    : 1;	/* check client */

	__u32			rq_daddr;	/* dest addr of request - reply from here */

	void *			rq_argp;	/* decoded arguments */
	void *			rq_resp;	/* xdr'd results */

	int			rq_reserved;	/* space on socket outq
						 * reserved for this request
						 */

	/* Catering to nfsd */
	struct svc_client *	rq_client;	/* RPC peer info */
	struct svc_cacherep *	rq_cacherep;	/* cache info */
	struct knfsd_fh *	rq_reffh;	/* Referrence filehandle, used to
						 * determine what device number
						 * to report (real or virtual)
						 */

	wait_queue_head_t	rq_wait;	/* synchronozation */
};

/*
 * RPC program
 */
struct svc_program {
	u32			pg_prog;	/* program number */
	unsigned int		pg_lovers;	/* lowest version */
	unsigned int		pg_hivers;	/* lowest version */
	unsigned int		pg_nvers;	/* number of versions */
	struct svc_version **	pg_vers;	/* version array */
	char *			pg_name;	/* service name */
	struct svc_stat *	pg_stats;	/* rpc statistics */
};

/*
 * RPC program version
 */
struct svc_version {
	u32			vs_vers;	/* version number */
	u32			vs_nproc;	/* number of procedures */
	struct svc_procedure *	vs_proc;	/* per-procedure info */

	/* Override dispatch function (e.g. when caching replies).
	 * A return value of 0 means drop the request. 
	 * vs_dispatch == NULL means use default dispatcher.
	 */
	int			(*vs_dispatch)(struct svc_rqst *, u32 *);
};

/*
 * RPC procedure info
 */
typedef int	(*svc_procfunc)(struct svc_rqst *, void *argp, void *resp);
struct svc_procedure {
	svc_procfunc		pc_func;	/* process the request */
	kxdrproc_t		pc_decode;	/* XDR decode args */
	kxdrproc_t		pc_encode;	/* XDR encode result */
	kxdrproc_t		pc_release;	/* XDR free result */
	unsigned int		pc_argsize;	/* argument struct size */
	unsigned int		pc_ressize;	/* result struct size */
	unsigned int		pc_count;	/* call count */
	unsigned int		pc_cachetype;	/* cache info (NFS) */
	unsigned int		pc_xdrressize;	/* maximum size of XDR reply */
};

/*
 * This is the RPC server thread function prototype
 */
typedef void		(*svc_thread_fn)(struct svc_rqst *);

/*
 * Function prototypes.
 */
struct svc_serv *  svc_create(struct svc_program *, unsigned int, unsigned int);
int		   svc_create_thread(svc_thread_fn, struct svc_serv *);
void		   svc_exit_thread(struct svc_rqst *);
void		   svc_destroy(struct svc_serv *);
int		   svc_process(struct svc_serv *, struct svc_rqst *);
int		   svc_register(struct svc_serv *, int, unsigned short);
void		   svc_wake_up(struct svc_serv *);
void		   svc_reserve(struct svc_rqst *rqstp, int space);

#endif /* SUNRPC_SVC_H */
