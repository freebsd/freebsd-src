/*
 * Copyright (C) 1997 by Darren Reed & Guido Van Rooij.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * $Id: ip_auth.h,v 2.0.2.10 1997/10/29 12:14:07 darrenr Exp $
 *
 */
#ifndef	__IP_AUTH_H__
#define	__IP_AUTH_H__

#define FR_NUMAUTH      32

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
} fr_authstat_t;

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


extern	frentry_t	*ipauth;
extern	struct fr_authstat	fr_authstats;
extern	int	fr_defaultauthage;
extern	int	fr_authstart;
extern	int	fr_authend;
extern	int	fr_authsize;
extern	int	fr_authused;
extern	int	fr_checkauth __P((ip_t *, fr_info_t *));
extern	void	fr_authexpire __P((void));
extern	void	fr_authunload __P((void));
extern	mb_t	*fr_authpkts[];
#if defined(_KERNEL) && SOLARIS
extern	int	fr_newauth __P((mb_t *, fr_info_t *, ip_t *, qif_t *));
#else
extern	int	fr_newauth __P((mb_t *, fr_info_t *, ip_t *));
#endif
#if defined(__NetBSD__) || defined(__OpenBSD__)
extern	int	fr_auth_ioctl __P((caddr_t, u_long, frentry_t *, frentry_t **));
#else
extern	int	fr_auth_ioctl __P((caddr_t, int, frentry_t *, frentry_t **));
#endif
#endif	/* __IP_AUTH_H__ */
