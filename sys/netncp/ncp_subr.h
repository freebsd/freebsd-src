/*-
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
 * $FreeBSD: src/sys/netncp/ncp_subr.h,v 1.10 2006/11/06 13:42:06 rwatson Exp $
 */
#ifndef _NETNCP_NCP_SUBR_H_
#define _NETNCP_NCP_SUBR_H_

#define NCP_TIMER_TICK	2*hz	/* 1sec */
#define	NCP_SIGMASK(set) 						\
	(SIGISMEMBER(set, SIGINT) || SIGISMEMBER(set, SIGTERM) ||	\
	 SIGISMEMBER(set, SIGHUP) || SIGISMEMBER(set, SIGKILL) ||	\
	 SIGISMEMBER(set, SIGQUIT))


#define	NCP_PRINT(format, args...) printf("FATAL: %s: "format, __func__ ,## args)
#define nwfs_printf	NCP_PRINT
/* Maybe this should panic, but I dont like that */
#define NCPFATAL	NCP_PRINT
#define NCPERROR	NCP_PRINT

/* socket debugging */
#ifdef NCP_SOCKET_DEBUG
#define NCPSDEBUG(format, args...) printf("%s: "format, __func__ ,## args)
#else
#define NCPSDEBUG(format, args...)
#endif

/* NCP calls debug */
#ifdef NCP_NCP_DEBUG
#define NCPNDEBUG(format, args...) printf("%s: "format, __func__ ,## args)
#else
#define NCPNDEBUG(format, args...)
#endif

/* NCP data dump */
#ifdef NCP_DATA_DEBUG
#define NCPDDEBUG(m) m_dumpm(m)
#else
#define NCPDDEBUG(m)
#endif

/* FS VOPS debug */
#ifdef NWFS_VOPS_DEBUG
#define NCPVODEBUG(format, args...) printf("%s: "format, __func__ ,## args)
#else
#define NCPVODEBUG(format, args...)
#endif

/* FS VNOPS debug */
#ifdef NWFS_VNOPS_DEBUG
#define NCPVNDEBUG(format, args...) printf("%s: "format, __func__ ,## args)
#else
#define NCPVNDEBUG(format, args...)
#endif

#define checkbad(fn) {error=(fn);if(error) goto bad;}

#define	ncp_suser(cred)	priv_check_cred(cred, PRIV_NETNCP, 0)

#define ncp_isowner(conn,cred) ((cred)->cr_uid == (conn)->nc_owner->cr_uid)

struct ncp_conn;

struct nwmount;
struct vnode;
struct nwnode;
struct vattr;
struct uio;
struct ncp_nlstables;

struct ncp_open_info {
	u_int32_t 		origfh;
	ncp_fh			fh;
	u_int8_t 		action;
	struct nw_entry_info	fattr;
};

extern int ncp_debuglevel;

struct proc;
struct ucred;

int  ncp_init(void);
int  ncp_done(void);
int  ncp_chkintr(struct ncp_conn *conn, struct thread *td);
char*ncp_str_dup(char *s);

/* ncp_crypt.c */
void nw_keyhash(const u_char *key, const u_char *buf, int buflen, u_char *target);
void nw_encrypt(const u_char *fra, const u_char *buf, u_char *target);
void ncp_sign(const u_int32_t *state, const char *x, u_int32_t *ostate);

#endif /* _NCP_SUBR_H_ */
