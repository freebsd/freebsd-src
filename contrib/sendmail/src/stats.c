/*
 * Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
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
static char id[] = "@(#)$Id: stats.c,v 8.36.14.2 2000/05/25 23:33:34 gshapiro Exp $";
#endif /* ! lint */

#include <sendmail.h>
#include <sendmail/mailstats.h>


static struct statistics	Stat;

static bool	GotStats = FALSE;	/* set when we have stats to merge */

/* See http://physics.nist.gov/cuu/Units/binary.html */
#define ONE_K		1000		/* one thousand (twenty-four?) */
#define KBYTES(x)	(((x) + (ONE_K - 1)) / ONE_K)
/*
**  MARKSTATS -- mark statistics
**
**	Parameters:
**		e -- the envelope.
**		to -- to address.
**		reject -- whether this is a rejection.
**
**	Returns:
**		none.
**
**	Side Effects:
**		changes static Stat structure
*/

void
markstats(e, to, reject)
	register ENVELOPE *e;
	register ADDRESS *to;
	bool reject;
{
	if (reject)
	{
		if (e->e_from.q_mailer != NULL)
		{
			if (bitset(EF_DISCARD, e->e_flags))
				Stat.stat_nd[e->e_from.q_mailer->m_mno]++;
			else
				Stat.stat_nr[e->e_from.q_mailer->m_mno]++;
		}
		Stat.stat_cr++;
	}
	else if (to == NULL)
	{
		Stat.stat_cf++;
		if (e->e_from.q_mailer != NULL)
		{
			Stat.stat_nf[e->e_from.q_mailer->m_mno]++;
			Stat.stat_bf[e->e_from.q_mailer->m_mno] +=
				KBYTES(e->e_msgsize);
		}
	}
	else
	{
		Stat.stat_ct++;
		Stat.stat_nt[to->q_mailer->m_mno]++;
		Stat.stat_bt[to->q_mailer->m_mno] += KBYTES(e->e_msgsize);
	}


	GotStats = TRUE;
}
/*
**  CLEARSTATS -- clear statistics structure
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		clears the Stat structure.
*/

void
clearstats()
{
	/* clear the structure to avoid future disappointment */
	memset(&Stat, '\0', sizeof Stat);
	GotStats = FALSE;
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
	long sff = SFF_REGONLY|SFF_OPENASROOT;
	struct statistics stats;
	extern off_t lseek();

	if (sfile == NULL || !GotStats)
		return;

	(void) time(&Stat.stat_itime);
	Stat.stat_size = sizeof Stat;
	Stat.stat_magic = STAT_MAGIC;
	Stat.stat_version = STAT_VERSION;

	if (!bitnset(DBS_WRITESTATSTOSYMLINK, DontBlameSendmail))
		sff |= SFF_NOSLINK;
	if (!bitnset(DBS_WRITESTATSTOHARDLINK, DontBlameSendmail))
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
	if (read(fd, (char *) &stats, sizeof stats) == sizeof stats &&
	    stats.stat_size == sizeof stats &&
	    stats.stat_magic == Stat.stat_magic &&
	    stats.stat_version == Stat.stat_version)
	{
		/* merge current statistics into statfile */
		register int i;

		for (i = 0; i < MAXMAILERS; i++)
		{
			stats.stat_nf[i] += Stat.stat_nf[i];
			stats.stat_bf[i] += Stat.stat_bf[i];
			stats.stat_nt[i] += Stat.stat_nt[i];
			stats.stat_bt[i] += Stat.stat_bt[i];
			stats.stat_nr[i] += Stat.stat_nr[i];
			stats.stat_nd[i] += Stat.stat_nd[i];
		}
		stats.stat_cr += Stat.stat_cr;
		stats.stat_ct += Stat.stat_ct;
		stats.stat_cf += Stat.stat_cf;
	}
	else
		memmove((char *) &stats, (char *) &Stat, sizeof stats);

	/* write out results */
	(void) lseek(fd, (off_t) 0, 0);
	(void) write(fd, (char *) &stats, sizeof stats);
	(void) close(fd);

	/* clear the structure to avoid future disappointment */
	clearstats();
}
