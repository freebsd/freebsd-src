/*
 * (C)opyright 1993, 1994, 1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * @(#)ip_frag.h	1.5 3/24/96
 * $Id: ip_frag.h,v 2.0.2.4 1997/03/27 13:45:09 darrenr Exp $
 */

#ifndef	__IP_FRAG_H_
#define	__IP_FRAG_H__

#define	IPFT_SIZE	257

typedef	struct	ipfr	{
	struct	ipfr	*ipfr_next, *ipfr_prev;
	struct	in_addr	ipfr_src;
	struct	in_addr	ipfr_dst;
	u_short	ipfr_id;
	u_char	ipfr_p;
	u_char	ipfr_tos;
	u_short	ipfr_off;
	u_short	ipfr_ttl;
	u_char	ipfr_pass;
} ipfr_t;


typedef	struct	ipfrstat {
	u_long	ifs_exists;	/* add & already exists */
	u_long	ifs_nomem;
	u_long	ifs_new;
	u_long	ifs_hits;
	u_long	ifs_expire;
	u_long	ifs_inuse;
	struct	ipfr	**ifs_table;
} ipfrstat_t;

#define	IPFR_CMPSZ	(4 + 4 + 2 + 1 + 1)

extern ipfrstat_t *ipfr_fragstats __P((void));
extern int ipfr_newfrag __P((ip_t *, fr_info_t *, int));
extern int ipfr_knownfrag __P((ip_t *, fr_info_t *));
extern void ipfr_unload __P((void));
#if     (BSD >= 199306) || SOLARIS
extern	void	ipfr_slowtimer __P((void));
#else
extern	int	ipfr_slowtimer __P((void));
#endif
#endif	/* __IP_FIL_H__ */
