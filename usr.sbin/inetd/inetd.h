/*
 * Copyright (c) 1983, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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

#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <stdio.h>

#define BUFSIZE 8192
#define LINESIZ 72

#define NORM_TYPE	0
#define MUX_TYPE	1
#define MUXPLUS_TYPE	2
#define TTCP_TYPE	3
#define FAITH_TYPE	4
#define ISMUX(sep)	(((sep)->se_type == MUX_TYPE) || \
			 ((sep)->se_type == MUXPLUS_TYPE))
#define ISMUXPLUS(sep)	((sep)->se_type == MUXPLUS_TYPE)
#define ISTTCP(sep)	((sep)->se_type == TTCP_TYPE)

struct	servtab {
	char	*se_service;		/* name of service */
	int	se_socktype;		/* type of socket to use */
	int	se_family;		/* address family */
	char	*se_proto;		/* protocol used */
	int	se_maxchild;		/* max number of children */
	int	se_maxcpm;		/* max connects per IP per minute */
	int	se_numchild;		/* current number of children */
	pid_t	*se_pids;		/* array of child pids */
	char	*se_user;		/* user name to run as */
	char    *se_group;              /* group name to run as */
#ifdef  LOGIN_CAP
	char    *se_class;              /* login class name to run with */
#endif
	struct	biltin *se_bi;		/* if built-in, description */
	char	*se_server;		/* server program */
	char	*se_server_name;	/* server program without path */
#define	MAXARGV 20
	char	*se_argv[MAXARGV+1];	/* program arguments */
#ifdef IPSEC
	char	*se_policy;		/* IPsec policy string */
#endif
	int	se_fd;			/* open descriptor */
	union {				/* bound address */
		struct	sockaddr se_un_ctrladdr;
		struct	sockaddr_in se_un_ctrladdr4;
		struct	sockaddr_in6 se_un_ctrladdr6;
	} se_un;
#define se_ctrladdr	se_un.se_un_ctrladdr
#define se_ctrladdr4	se_un.se_un_ctrladdr4
#define se_ctrladdr6	se_un.se_un_ctrladdr6
  	socklen_t	se_ctrladdr_size;
	u_char	se_type;		/* type: normal, mux, or mux+ */
	u_char	se_checked;		/* looked at during merge */
	u_char	se_accept;		/* i.e., wait/nowait mode */
	u_char	se_rpc;			/* ==1 if RPC service */
	int	se_rpc_prog;		/* RPC program number */
	u_int	se_rpc_lowvers;		/* RPC low version */
	u_int	se_rpc_highvers;	/* RPC high version */
	int	se_count;		/* number started since se_time */
	struct	timeval se_time;	/* start of se_count */
	struct	servtab *se_next;
	struct se_flags {
		u_int se_nomapped : 1;
		u_int se_reset : 1;
	} se_flags;
};

#define	se_nomapped		se_flags.se_nomapped
#define	se_reset		se_flags.se_reset

void		chargen_dg __P((int, struct servtab *));
void		chargen_stream __P((int, struct servtab *));
void		close_sep __P((struct servtab *));
void		flag_signal __P((int));
void		flag_config __P((int));
void		config __P((void));
void		daytime_dg __P((int, struct servtab *));
void		daytime_stream __P((int, struct servtab *));
void		discard_dg __P((int, struct servtab *));
void		discard_stream __P((int, struct servtab *));
void		echo_dg __P((int, struct servtab *));
void		echo_stream __P((int, struct servtab *));
void		endconfig __P((void));
struct servtab *enter __P((struct servtab *));
void		freeconfig __P((struct servtab *));
struct servtab *getconfigent __P((void));
void		iderror __P((int, int, int, char *));
void		ident_stream __P((int, struct servtab *));
void		machtime_dg __P((int, struct servtab *));
void		machtime_stream __P((int, struct servtab *));
int		matchservent __P((char *, char *, char *));
char	       *newstr __P((char *));
char	       *nextline __P((FILE *));
void		print_service __P((char *, struct servtab *));
void		addchild __P((struct servtab *, int));
void		flag_reapchild __P((int));
void		reapchild __P((void));
void		enable __P((struct servtab *));
void		disable __P((struct servtab *));
void		flag_retry __P((int));
void		retry __P((void));
int		setconfig __P((void));
void		setup __P((struct servtab *));
#ifdef IPSEC
void		ipsecsetup __P((struct servtab *));
#endif
char	       *sskip __P((char **));
char	       *skip __P((char **));
struct servtab *tcpmux __P((int));
int		cpmip __P((struct servtab *, int));
void		inetd_setproctitle __P((char *, int));
int		check_loop __P((struct sockaddr *, struct servtab *sep));

void		unregisterrpc __P((register struct servtab *sep));

struct biltin {
	char	*bi_service;		/* internally provided service name */
	int	bi_socktype;		/* type of socket supported */
	short	bi_fork;		/* 1 if should fork before call */
	int	bi_maxchild;		/* max number of children, -1=default */
	void	(*bi_fn)();		/* function which performs it */
};
