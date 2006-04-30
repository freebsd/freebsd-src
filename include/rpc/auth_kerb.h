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
 * auth_kerb.h, Protocol for Kerberos style authentication for RPC
 *
 * Copyright (C) 1986, Sun Microsystems, Inc.
 */

#ifndef	_RPC_AUTH_KERB_H
#define	_RPC_AUTH_KERB_H

#ifdef KERBEROS

#include <kerberos/krb.h>
#include <sys/socket.h>
#include <sys/t_kuser.h>
#include <netinet/in.h>
#include <rpc/svc.h>

/*
 * There are two kinds of "names": fullnames and nicknames
 */
enum authkerb_namekind {
	AKN_FULLNAME,
	AKN_NICKNAME
};
/*
 * A fullname contains the ticket and the window
 */
struct authkerb_fullname {
	KTEXT_ST ticket;
	u_long window;		/* associated window */
};

/*
 *  cooked credential stored in rq_clntcred
 */
struct authkerb_clnt_cred {
	/* start of AUTH_DAT */
	unsigned char k_flags;	/* Flags from ticket */
	char    pname[ANAME_SZ]; /* Principal's name */
	char    pinst[INST_SZ];	/* His Instance */
	char    prealm[REALM_SZ]; /* His Realm */
	unsigned long checksum;	/* Data checksum (opt) */
	C_Block session;	/* Session Key */
	int	life;		/* Life of ticket */
	unsigned long time_sec;	/* Time ticket issued */
	unsigned long address;	/* Address in ticket */
	/* KTEXT_ST reply;	Auth reply (opt) */
	/* end of AUTH_DAT */
	unsigned long expiry;	/* time the ticket is expiring */
	u_long nickname;	/* Nickname into cache */
	u_long window;		/* associated window */
};

typedef struct authkerb_clnt_cred authkerb_clnt_cred;

/*
 * A credential
 */
struct authkerb_cred {
	enum authkerb_namekind akc_namekind;
	struct authkerb_fullname akc_fullname;
	u_long akc_nickname;
};

/*
 * A kerb authentication verifier
 */
struct authkerb_verf {
	union {
		struct timeval akv_ctime;	/* clear time */
		des_block akv_xtime;		/* crypt time */
	} akv_time_u;
	u_long akv_int_u;
};

/*
 * des authentication verifier: client variety
 *
 * akv_timestamp is the current time.
 * akv_winverf is the credential window + 1.
 * Both are encrypted using the conversation key.
 */
#ifndef akv_timestamp
#define	akv_timestamp	akv_time_u.akv_ctime
#define	akv_xtimestamp	akv_time_u.akv_xtime
#define	akv_winverf	akv_int_u
#endif
/*
 * des authentication verifier: server variety
 *
 * akv_timeverf is the client's timestamp + client's window
 * akv_nickname is the server's nickname for the client.
 * akv_timeverf is encrypted using the conversation key.
 */
#ifndef akv_timeverf
#define	akv_timeverf	akv_time_u.akv_ctime
#define	akv_xtimeverf	akv_time_u.akv_xtime
#define	akv_nickname	akv_int_u
#endif

/*
 * Register the service name, instance and realm.
 */
extern int	authkerb_create(char *, char *, char *, u_int,
			struct netbuf *, int *, dev_t, int, AUTH **);
extern bool_t	xdr_authkerb_cred(XDR *, struct authkerb_cred *);
extern bool_t	xdr_authkerb_verf(XDR *, struct authkerb_verf *);
extern int	svc_kerb_reg(SVCXPRT *, char *, char *, char *);
extern enum auth_stat _svcauth_kerb(struct svc_req *, struct rpc_msg *);

#endif KERBEROS
#endif	/* !_RPC_AUTH_KERB_H */
