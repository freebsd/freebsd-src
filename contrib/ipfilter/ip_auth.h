/*
 * Copyright (C) 1997-2001 by Darren Reed & Guido Van Rooij.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: ip_auth.h,v 2.3.2.4 2001/07/18 14:57:08 darrenr Exp $
 *
 */
#ifndef	__IP_AUTH_H__
#define	__IP_AUTH_H__

#define FR_NUMAUTH      32

typedef struct  frauth {
	int	fra_age;
	int	fra_index;
	u_32_t	fra_pass;
	fr_info_t	fra_info;
#if SOLARIS
	queue_t	*fra_q;
#endif
} frauth_t;

typedef	struct	frauthent  {
	struct	frentry	fae_fr;
	struct	frauthent	*fae_next;
	u_long	fae_age;
} frauthent_t;

typedef struct  fr_authstat {
	U_QUAD_T	fas_hits;
	U_QUAD_T	fas_miss;
	u_long		fas_nospace;
	u_long		fas_added;
	u_long		fas_sendfail;
	u_long		fas_sendok;
	u_long		fas_queok;
	u_long		fas_quefail;
	u_long		fas_expire;
	frauthent_t	*fas_faelist;
} fr_authstat_t;


extern	frentry_t	*ipauth;
extern	struct fr_authstat	fr_authstats;
extern	int	fr_defaultauthage;
extern	int	fr_authsize;
extern	int	fr_authused;
extern	int	fr_auth_lock;
extern	u_32_t	fr_checkauth __P((ip_t *, fr_info_t *));
extern	void	fr_authexpire __P((void));
extern	void	fr_authunload __P((void));
extern	mb_t	*fr_authpkts[];
extern	int	fr_newauth __P((mb_t *, fr_info_t *, ip_t *));
#if defined(__NetBSD__) || defined(__OpenBSD__)
extern	int	fr_auth_ioctl __P((caddr_t, int, u_long, frentry_t *, frentry_t **));
#else
extern	int	fr_auth_ioctl __P((caddr_t, int, int, frentry_t *, frentry_t **));
#endif
#endif	/* __IP_AUTH_H__ */
