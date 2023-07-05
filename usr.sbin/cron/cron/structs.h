/*
 * $Id: structs.h,v 1.1 1998/08/14 00:31:24 vixie Exp $
 */

/*
 * Copyright (c) 1997 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

typedef	struct _entry {
	struct _entry	*next;
	uid_t		uid;
	gid_t		gid;
#ifdef LOGIN_CAP
	char            *class;
#endif
	char		**envp;
	char		*cmd;
	union {
		struct {
			bitstr_t	bit_decl(second, SECOND_COUNT);
			bitstr_t	bit_decl(minute, MINUTE_COUNT);
			bitstr_t	bit_decl(hour,   HOUR_COUNT);
			bitstr_t	bit_decl(dom,    DOM_COUNT);
			bitstr_t	bit_decl(month,  MONTH_COUNT);
			bitstr_t	bit_decl(dow,    DOW_COUNT);
		};
		struct {
			time_t	lastexit;
			time_t	interval;
			pid_t	child;
		};
	};
	int		flags;
#define	DOM_STAR	0x01
#define	DOW_STAR	0x02
#define	WHEN_REBOOT	0x04
#define	DONT_LOG	0x08
#define	NOT_UNTIL	0x10
#define	SEC_RES		0x20
#define	INTERVAL	0x40
#define	RUN_AT		0x80
#define	MAIL_WHEN_ERR	0x100
	time_t	lastrun;
} entry;

			/* the crontab database will be a list of the
			 * following structure, one element per user
			 * plus one for the system.
			 *
			 * These are the crontabs.
			 */

typedef	struct _user {
	struct _user	*next, *prev;	/* links */
	char		*name;
	time_t		mtime;		/* last modtime of crontab */
	entry		*crontab;	/* this person's crontab */
} user;

typedef	struct _cron_db {
	user		*head, *tail;	/* links */
	time_t		mtime;		/* last modtime on spooldir */
} cron_db;
				/* in the C tradition, we only create
				 * variables for the main program, just
				 * extern them elsewhere.
				 */

