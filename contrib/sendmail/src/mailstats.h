/*
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
 * Copyright (c) 1983 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *
 *	@(#)mailstats.h	8.8 (Berkeley) 5/19/1998
 */

#define STAT_VERSION	2
#define STAT_MAGIC	0x1B1DE

/*
**  Statistics structure.
*/

struct statistics
{
	int	stat_magic;		/* magic number */
	int	stat_version;		/* stat file version */
	time_t	stat_itime;		/* file initialization time */
	short	stat_size;		/* size of this structure */
	long	stat_nf[MAXMAILERS];	/* # msgs from each mailer */
	long	stat_bf[MAXMAILERS];	/* kbytes from each mailer */
	long	stat_nt[MAXMAILERS];	/* # msgs to each mailer */
	long	stat_bt[MAXMAILERS];	/* kbytes to each mailer */
	long	stat_nr[MAXMAILERS];	/* # rejects by each mailer */
	long	stat_nd[MAXMAILERS];	/* # discards by each mailer */
};
