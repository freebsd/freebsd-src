/* 
 * $FreeBSD$
 * $Source: /home/ncvs/src/eBones/libexec/rkinitd/rkinitd.h,v $
 * $Author: gibbs $
 *
 * This header file contains function declarations for use for rkinitd
 */

#ifndef __RKINITD_H__
#define __RKINITD_H__

#if !defined(lint) && !defined(SABER) && !defined(LOCORE) && defined(RCS_HDRS)
static char *rcsid_rkinitd_h = "$FreeBSD$";
#endif /* lint || SABER || LOCORE || RCS_HDRS */

#ifdef __STDC__
#define RK_PROTO(x) x
#else
#define RK_PROTO(x) ()
#endif /* __STDC__ */

int get_tickets RK_PROTO((int));
void error RK_PROTO((void));
int setup_rpc RK_PROTO((int))		;
void rpc_exchange_version_info RK_PROTO((int *, int *, int, int));
void rpc_get_rkinit_info RK_PROTO((rkinit_info *));
void rpc_send_error RK_PROTO((char *));
void rpc_send_success RK_PROTO((void));
void rpc_exchange_tkt RK_PROTO((KTEXT, MSG_DAT *));
void rpc_getauth RK_PROTO((KTEXT, struct sockaddr_in *, struct sockaddr_in *));
int choose_version RK_PROTO((int *));


#endif /* __RKINITD_H__ */
