/*
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char sccsid[] = "@(#)stats.c	8.22 (Berkeley) 5/19/98";
#endif /* not lint */

# include "sendmail.h"
# include "mailstats.h"

struct statistics	Stat;

bool	GotStats = FALSE;	/* set when we have stats to merge */

#define ONE_K		1000		/* one thousand (twenty-four?) */
#define KBYTES(x)	(((x) + (ONE_K - 1)) / ONE_K)
/*
**  MARKSTATS -- mark statistics
*/

void
markstats(e, to, reject)
	register ENVELOPE *e;
	register ADDRESS *to;
	bool reject;
{
	if (reject == TRUE)
	{
		if (e->e_from.q_mailer != NULL)
		{
			if (bitset(EF_DISCARD, e->e_flags))
				Stat.stat_nd[e->e_from.q_mailer->m_mno]++;
			else
				Stat.stat_nr[e->e_from.q_mailer->m_mno]++;
		}
	}
	else if (to == NULL)
	{
		if (e->e_from.q_mailer != NULL)
		{
			Stat.stat_nf[e->e_from.q_mailer->m_mno]++;
			Stat.stat_bf[e->e_from.q_mailer->m_mno] +=
				KBYTES(e->e_msgsize);
		}
	}
	else
	{
		Stat.stat_nt[to->q_mailer->m_mno]++;
		Stat.stat_bt[to->q_mailer->m_mno] += KBYTES(e->e_msgsize);
	}
	GotStats = TRUE;
}
/*
**  POSTSTATS -- post statistics in the statistics file
**
**	Parameters:
**		sfile -- the name of the statistics file.
**
**	Returns:
**		none.
**
**	Side Effects:
**		merges the Stat structure with the sfile file.
*/

void
poststats(sfile)
	char *sfile;
{
	register int fd;
	int sff = SFF_REGONLY|SFF_OPENASROOT;
	struct statistics stat;
	extern off_t lseek();

	if (sfile == NULL || !GotStats)
		return;

	(void) time(&Stat.stat_itime);
	Stat.stat_size = sizeof Stat;
	Stat.stat_magic = STAT_MAGIC;
	Stat.stat_version = STAT_VERSION;

	if (!bitset(DBS_WRITESTATSTOSYMLINK, DontBlameSendmail))
		sff |= SFF_NOSLINK;
	if (!bitset(DBS_WRITESTATSTOHARDLINK, DontBlameSendmail))
		sff |= SFF_NOHLINK;

	fd = safeopen(sfile, O_RDWR, 0644, sff);
	if (fd < 0)
	{
		if (LogLevel > 12)
			sm_syslog(LOG_INFO, NOQID, "poststats: %s: %s",
				  sfile, errstring(errno));
		errno = 0;
		return;
	}
	if (read(fd, (char *) &stat, sizeof stat) == sizeof stat &&
	    stat.stat_size == sizeof stat &&
	    stat.stat_magic == Stat.stat_magic &&
	    stat.stat_version == Stat.stat_version)
	{
		/* merge current statistics into statfile */
		register int i;

		for (i = 0; i < MAXMAILERS; i++)
		{
			stat.stat_nf[i] += Stat.stat_nf[i];
			stat.stat_bf[i] += Stat.stat_bf[i];
			stat.stat_nt[i] += Stat.stat_nt[i];
			stat.stat_bt[i] += Stat.stat_bt[i];
			stat.stat_nr[i] += Stat.stat_nr[i];
			stat.stat_nd[i] += Stat.stat_nd[i];
		}
	}
	else
		bcopy((char *) &Stat, (char *) &stat, sizeof stat);

	/* write out results */
	(void) lseek(fd, (off_t) 0, 0);
	(void) write(fd, (char *) &stat, sizeof stat);
	(void) close(fd);

	/* clear the structure to avoid future disappointment */
	bzero(&Stat, sizeof stat);
	GotStats = FALSE;
}
