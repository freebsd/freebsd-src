/* move3.c */

/* Author:
 *	Steve Kirkendall
 *	14407 SW Teal Blvd. #C
 *	Beaverton, OR 97005
 *	kirkenda@cs.pdx.edu
 */


/* This file contains movement functions that perform character searches */

#include "config.h"
#include "vi.h"

#ifndef NO_CHARSEARCH
static MARK	(*prevfwdfn)();	/* function to search in same direction */
static MARK	(*prevrevfn)();	/* function to search in opposite direction */
static char	prev_key;	/* sought cvhar from previous [fFtT] */

MARK	m__ch(m, cnt, cmd)
	MARK	m;	/* current position */
	long	cnt;
	int	cmd;	/* command: either ',' or ';' */
{
	MARK	(*tmp)();

	if (!prevfwdfn)
	{
		msg("No previous f, F, t, or T command");
		return MARK_UNSET;
	}

	if (cmd == ',')
	{
		m =  (*prevrevfn)(m, cnt, prev_key);

		/* Oops! we didn't want to change the prev*fn vars! */
		tmp = prevfwdfn;
		prevfwdfn = prevrevfn;
		prevrevfn = tmp;

		return m;
	}
	else
	{
		return (*prevfwdfn)(m, cnt, prev_key);
	}
}

/* move forward within this line to next occurrence of key */
MARK	m_fch(m, cnt, key)
	MARK	m;	/* where to search from */
	long	cnt;
	int	key;	/* what to search for */
{
	REG char	*text;

	DEFAULT(1);

	prevfwdfn = m_fch;
	prevrevfn = m_Fch;
	prev_key = key;

	pfetch(markline(m));
	text = ptext + markidx(m);
	while (cnt-- > 0)
	{
		do
		{
			m++;
			text++;
		} while (*text && *text != key);
	}
	if (!*text)
	{
		return MARK_UNSET;
	}
	return m;
}

/* move backward within this line to previous occurrence of key */
MARK	m_Fch(m, cnt, key)
	MARK	m;	/* where to search from */
	long	cnt;
	int	key;	/* what to search for */
{
	REG char	*text;

	DEFAULT(1);

	prevfwdfn = m_Fch;
	prevrevfn = m_fch;
	prev_key = key;

	pfetch(markline(m));
	text = ptext + markidx(m);
	while (cnt-- > 0)
	{
		do
		{
			m--;
			text--;
		} while (text >= ptext && *text != key);
	}
	if (text < ptext)
	{
		return MARK_UNSET;
	}
	return m;
}

/* move forward within this line almost to next occurrence of key */
MARK	m_tch(m, cnt, key)
	MARK	m;	/* where to search from */
	long	cnt;
	int	key;	/* what to search for */
{
	m = m_fch(m, cnt, key);
	if (m != MARK_UNSET)
	{
		prevfwdfn = m_tch;
		prevrevfn = m_Tch;
		m--;
	}
	return m;
}

/* move backward within this line almost to previous occurrence of key */
MARK	m_Tch(m, cnt, key)
	MARK	m;	/* where to search from */
	long	cnt;
	int	key;	/* what to search for */
{
	m = m_Fch(m, cnt, key);
	if (m == MARK_UNSET)
	{
		prevfwdfn = m_Tch;
		prevrevfn = m_tch;
		m++;
	}

	return m;
}
#endif
