/*
 * Copyright (c) 1983 Eric P. Allman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)mci.c	8.13 (Berkeley) 4/12/94";
#endif /* not lint */

#include "sendmail.h"

/*
**  Mail Connection Information (MCI) Caching Module.
**
**	There are actually two separate things cached.  The first is
**	the set of all open connections -- these are stored in a
**	(small) list.  The second is stored in the symbol table; it
**	has the overall status for all hosts, whether or not there
**	is a connection open currently.
**
**	There should never be too many connections open (since this
**	could flood the socket table), nor should a connection be
**	allowed to sit idly for too long.
**
**	MaxMciCache is the maximum number of open connections that
**	will be supported.
**
**	MciCacheTimeout is the time (in seconds) that a connection
**	is permitted to survive without activity.
**
**	We actually try any cached connections by sending a NOOP
**	before we use them; if the NOOP fails we close down the
**	connection and reopen it.  Note that this means that a
**	server SMTP that doesn't support NOOP will hose the
**	algorithm -- but that doesn't seem too likely.
*/

MCI	**MciCache;		/* the open connection cache */
/*
**  MCI_CACHE -- enter a connection structure into the open connection cache
**
**	This may cause something else to be flushed.
**
**	Parameters:
**		mci -- the connection to cache.
**
**	Returns:
**		none.
*/

mci_cache(mci)
	register MCI *mci;
{
	register MCI **mcislot;
	extern MCI **mci_scan();

	if (MaxMciCache <= 0)
	{
		/* we don't support caching */
		return;
	}

	/*
	**  Find the best slot.  This may cause expired connections
	**  to be closed.
	*/

	mcislot = mci_scan(mci);

	/* if this is already cached, we are done */
	if (bitset(MCIF_CACHED, mci->mci_flags))
		return;

	/* otherwise we may have to clear the slot */
	if (*mcislot != NULL)
		mci_uncache(mcislot, TRUE);

	if (tTd(42, 5))
		printf("mci_cache: caching %x (%s) in slot %d\n",
			mci, mci->mci_host, mcislot - MciCache);
#ifdef LOG
	if (tTd(91, 100))
		syslog(LOG_DEBUG, "%s: mci_cache: caching %x (%s) in slot %d",
			CurEnv->e_id ? CurEnv->e_id : "NOQUEUE",
			mci, mci->mci_host, mcislot - MciCache);
#endif

	*mcislot = mci;
	mci->mci_flags |= MCIF_CACHED;
}
/*
**  MCI_SCAN -- scan the cache, flush junk, and return best slot
**
**	Parameters:
**		savemci -- never flush this one.  Can be null.
**
**	Returns:
**		The LRU (or empty) slot.
*/

MCI **
mci_scan(savemci)
	MCI *savemci;
{
	time_t now;
	register MCI **bestmci;
	register MCI *mci;
	register int i;

	if (MciCache == NULL)
	{
		/* first call */
		MciCache = (MCI **) xalloc(MaxMciCache * sizeof *MciCache);
		bzero((char *) MciCache, MaxMciCache * sizeof *MciCache);
		return (&MciCache[0]);
	}

	now = curtime();
	bestmci = &MciCache[0];
	for (i = 0; i < MaxMciCache; i++)
	{
		mci = MciCache[i];
		if (mci == NULL || mci->mci_state == MCIS_CLOSED)
		{
			bestmci = &MciCache[i];
			continue;
		}
		if (mci->mci_lastuse + MciCacheTimeout < now && mci != savemci)
		{
			/* connection idle too long -- close it */
			bestmci = &MciCache[i];
			mci_uncache(bestmci, TRUE);
			continue;
		}
		if (*bestmci == NULL)
			continue;
		if (mci->mci_lastuse < (*bestmci)->mci_lastuse)
			bestmci = &MciCache[i];
	}
	return bestmci;
}
/*
**  MCI_UNCACHE -- remove a connection from a slot.
**
**	May close a connection.
**
**	Parameters:
**		mcislot -- the slot to empty.
**		doquit -- if TRUE, send QUIT protocol on this connection.
**			  if FALSE, we are assumed to be in a forked child;
**				all we want to do is close the file(s).
**
**	Returns:
**		none.
*/

mci_uncache(mcislot, doquit)
	register MCI **mcislot;
	bool doquit;
{
	register MCI *mci;
	extern ENVELOPE BlankEnvelope;

	mci = *mcislot;
	if (mci == NULL)
		return;
	*mcislot = NULL;

	if (tTd(42, 5))
		printf("mci_uncache: uncaching %x (%s) from slot %d (%d)\n",
			mci, mci->mci_host, mcislot - MciCache, doquit);
#ifdef LOG
	if (tTd(91, 100))
		syslog(LOG_DEBUG, "%s: mci_uncache: uncaching %x (%s) from slot %d (%d)",
			CurEnv->e_id ? CurEnv->e_id : "NOQUEUE",
			mci, mci->mci_host, mcislot - MciCache, doquit);
#endif

	if (doquit)
	{
		message("Closing connection to %s", mci->mci_host);

		mci->mci_flags &= ~MCIF_CACHED;

		/* only uses the envelope to flush the transcript file */
		if (mci->mci_state != MCIS_CLOSED)
			smtpquit(mci->mci_mailer, mci, &BlankEnvelope);
#ifdef XLA
		xla_host_end(mci->mci_host);
#endif
	}
	else
	{
		if (mci->mci_in != NULL)
			xfclose(mci->mci_in, "mci_uncache", "mci_in");
		if (mci->mci_out != NULL)
			xfclose(mci->mci_out, "mci_uncache", "mci_out");
		mci->mci_in = mci->mci_out = NULL;
		mci->mci_state = MCIS_CLOSED;
		mci->mci_exitstat = EX_OK;
		mci->mci_errno = 0;
		mci->mci_flags = 0;
	}
}
/*
**  MCI_FLUSH -- flush the entire cache
**
**	Parameters:
**		doquit -- if TRUE, send QUIT protocol.
**			  if FALSE, just close the connection.
**		allbut -- but leave this one open.
**
**	Returns:
**		none.
*/

mci_flush(doquit, allbut)
	bool doquit;
	MCI *allbut;
{
	register int i;

	if (MciCache == NULL)
		return;

	for (i = 0; i < MaxMciCache; i++)
		if (allbut != MciCache[i])
			mci_uncache(&MciCache[i], doquit);
}
/*
**  MCI_GET -- get information about a particular host
*/

MCI *
mci_get(host, m)
	char *host;
	MAILER *m;
{
	register MCI *mci;
	register STAB *s;

#ifdef DAEMON
	extern SOCKADDR CurHostAddr;

	/* clear CurHostAddr so we don't get a bogus address with this name */
	bzero(&CurHostAddr, sizeof CurHostAddr);
#endif

	/* clear out any expired connections */
	mci_scan(NULL);

	if (m->m_mno < 0)
		syserr("negative mno %d (%s)", m->m_mno, m->m_name);
	s = stab(host, ST_MCI + m->m_mno, ST_ENTER);
	mci = &s->s_mci;
	mci->mci_host = s->s_name;

	if (tTd(42, 2))
	{
		printf("mci_get(%s %s): mci_state=%d, _flags=%x, _exitstat=%d, _errno=%d\n",
			host, m->m_name, mci->mci_state, mci->mci_flags,
			mci->mci_exitstat, mci->mci_errno);
	}

	if (mci->mci_state == MCIS_OPEN)
	{
		/* poke the connection to see if it's still alive */
		smtpprobe(mci);

		/* reset the stored state in the event of a timeout */
		if (mci->mci_state != MCIS_OPEN)
		{
			mci->mci_errno = 0;
			mci->mci_exitstat = EX_OK;
			mci->mci_state = MCIS_CLOSED;
		}
		else
		{
			/* get peer host address for logging reasons only */
			/* (this should really be in the mci struct) */
			int socksize = sizeof CurHostAddr;

			(void) getpeername(fileno(mci->mci_in),
				(struct sockaddr *) &CurHostAddr, &socksize);
		}
	}
	if (mci->mci_state == MCIS_CLOSED)
	{
		/* copy out any mailer flags needed in connection state */
		if (bitnset(M_7BITS, m->m_flags))
			mci->mci_flags |= MCIF_7BIT;
	}

	return mci;
}
/*
**  MCI_DUMP -- dump the contents of an MCI structure.
**
**	Parameters:
**		mci -- the MCI structure to dump.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

mci_dump(mci, logit)
	register MCI *mci;
	bool logit;
{
	register char *p;
	char *sep;
	char buf[1000];
	extern char *ctime();

	sep = logit ? " " : "\n\t";
	p = buf;
	sprintf(p, "MCI@%x: ", mci);
	p += strlen(p);
	if (mci == NULL)
	{
		sprintf(p, "NULL");
		goto printit;
	}
	sprintf(p, "flags=%o, errno=%d, herrno=%d, exitstat=%d, state=%d, pid=%d,%s",
		mci->mci_flags, mci->mci_errno, mci->mci_herrno,
		mci->mci_exitstat, mci->mci_state, mci->mci_pid, sep);
	p += strlen(p);
	sprintf(p, "maxsize=%ld, phase=%s, mailer=%s,%s",
		mci->mci_maxsize,
		mci->mci_phase == NULL ? "NULL" : mci->mci_phase,
		mci->mci_mailer == NULL ? "NULL" : mci->mci_mailer->m_name,
		sep);
	p += strlen(p);
	sprintf(p, "host=%s, lastuse=%s",
		mci->mci_host == NULL ? "NULL" : mci->mci_host,
		ctime(&mci->mci_lastuse));
printit:
#ifdef LOG
	if (logit)
		syslog(LOG_DEBUG, "%s", buf);
	else
#endif
		printf("%s\n", buf);
}
/*
**  MCI_DUMP_ALL -- print the entire MCI cache
**
**	Parameters:
**		logit -- if set, log the result instead of printing
**			to stdout.
**
**	Returns:
**		none.
*/

mci_dump_all(logit)
	bool logit;
{
	register int i;

	if (MciCache == NULL)
		return;

	for (i = 0; i < MaxMciCache; i++)
		mci_dump(MciCache[i], logit);
}
