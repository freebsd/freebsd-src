/*	$NetBSD: lockd_lock.h,v 1.2 2000/06/09 14:00:54 fvdl Exp $	*/
/*	$FreeBSD$ */

/* Headers and function declarations for file-locking utilities */

struct nlm4_holder * testlock __P((struct nlm4_lock *, int, int));

enum nlm_stats getlock __P((nlm4_lockargs *, struct svc_req *, int));
enum nlm_stats unlock __P((nlm4_lock *, int));
void notify __P((char *, int));

/* flags for testlock, getlock & unlock */
#define LOCK_ASYNC	0x01 /* async version (getlock only) */
#define LOCK_V4 	0x02 /* v4 version */
#define LOCK_MON 	0x04 /* monitored lock (getlock only) */
#define LOCK_CANCEL 0x08 /* cancel, not unlock request (unlock only) */

/* callbacks from lock_proc.c */
void	transmit_result __P((int, nlm_res *, struct sockaddr *));
void	transmit4_result __P((int, nlm4_res *, struct sockaddr *));
CLIENT  *get_client __P((struct sockaddr *, rpcvers_t));
