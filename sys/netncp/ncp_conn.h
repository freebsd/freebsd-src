/*
 * Copyright (c) 1999, Boris Popov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef _NETNCP_NCP_CONN_H_
#define _NETNCP_NCP_CONN_H_

#ifdef INET
#ifndef _NETINET_IN_H_
#include <netinet/in.h>
#endif
#endif

#ifdef IPX
#ifndef _NETIPX_IPX_H_
#include <netipx/ipx.h>
#endif
#endif

#ifndef _SYS_SOCKET_H_
#include <sys/socket.h>
#endif

/* type of transport we use */
/*#define	NCP_ON_IPX	0
#define	NCP_ON_TCP	1*/

/* flags field in conn structure */
#define	NCPFL_SOCONN		0x01	/* socket layer is up */
#define	NCPFL_ATTACHED		0x02	/* ncp layer is up */
#define	NCPFL_LOGGED		0x04	/* logged in to server */
#define NCPFL_INVALID		0x08	/* last request was not completed */
#define	NCPFL_INTR		0x10	/* interrupted call */
#define	NCPFL_RESTORING		0x20	/* trying to reconnect */
#define	NCPFL_PERMANENT		0x40	/* no way to kill conn, when this set */
#define	NCPFL_PRIMARY		0x80	/* have meaning only for owner */
#define	NCPFL_SIGNACTIVE	0x1000	/* packet signing active */
#define	NCPFL_SIGNWANTED	0x2000	/* signing should start */

/* access mode for connection */
#define	NCPM_READ		0400	/* able to fetch conn params */
#define	NCPM_WRITE		0200	/* modify/close */
#define	NCPM_EXECUTE		0100	/* run requests */

#define	NCP_DEFAULT_OWNER	((uid_t)-1)
#define	NCP_DEFAULT_GROUP	((uid_t)-1)


/* args used to create connection */
#define	ncp_conn_loginfo	ncp_conn_args
struct ncp_conn_args {
	int		opt;
#define	NCP_OPT_WDOG		1	/* need watch dog socket */
#define	NCP_OPT_MSG		2	/* need message socket */
#define	NCP_OPT_SIGN		4	/* signatures wanted */
#define NCP_OPT_BIND		8	/* force bindery login */
#define NCP_OPT_PERMANENT	0x10	/* only for refernce, completly ignored */
#define	NCP_OPT_NOUPCASEPASS	0x20	/* leave password as is */
	int		sig_level;	/* wanted signature level */
	char 		server[NCP_BINDERY_NAME_LEN+1];
	char		*user;
	char		*password;
	u_int32_t	objtype;
	union {
		struct sockaddr	addr;
#ifdef IPX
		struct sockaddr_ipx ipxaddr;
#endif
#ifdef INET
		struct sockaddr_in inaddr;
#endif
	} addr;
	int	 	timeout;	/* ncp rq timeout */
	int	 	retry_count;	/* counts to give an error */
	uid_t		owner;		/* proposed owner of connection */
	gid_t		group;		/* proposed group of connection */
	mode_t		access_mode;	/* R/W - can do rq's, X - can see the conn */
};

#define ipxaddr		addr.ipxaddr
#define	inaddr		addr.inaddr
#define	saddr		addr.addr

/* user side structure to issue ncp calls */
struct ncp_buf {
	int	rqsize;			/* request size without ncp header */
	int	rpsize;			/* reply size minus ncp header */
	int	cc;			/* completion code */
	int	cs;			/* connection state */
	char	packet[NCP_MAX_PACKET_SIZE];/* Here we prepare requests and receive replies */
};

/*
 * Connection status, returned via sysctl(vfs.nwfs.connstat)
 */
struct ncp_conn_stat {
	struct ncp_conn_args li;
	int		connRef;
	int 		ref_cnt;
	int 		connid;
	int		buffer_size;
	int		flags;
	int 		sign_active;
	uid_t		owner;
	gid_t		group;
	char		user[NCP_MAXUSERNAMELEN+1];
};

#ifdef _KERNEL

#ifndef LK_SHARED
#include <sys/lock.h>
#endif

struct socket;
struct u_cred;

SLIST_HEAD(ncp_conn_head,ncp_conn);

struct ncp_rq;
struct ncp_conn;

/*
 * External and internal processes can reference connection only by handle.
 * This gives us a freedom in maintance of underlying connections.
 */
struct ncp_handle {
	SLIST_ENTRY(ncp_handle)	nh_next;
	int		nh_id;		/* handle id */
	struct ncp_conn*nh_conn;	/* which conn we are refernce */
	struct proc *	nh_proc;	/* who owns the handle	*/
	int		nh_ref;		/* one process can asquire many handles, but we return the one */
};

/* 
 * Describes any connection to server 
 */
struct ncp_conn {
	SLIST_ENTRY(ncp_conn) nc_next;
	struct ncp_conn_args li;
	struct ucred 	*nc_owner;
	gid_t		nc_group;
	int		flags;
	int		nc_id;
	struct socket 	*ncp_so;
	struct socket 	*wdg_so;
	struct socket	*msg_so;
	struct socket 	*bc_so;
	int 		ref_cnt;		/* how many handles leased */
	SLIST_HEAD(ncp_ref_hd,ncp_ref) ref_list;/* list of handles */
	struct lock	nc_lock;		/* excl locks */
	int		nc_lwant;		/* number of wanted locks */
	struct proc	*procp;			/* pid currently operates */
	struct ucred	*ucred;			/* usr currently operates */
	struct ncp_rq	*nc_rq;			/* current request */
	/* Fields used to process ncp requests */
	int 		connid;			/* assigned by server */
	u_int8_t	seq;
	int		buffer_size;		/* Negotiated bufsize */
	/* Fields used to make packet signatures */
	u_int32_t	sign_root[2];
	u_int32_t	sign_state[4];		/* md4 state */
#ifdef NCPBURST
	/* Fields used for packet bursting */
	u_long		bc_pktseq;		/* raw packet sequence */
	u_short		bc_seq;			/* burst sequence */
	u_long		bc_locid;		/* local connection id */
	u_long		bc_remid;		/* remote connection id */
	u_long		bc_pktsize;		/* negotiated burst packet size */
#endif
};

#define	ncp_conn_signwanted(conn)	((conn)->flags & NCPFL_SIGNWANTED)
#define ncp_conn_valid(conn) 		((conn->flags & NCPFL_INVALID) == 0)
#define ncp_conn_invalidate(conn) 	{conn->flags |= NCPFL_INVALID;}

int  ncp_conn_init(void);
int  ncp_conn_destroy(void);
int  ncp_conn_alloc(struct proc *p,struct ucred *cred, struct ncp_conn **connid);
int  ncp_conn_free(struct ncp_conn *conn);
int  ncp_conn_access(struct ncp_conn *conn,struct ucred *cred,mode_t mode);
int  ncp_conn_lock(struct ncp_conn *conn,struct proc *p,struct ucred *cred,int mode);
void ncp_conn_unlock(struct ncp_conn *conn,struct proc *p);
int  ncp_conn_assert_locked(struct ncp_conn *conn,char *checker,struct proc *p);
/*int  ncp_conn_ref(struct ncp_conn *conn, pid_t pid);
int  ncp_conn_rm_ref(struct ncp_conn *conn, pid_t pid, int force);
void ncp_conn_list_rm_ref(pid_t pid);*/
int  ncp_conn_getbyref(int connRef,struct proc *p,struct ucred *cred, int mode,
	struct ncp_conn **connpp);
int  ncp_conn_getbyli(struct ncp_conn_loginfo *li,struct proc *p,struct ucred *cred, 
	int mode, struct ncp_conn **connpp);
int  ncp_conn_setprimary(struct ncp_conn *conn, int on);
int  ncp_conn_locklist(int flags, struct proc *p);
void ncp_conn_unlocklist(struct proc *p);
int  ncp_conn_gethandle(struct ncp_conn *conn, struct proc *p, struct ncp_handle **handle);
int  ncp_conn_puthandle(struct ncp_handle *handle, struct proc *p, int force);
int  ncp_conn_findhandle(int connHandle, struct proc *p, struct ncp_handle **handle);
int  ncp_conn_getattached(struct ncp_conn_args *li,struct proc *p,struct ucred *cred,int mode, struct ncp_conn **connpp);
int  ncp_conn_putprochandles(struct proc *p);
int  ncp_conn_getinfo(struct ncp_conn *ncp, struct ncp_conn_stat *ncs);

extern struct ncp_conn_head conn_list;
extern int ncp_burst_enabled;

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_NCPDATA);
#endif

#endif /* _KERNEL */
#endif /* _NCP_CONN_H_ */
