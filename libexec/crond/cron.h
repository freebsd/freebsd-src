/* cron.h - header for vixie's cron
 *
 * $Header: /a/cvs/386BSD/src/libexec/crond/cron.h,v 1.1.1.1 1993/06/12 14:55:04 rgrimes Exp $
 * $Source: /a/cvs/386BSD/src/libexec/crond/cron.h,v $
 * $Revision: 1.1.1.1 $
 * $Log: cron.h,v $
 * Revision 1.1.1.1  1993/06/12  14:55:04  rgrimes
 * Initial import, 0.1 + pk 0.2.4-B1
 *
 * Revision 2.1  90/07/18  00:23:47  vixie
 * Baseline for 4.4BSD release
 * 
 * Revision 2.0  88/12/10  04:57:39  vixie
 * V2 Beta
 * 
 * Revision 1.2  88/11/29  13:05:46  vixie
 * seems to work on Ultrix 3.0 FT1
 * 
 * Revision 1.1  88/11/14  12:27:49  vixie
 * Initial revision
 * 
 * Revision 1.4  87/05/02  17:33:08  paul
 * baseline for mod.sources release
 *
 * vix 14jan87 [0 or 7 can be sunday; thanks, mwm@berkeley]
 * vix 30dec86 [written]
 */

/* Copyright 1988,1990 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie, 329 Noe Street, San Francisco, CA, 94114, (415) 864-7013,
 * paul@vixie.sf.ca.us || {hoptoad,pacbell,decwrl,crash}!vixie!paul
 */

#ifndef	_CRON_FLAG
#define	_CRON_FLAG

#include <stdio.h>
#include <ctype.h>
#include <bitstring.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"

	/* these are really immutable, and are
	 *   defined for symbolic convenience only
	 * TRUE, FALSE, and ERR must be distinct
	 */
#define TRUE		1
#define FALSE		0
	/* system calls return this on success */
#define OK		0
	/*   or this on error */
#define ERR		(-1)

	/* meaningless cookie for smart compilers that will pick their
	 * own register variables; this makes the code neater.
	 */
#define	local		/**/

	/* turn this on to get '-x' code */
#ifndef DEBUGGING
#define DEBUGGING	FALSE
#endif

#define READ_PIPE	0	/* which end of a pipe pair do you read? */
#define WRITE_PIPE	1	/*   or write to? */
#define STDIN		0	/* what is stdin's file descriptor? */
#define STDOUT		1	/*   stdout's? */
#define STDERR		2	/*   stderr's? */
#define ERROR_EXIT	1	/* exit() with this will scare the shell */
#define	OK_EXIT		0	/* exit() with this is considered 'normal' */
#define	MAX_FNAME	100	/* max length of internally generated fn */
#define	MAX_COMMAND	1000	/* max length of internally generated cmd */
#define	MAX_ENVSTR	1000	/* max length of envvar=value\0 strings */
#define	MAX_TEMPSTR	100	/* obvious */
#define	MAX_UNAME	20	/* max length of username, should be overkill */
#define	ROOT_UID	0	/* don't change this, it really must be root */
#define	ROOT_USER	"root"	/* ditto */

				/* NOTE: these correspond to DebugFlagNames,
				 *	defined below.
				 */
#define	DEXT		0x0001	/* extend flag for other debug masks */
#define	DSCH		0x0002	/* scheduling debug mask */
#define	DPROC		0x0004	/* process control debug mask */
#define	DPARS		0x0008	/* parsing debug mask */
#define	DLOAD		0x0010	/* database loading debug mask */
#define	DMISC		0x0020	/* misc debug mask */
#define	DTEST		0x0040	/* test mode: don't execute any commands */

				/* the code does not depend on any of vfork's
				 * side-effects; it just uses it as a quick
				 * fork-and-exec.
				 */
#if defined(BSD)
# define VFORK		vfork
#endif
#if defined(ATT)
# define VFORK		fork
#endif

#define	CRON_TAB(u)	"%s/%s", SPOOL_DIR, u
#define	REG		register
#define	PPC_NULL	((char **)NULL)

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#define	Skip_Blanks(c, f) \
			while (c == '\t' || c == ' ') \
				c = get_char(f);

#define	Skip_Nonblanks(c, f) \
			while (c!='\t' && c!=' ' && c!='\n' && c != EOF) \
				c = get_char(f);

#define	Skip_Line(c, f) \
			do {c = get_char(f);} while (c != '\n' && c != EOF);

#if DEBUGGING
# define Debug(mask, message) \
			if ( (DebugFlags & (mask) ) == (mask) ) \
				printf message;
#else /* !DEBUGGING */
# define Debug(mask, message) \
			;
#endif /* DEBUGGING */

#define	MkLower(ch)	(isupper(ch) ? tolower(ch) : ch)
#define	MkUpper(ch)	(islower(ch) ? toupper(ch) : ch)
#define	Set_LineNum(ln)	{Debug(DPARS|DEXT,("linenum=%d\n",ln)); \
			 LineNumber = ln; \
			}

#define	FIRST_MINUTE	0
#define	LAST_MINUTE	59
#define	MINUTE_COUNT	(LAST_MINUTE - FIRST_MINUTE + 1)

#define	FIRST_HOUR	0
#define	LAST_HOUR	23
#define	HOUR_COUNT	(LAST_HOUR - FIRST_HOUR + 1)

#define	FIRST_DOM	1
#define	LAST_DOM	31
#define	DOM_COUNT	(LAST_DOM - FIRST_DOM + 1)

#define	FIRST_MONTH	1
#define	LAST_MONTH	12
#define	MONTH_COUNT	(LAST_MONTH - FIRST_MONTH + 1)

/* note on DOW: 0 and 7 are both Sunday, for compatibility reasons. */
#define	FIRST_DOW	0
#define	LAST_DOW	7
#define	DOW_COUNT	(LAST_DOW - FIRST_DOW + 1)

			/* each user's crontab will be held as a list of
			 * the following structure.
			 *
			 * These are the cron commands.
			 */

typedef	struct	_entry
	{
		struct _entry	*next;
		char		*cmd;
		bitstr_t	bit_decl(minute, MINUTE_COUNT);
		bitstr_t	bit_decl(hour,   HOUR_COUNT);
		bitstr_t	bit_decl(dom,    DOM_COUNT);
		bitstr_t	bit_decl(month,  MONTH_COUNT);
		bitstr_t	bit_decl(dow,    DOW_COUNT);
		int		flags;
# define	DOM_STAR	0x1
# define	DOW_STAR	0x2
# define	WHEN_REBOOT	0x4
	}
	entry;

			/* the crontab database will be a list of the
			 * following structure, one element per user.
			 *
			 * These are the crontabs.
			 */

typedef	struct	_user
	{
		struct _user	*next, *prev;	/* links */
		int		uid;		/* uid from passwd file */
		int		gid;		/* gid from passwd file */
		char		**envp;		/* environ for commands */
		time_t		mtime;		/* last modtime of crontab */
		entry		*crontab;	/* this person's crontab */
	}
	user;

typedef	struct	_cron_db
	{
		user		*head, *tail;	/* links */
		time_t		mtime;		/* last modtime on spooldir */
	}
	cron_db;

				/* in the C tradition, we only create
				 * variables for the main program, just
				 * extern them elsewhere.
				 */

#ifdef MAIN_PROGRAM

# if !defined(LINT) && !defined(lint)
		static char *copyright[] = {
			"@(#) Copyright (C) 1988, 1989, 1990 by Paul Vixie",
			"@(#) All rights reserved"
		};
# endif

		char *MonthNames[] = {
			"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			"Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
			NULL};
		char *DowNames[] = {
			"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun",
			NULL};
		char *ProgramName;
		int LineNumber;
		time_t TargetTime;

# if DEBUGGING
		int DebugFlags;
		char *DebugFlagNames[] = {	/* sync with #defines */
			"ext", "sch", "proc", "pars", "load", "misc", "test",
			NULL};	/* NULL must be last element */
# endif /* DEBUGGING */

#else /* !MAIN_PROGRAM */

		extern char	*MonthNames[];
		extern char	*DowNames[];
		extern char	*ProgramName;
		extern int	LineNumber;
		extern time_t	TargetTime;
# if DEBUGGING
		extern int	DebugFlags;
		extern char	*DebugFlagNames[];
# endif /* DEBUGGING */
#endif /* MAIN_PROGRAM */


#endif	/* _CRON_FLAG */
