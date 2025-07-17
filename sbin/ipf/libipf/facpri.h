
/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

#ifndef	__FACPRI_H__
#define	__FACPRI_H__

#ifndef	__P
# define P_DEF
#  define	__P(x) x
#endif

extern	char	*fac_toname(int);
extern	int	fac_findname(char *);

extern	char	*pri_toname(int);
extern	int	pri_findname(char *);

#ifdef P_DEF
# undef	__P
# undef	P_DEF
#endif

#if LOG_CRON == (9<<3)
# define	LOG_CRON1	LOG_CRON
# define	LOG_CRON2	(15<<3)
#endif
#if LOG_CRON == (15<<3)
# define	LOG_CRON1	(9<<3)
# define	LOG_CRON2	LOG_CRON
#endif

#endif /* __FACPRI_H__ */
