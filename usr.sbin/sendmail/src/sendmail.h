/*
 * Copyright (c) 1983, 1995 Eric P. Allman
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
 *
 *	@(#)sendmail.h	8.159.1.3 (Berkeley) 9/16/96
 */

/*
**  SENDMAIL.H -- Global definitions for sendmail.
*/

# ifdef _DEFINE
# define EXTERN
# ifndef lint
static char SmailSccsId[] =	"@(#)sendmail.h	8.159.1.3		9/16/96";
# endif
# else /*  _DEFINE */
# define EXTERN extern
# endif /* _DEFINE */

# include <unistd.h>
# include <stddef.h>
# include <stdlib.h>
# include <stdio.h>
# include <ctype.h>
# include <setjmp.h>
# include <string.h>
# include <time.h>
# include <errno.h>
# ifdef EX_OK
#  undef EX_OK			/* for SVr4.2 SMP */
# endif
# include <sysexits.h>

# include "conf.h"
# include "useful.h"

# ifdef LOG
# include <syslog.h>
# endif /* LOG */

# ifdef DAEMON
# include <sys/socket.h>
# endif
# if NETUNIX
# include <sys/un.h>
# endif
# if NETINET
# include <netinet/in.h>
# endif
# if NETISO
# include <netiso/iso.h>
# endif
# if NETNS
# include <netns/ns.h>
# endif
# if NETX25
# include <netccitt/x25.h>
# endif



/* forward references for prototypes */
typedef struct envelope	ENVELOPE;
typedef struct mailer	MAILER;


/*
**  Data structure for bit maps.
**
**	Each bit in this map can be referenced by an ascii character.
**	This is 256 possible bits, or 32 8-bit bytes.
*/

#define BITMAPBYTES	32	/* number of bytes in a bit map */
#define BYTEBITS	8	/* number of bits in a byte */

/* internal macros */
#define _BITWORD(bit)	((bit) / (BYTEBITS * sizeof (int)))
#define _BITBIT(bit)	(1 << ((bit) % (BYTEBITS * sizeof (int))))

typedef int	BITMAP[BITMAPBYTES / sizeof (int)];

/* test bit number N */
#define bitnset(bit, map)	((map)[_BITWORD(bit)] & _BITBIT(bit))

/* set bit number N */
#define setbitn(bit, map)	(map)[_BITWORD(bit)] |= _BITBIT(bit)

/* clear bit number N */
#define clrbitn(bit, map)	(map)[_BITWORD(bit)] &= ~_BITBIT(bit)

/* clear an entire bit map */
#define clrbitmap(map)		bzero((char *) map, BITMAPBYTES)


/*
**  Utility macros
*/

/* return number of bytes left in a buffer */
#define SPACELEFT(buf, ptr)	(sizeof buf - ((ptr) - buf))
/*
**  Address structure.
**	Addresses are stored internally in this structure.
*/

struct address
{
	char		*q_paddr;	/* the printname for the address */
	char		*q_user;	/* user name */
	char		*q_ruser;	/* real user name, or NULL if q_user */
	char		*q_host;	/* host name */
	struct mailer	*q_mailer;	/* mailer to use */
	u_long		q_flags;	/* status flags, see below */
	uid_t		q_uid;		/* user-id of receiver (if known) */
	gid_t		q_gid;		/* group-id of receiver (if known) */
	char		*q_home;	/* home dir (local mailer only) */
	char		*q_fullname;	/* full name if known */
	struct address	*q_next;	/* chain */
	struct address	*q_alias;	/* address this results from */
	char		*q_owner;	/* owner of q_alias */
	struct address	*q_tchain;	/* temporary use chain */
	char		*q_orcpt;	/* ORCPT parameter from RCPT TO: line */
	char		*q_status;	/* status code for DSNs */
	char		*q_rstatus;	/* remote status message for DSNs */
	time_t		q_statdate;	/* date of status messages */
	char		*q_statmta;	/* MTA generating q_rstatus */
	short		q_specificity;	/* how "specific" this address is */
};

typedef struct address ADDRESS;

# define QDONTSEND	0x00000001	/* don't send to this address */
# define QBADADDR	0x00000002	/* this address is verified bad */
# define QGOODUID	0x00000004	/* the q_uid q_gid fields are good */
# define QPRIMARY	0x00000008	/* set from RCPT or argv */
# define QQUEUEUP	0x00000010	/* queue for later transmission */
# define QSENT		0x00000020	/* has been successfully delivered */
# define QNOTREMOTE	0x00000040	/* address not for remote forwarding */
# define QSELFREF	0x00000080	/* this address references itself */
# define QVERIFIED	0x00000100	/* verified, but not expanded */
# define QBOGUSSHELL	0x00000400	/* user has no valid shell listed */
# define QUNSAFEADDR	0x00000800	/* address aquired via unsafe path */
# define QPINGONSUCCESS	0x00001000	/* give return on successful delivery */
# define QPINGONFAILURE	0x00002000	/* give return on failure */
# define QPINGONDELAY	0x00004000	/* give return on message delay */
# define QHASNOTIFY	0x00008000	/* propogate notify parameter */
# define QRELAYED	0x00010000	/* DSN: relayed to non-DSN aware sys */
# define QEXPANDED	0x00020000	/* DSN: undergone list expansion */
# define QDELIVERED	0x00040000	/* DSN: successful final delivery */
# define QDELAYED	0x00080000	/* DSN: message delayed */
# define QTHISPASS	0x80000000	/* temp: address set this pass */

# define NULLADDR	((ADDRESS *) NULL)

/* functions */
extern ADDRESS	*parseaddr __P((char *, ADDRESS *, int, int, char **, ENVELOPE *));
extern ADDRESS	*recipient __P((ADDRESS *, ADDRESS **, int, ENVELOPE *));
extern char	**prescan __P((char *, int, char[], int, char **, u_char *));
extern int	rewrite __P((char **, int, int, ENVELOPE *));
extern char	*remotename __P((char *, MAILER *, int, int *, ENVELOPE *));
extern ADDRESS	*getctladdr __P((ADDRESS *));
extern bool	sameaddr __P((ADDRESS *, ADDRESS *));
extern bool	emptyaddr __P((ADDRESS *));
extern void	printaddr __P((ADDRESS *, bool));
extern void	cataddr __P((char **, char **, char *, int, int));
extern int	sendtolist __P((char *, ADDRESS *, ADDRESS **, int, ENVELOPE *));
/*
**  Mailer definition structure.
**	Every mailer known to the system is declared in this
**	structure.  It defines the pathname of the mailer, some
**	flags associated with it, and the argument vector to
**	pass to it.  The flags are defined in conf.c
**
**	The argument vector is expanded before actual use.  All
**	words except the first are passed through the macro
**	processor.
*/

struct mailer
{
	char	*m_name;	/* symbolic name of this mailer */
	char	*m_mailer;	/* pathname of the mailer to use */
	char	*m_mtatype;	/* type of this MTA */
	char	*m_addrtype;	/* type for addresses */
	char	*m_diagtype;	/* type for diagnostics */
	BITMAP	m_flags;	/* status flags, see below */
	short	m_mno;		/* mailer number internally */
	short	m_nice;		/* niceness to run at (mostly for prog) */
	char	**m_argv;	/* template argument vector */
	short	m_sh_rwset;	/* rewrite set: sender header addresses */
	short	m_se_rwset;	/* rewrite set: sender envelope addresses */
	short	m_rh_rwset;	/* rewrite set: recipient header addresses */
	short	m_re_rwset;	/* rewrite set: recipient envelope addresses */
	char	*m_eol;		/* end of line string */
	long	m_maxsize;	/* size limit on message to this mailer */
	int	m_linelimit;	/* max # characters per line */
	char	*m_execdir;	/* directory to chdir to before execv */
	uid_t	m_uid;		/* UID to run as */
	gid_t	m_gid;		/* GID to run as */
	char	*m_defcharset;	/* default character set */
};

/* bits for m_flags */
# define M_ESMTP	'a'	/* run Extended SMTP protocol */
# define M_ALIASABLE	'A'	/* user can be LHS of an alias */
# define M_BLANKEND	'b'	/* ensure blank line at end of message */
# define M_NOCOMMENT	'c'	/* don't include comment part of address */
# define M_CANONICAL	'C'	/* make addresses canonical "u@dom" */
# define M_NOBRACKET	'd'	/* never angle bracket envelope route-addrs */
		/*	'D'	   CF: include Date: */
# define M_EXPENSIVE	'e'	/* it costs to use this mailer.... */
# define M_ESCFROM	'E'	/* escape From lines to >From */
# define M_FOPT		'f'	/* mailer takes picky -f flag */
		/*	'F'	   CF: include From: or Resent-From: */
# define M_NO_NULL_FROM	'g'	/* sender of errors should be $g */
# define M_HST_UPPER	'h'	/* preserve host case distinction */
# define M_PREHEAD	'H'	/* MAIL11V3: preview headers */
# define M_UDBENVELOPE	'i'	/* do udbsender rewriting on envelope */
# define M_INTERNAL	'I'	/* SMTP to another sendmail site */
# define M_UDBRECIPIENT	'j'	/* do udbsender rewriting on recipient lines */
# define M_NOLOOPCHECK	'k'	/* don't check for loops in HELO command */
# define M_LOCALMAILER	'l'	/* delivery is to this host */
# define M_LIMITS	'L'	/* must enforce SMTP line limits */
# define M_MUSER	'm'	/* can handle multiple users at once */
		/*	'M'	   CF: include Message-Id: */
# define M_NHDR		'n'	/* don't insert From line */
# define M_MANYSTATUS	'N'	/* MAIL11V3: DATA returns multi-status */
# define M_RUNASRCPT	'o'	/* always run mailer as recipient */
# define M_FROMPATH	'p'	/* use reverse-path in MAIL FROM: */
		/*	'P'	   CF: include Return-Path: */
# define M_ROPT		'r'	/* mailer takes picky -r flag */
# define M_SECURE_PORT	'R'	/* try to send on a reserved TCP port */
# define M_STRIPQ	's'	/* strip quote chars from user/host */
# define M_SPECIFIC_UID	'S'	/* run as specific uid/gid */
# define M_USR_UPPER	'u'	/* preserve user case distinction */
# define M_UGLYUUCP	'U'	/* this wants an ugly UUCP from line */
# define M_CONTENT_LEN	'v'	/* add Content-Length: header (SVr4) */
		/*	'V'	   UIUC: !-relativize all addresses */
# define M_HASPWENT	'w'	/* check for /etc/passwd entry */
		/*	'x'	   CF: include Full-Name: */
# define M_XDOT		'X'	/* use hidden-dot algorithm */
# define M_EBCDIC	'3'	/* extend Q-P encoding for EBCDIC */
# define M_TRYRULESET5	'5'	/* use ruleset 5 after local aliasing */
# define M_7BITS	'7'	/* use 7-bit path */
# define M_8BITS	'8'	/* force "just send 8" behaviour */
# define M_MAKE8BIT	'9'	/* convert 7 -> 8 bit if appropriate */
# define M_CHECKINCLUDE	':'	/* check for :include: files */
# define M_CHECKPROG	'|'	/* check for |program addresses */
# define M_CHECKFILE	'/'	/* check for /file addresses */
# define M_CHECKUDB	'@'	/* user can be user database key */

EXTERN MAILER	*Mailer[MAXMAILERS+1];

EXTERN MAILER	*LocalMailer;		/* ptr to local mailer */
EXTERN MAILER	*ProgMailer;		/* ptr to program mailer */
EXTERN MAILER	*FileMailer;		/* ptr to *file* mailer */
EXTERN MAILER	*InclMailer;		/* ptr to *include* mailer */
/*
**  Header structure.
**	This structure is used internally to store header items.
*/

struct header
{
	char		*h_field;	/* the name of the field */
	char		*h_value;	/* the value of that field */
	struct header	*h_link;	/* the next header */
	u_short		h_flags;	/* status bits, see below */
	BITMAP		h_mflags;	/* m_flags bits needed */
};

typedef struct header	HDR;

/*
**  Header information structure.
**	Defined in conf.c, this struct declares the header fields
**	that have some magic meaning.
*/

struct hdrinfo
{
	char	*hi_field;	/* the name of the field */
	u_short	hi_flags;	/* status bits, see below */
};

extern struct hdrinfo	HdrInfo[];

/* bits for h_flags and hi_flags */
# define H_EOH		0x0001	/* this field terminates header */
# define H_RCPT		0x0002	/* contains recipient addresses */
# define H_DEFAULT	0x0004	/* if another value is found, drop this */
# define H_RESENT	0x0008	/* this address is a "Resent-..." address */
# define H_CHECK	0x0010	/* check h_mflags against m_flags */
# define H_ACHECK	0x0020	/* ditto, but always (not just default) */
# define H_FORCE	0x0040	/* force this field, even if default */
# define H_TRACE	0x0080	/* this field contains trace information */
# define H_FROM		0x0100	/* this is a from-type field */
# define H_VALID	0x0200	/* this field has a validated value */
# define H_RECEIPTTO	0x0400	/* this field has return receipt info */
# define H_ERRORSTO	0x0800	/* this field has error address info */
# define H_CTE		0x1000	/* this field is a content-transfer-encoding */
# define H_CTYPE	0x2000	/* this is a content-type field */
# define H_BCC		0x4000	/* Bcc: header: strip value or delete */
/*
**  Information about currently open connections to mailers, or to
**  hosts that we have looked up recently.
*/

# define MCI		struct mailer_con_info

MCI
{
	short		mci_flags;	/* flag bits, see below */
	short		mci_errno;	/* error number on last connection */
	short		mci_herrno;	/* h_errno from last DNS lookup */
	short		mci_exitstat;	/* exit status from last connection */
	short		mci_state;	/* SMTP state */
	long		mci_maxsize;	/* max size this server will accept */
	FILE		*mci_in;	/* input side of connection */
	FILE		*mci_out;	/* output side of connection */
	int		mci_pid;	/* process id of subordinate proc */
	char		*mci_phase;	/* SMTP phase string */
	struct mailer	*mci_mailer;	/* ptr to the mailer for this conn */
	char		*mci_host;	/* host name */
	char		*mci_status;	/* DSN status to be copied to addrs */
	time_t		mci_lastuse;	/* last usage time */
};


/* flag bits */
#define MCIF_VALID	0x0001		/* this entry is valid */
#define MCIF_TEMP	0x0002		/* don't cache this connection */
#define MCIF_CACHED	0x0004		/* currently in open cache */
#define MCIF_ESMTP	0x0008		/* this host speaks ESMTP */
#define MCIF_EXPN	0x0010		/* EXPN command supported */
#define MCIF_SIZE	0x0020		/* SIZE option supported */
#define MCIF_8BITMIME	0x0040		/* BODY=8BITMIME supported */
#define MCIF_7BIT	0x0080		/* strip this message to 7 bits */
#define MCIF_MULTSTAT	0x0100		/* MAIL11V3: handles MULT status */
#define MCIF_INHEADER	0x0200		/* currently outputing header */
#define MCIF_CVT8TO7	0x0400		/* convert from 8 to 7 bits */
#define MCIF_DSN	0x0800		/* DSN extension supported */
#define MCIF_8BITOK	0x1000		/* OK to send 8 bit characters */
#define MCIF_CVT7TO8	0x2000		/* convert from 7 to 8 bits */
#define MCIF_INMIME	0x4000		/* currently reading MIME header */

/* states */
#define MCIS_CLOSED	0		/* no traffic on this connection */
#define MCIS_OPENING	1		/* sending initial protocol */
#define MCIS_OPEN	2		/* open, initial protocol sent */
#define MCIS_ACTIVE	3		/* message being sent */
#define MCIS_QUITING	4		/* running quit protocol */
#define MCIS_SSD	5		/* service shutting down */
#define MCIS_ERROR	6		/* I/O error on connection */

/* functions */
extern MCI	*mci_get __P((char *, MAILER *));
extern void	mci_cache __P((MCI *));
extern void	mci_flush __P((bool, MCI *));
/*
**  Envelope structure.
**	This structure defines the message itself.  There is usually
**	only one of these -- for the message that we originally read
**	and which is our primary interest -- but other envelopes can
**	be generated during processing.  For example, error messages
**	will have their own envelope.
*/

struct envelope
{
	HDR		*e_header;	/* head of header list */
	long		e_msgpriority;	/* adjusted priority of this message */
	time_t		e_ctime;	/* time message appeared in the queue */
	char		*e_to;		/* the target person */
	char		*e_receiptto;	/* return receipt address */
	ADDRESS		e_from;		/* the person it is from */
	char		*e_sender;	/* e_from.q_paddr w comments stripped */
	char		**e_fromdomain;	/* the domain part of the sender */
	ADDRESS		*e_sendqueue;	/* list of message recipients */
	ADDRESS		*e_errorqueue;	/* the queue for error responses */
	long		e_msgsize;	/* size of the message in bytes */
	long		e_flags;	/* flags, see below */
	int		e_nrcpts;	/* number of recipients */
	short		e_class;	/* msg class (priority, junk, etc.) */
	short		e_hopcount;	/* number of times processed */
	short		e_nsent;	/* number of sends since checkpoint */
	short		e_sendmode;	/* message send mode */
	short		e_errormode;	/* error return mode */
	short		e_timeoutclass;	/* message timeout class */
	void		(*e_puthdr)__P((MCI *, HDR *, ENVELOPE *));
					/* function to put header of message */
	void		(*e_putbody)__P((MCI *, ENVELOPE *, char *));
					/* function to put body of message */
	struct envelope	*e_parent;	/* the message this one encloses */
	struct envelope *e_sibling;	/* the next envelope of interest */
	char		*e_bodytype;	/* type of message body */
	FILE		*e_dfp;		/* temporary file */
	char		*e_id;		/* code for this entry in queue */
	FILE		*e_xfp;		/* transcript file */
	FILE		*e_lockfp;	/* the lock file for this message */
	char		*e_message;	/* error message */
	char		*e_statmsg;	/* stat msg (changes per delivery) */
	char		*e_msgboundary;	/* MIME-style message part boundary */
	char		*e_origrcpt;	/* original recipient (one only) */
	char		*e_envid;	/* envelope id from MAIL FROM: line */
	char		*e_status;	/* DSN status for this message */
	time_t		e_dtime;	/* time of last delivery attempt */
	int		e_ntries;	/* number of delivery attempts */
	dev_t		e_dfdev;	/* df file's device, for crash recov */
	ino_t		e_dfino;	/* df file's ino, for crash recovery */
	char		*e_macro[256];	/* macro definitions */
};

/* values for e_flags */
#define EF_OLDSTYLE	0x0000001	/* use spaces (not commas) in hdrs */
#define EF_INQUEUE	0x0000002	/* this message is fully queued */
#define EF_NO_BODY_RETN	0x0000004	/* omit message body on error */
#define EF_CLRQUEUE	0x0000008	/* disk copy is no longer needed */
#define EF_SENDRECEIPT	0x0000010	/* send a return receipt */
#define EF_FATALERRS	0x0000020	/* fatal errors occured */
#define EF_DELETE_BCC	0x0000040	/* delete Bcc: headers entirely */
#define EF_RESPONSE	0x0000080	/* this is an error or return receipt */
#define EF_RESENT	0x0000100	/* this message is being forwarded */
#define EF_VRFYONLY	0x0000200	/* verify only (don't expand aliases) */
#define EF_WARNING	0x0000400	/* warning message has been sent */
#define EF_QUEUERUN	0x0000800	/* this envelope is from queue */
#define EF_GLOBALERRS	0x0001000	/* treat errors as global */
#define EF_PM_NOTIFY	0x0002000	/* send return mail to postmaster */
#define EF_METOO	0x0004000	/* send to me too */
#define EF_LOGSENDER	0x0008000	/* need to log the sender */
#define EF_NORECEIPT	0x0010000	/* suppress all return-receipts */
#define EF_HAS8BIT	0x0020000	/* at least one 8-bit char in body */
#define EF_NL_NOT_EOL	0x0040000	/* don't accept raw NL as EOLine */
#define EF_CRLF_NOT_EOL	0x0080000	/* don't accept CR-LF as EOLine */
#define EF_RET_PARAM	0x0100000	/* RCPT command had RET argument */
#define EF_HAS_DF	0x0200000	/* set when df file is instantiated */
#define EF_IS_MIME	0x0400000	/* really is a MIME message */
#define EF_DONT_MIME	0x0800000	/* never MIME this message */

EXTERN ENVELOPE	*CurEnv;	/* envelope currently being processed */

/* functions */
extern ENVELOPE	*newenvelope __P((ENVELOPE *, ENVELOPE *));
extern void	dropenvelope __P((ENVELOPE *));
extern void	clearenvelope __P((ENVELOPE *, bool));

extern void	putheader __P((MCI *, HDR *, ENVELOPE *));
extern void	putbody __P((MCI *, ENVELOPE *, char *));
/*
**  Message priority classes.
**
**	The message class is read directly from the Priority: header
**	field in the message.
**
**	CurEnv->e_msgpriority is the number of bytes in the message plus
**	the creation time (so that jobs ``tend'' to be ordered correctly),
**	adjusted by the message class, the number of recipients, and the
**	amount of time the message has been sitting around.  This number
**	is used to order the queue.  Higher values mean LOWER priority.
**
**	Each priority class point is worth WkClassFact priority points;
**	each recipient is worth WkRecipFact priority points.  Each time
**	we reprocess a message the priority is adjusted by WkTimeFact.
**	WkTimeFact should normally decrease the priority so that jobs
**	that have historically failed will be run later; thanks go to
**	Jay Lepreau at Utah for pointing out the error in my thinking.
**
**	The "class" is this number, unadjusted by the age or size of
**	this message.  Classes with negative representations will have
**	error messages thrown away if they are not local.
*/

struct priority
{
	char	*pri_name;	/* external name of priority */
	int	pri_val;	/* internal value for same */
};

EXTERN struct priority	Priorities[MAXPRIORITIES];
EXTERN int		NumPriorities;	/* pointer into Priorities */
/*
**  Rewrite rules.
*/

struct rewrite
{
	char	**r_lhs;	/* pattern match */
	char	**r_rhs;	/* substitution value */
	struct rewrite	*r_next;/* next in chain */
};

EXTERN struct rewrite	*RewriteRules[MAXRWSETS];

/*
**  Special characters in rewriting rules.
**	These are used internally only.
**	The COND* rules are actually used in macros rather than in
**		rewriting rules, but are given here because they
**		cannot conflict.
*/

/* left hand side items */
# define MATCHZANY	((u_char)0220)	/* match zero or more tokens */
# define MATCHANY	((u_char)0221)	/* match one or more tokens */
# define MATCHONE	((u_char)0222)	/* match exactly one token */
# define MATCHCLASS	((u_char)0223)	/* match one token in a class */
# define MATCHNCLASS	((u_char)0224)	/* match anything not in class */
# define MATCHREPL	((u_char)0225)	/* replacement on RHS for above */

/* right hand side items */
# define CANONNET	((u_char)0226)	/* canonical net, next token */
# define CANONHOST	((u_char)0227)	/* canonical host, next token */
# define CANONUSER	((u_char)0230)	/* canonical user, next N tokens */
# define CALLSUBR	((u_char)0231)	/* call another rewriting set */

/* conditionals in macros */
# define CONDIF		((u_char)0232)	/* conditional if-then */
# define CONDELSE	((u_char)0233)	/* conditional else */
# define CONDFI		((u_char)0234)	/* conditional fi */

/* bracket characters for host name lookup */
# define HOSTBEGIN	((u_char)0235)	/* hostname lookup begin */
# define HOSTEND	((u_char)0236)	/* hostname lookup end */

/* bracket characters for generalized lookup */
# define LOOKUPBEGIN	((u_char)0205)	/* generalized lookup begin */
# define LOOKUPEND	((u_char)0206)	/* generalized lookup end */

/* macro substitution character */
# define MACROEXPAND	((u_char)0201)	/* macro expansion */
# define MACRODEXPAND	((u_char)0202)	/* deferred macro expansion */

/* to make the code clearer */
# define MATCHZERO	CANONHOST

/* external <==> internal mapping table */
struct metamac
{
	char	metaname;	/* external code (after $) */
	u_char	metaval;	/* internal code (as above) */
};

/* values for macros with external names only */
# define MID_OPMODE	0202	/* operation mode */

/* functions */
extern void	expand __P((char *, char *, size_t, ENVELOPE *));
extern void	define __P((int, char *, ENVELOPE *));
extern char	*macvalue __P((int, ENVELOPE *));
extern char	*macname __P((int));
extern int	macid __P((char *, char **));
/*
**  Name canonification short circuit.
**
**	If the name server for a host is down, the process of trying to
**	canonify the name can hang.  This is similar to (but alas, not
**	identical to) looking up the name for delivery.  This stab type
**	caches the result of the name server lookup so we don't hang
**	multiple times.
*/

#define NAMECANON	struct _namecanon

NAMECANON
{
	short		nc_errno;	/* cached errno */
	short		nc_herrno;	/* cached h_errno */
	short		nc_stat;	/* cached exit status code */
	short		nc_flags;	/* flag bits */
	char		*nc_cname;	/* the canonical name */
};

/* values for nc_flags */
#define NCF_VALID	0x0001	/* entry valid */
/*
**  Mapping functions
**
**	These allow arbitrary mappings in the config file.  The idea
**	(albeit not the implementation) comes from IDA sendmail.
*/

# define MAPCLASS	struct _mapclass
# define MAP		struct _map
# define MAXMAPACTIONS	3		/* size of map_actions array */


/*
**  An actual map.
*/

MAP
{
	MAPCLASS	*map_class;	/* the class of this map */
	char		*map_mname;	/* name of this map */
	long		map_mflags;	/* flags, see below */
	char		*map_file;	/* the (nominal) filename */
	ARBPTR_T	map_db1;	/* the open database ptr */
	ARBPTR_T	map_db2;	/* an "extra" database pointer */
	char		*map_keycolnm;	/* key column name */
	char		*map_valcolnm;	/* value column name */
	u_char		map_keycolno;	/* key column number */
	u_char		map_valcolno;	/* value column number */
	char		map_coldelim;	/* column delimiter */
	char		*map_app;	/* to append to successful matches */
	char		*map_domain;	/* the (nominal) NIS domain */
	char		*map_rebuild;	/* program to run to do auto-rebuild */
	time_t		map_mtime;	/* last database modification time */
	short		map_specificity;	/* specificity of alaases */
	MAP		*map_stack[MAXMAPSTACK];   /* list for stacked maps */
	short		map_return[MAXMAPACTIONS]; /* return bitmaps for stacked maps */
};

/* bit values for map_mflags */
# define MF_VALID	0x00000001	/* this entry is valid */
# define MF_INCLNULL	0x00000002	/* include null byte in key */
# define MF_OPTIONAL	0x00000004	/* don't complain if map not found */
# define MF_NOFOLDCASE	0x00000008	/* don't fold case in keys */
# define MF_MATCHONLY	0x00000010	/* don't use the map value */
# define MF_OPEN	0x00000020	/* this entry is open */
# define MF_WRITABLE	0x00000040	/* open for writing */
# define MF_ALIAS	0x00000080	/* this is an alias file */
# define MF_TRY0NULL	0x00000100	/* try with no null byte */
# define MF_TRY1NULL	0x00000200	/* try with the null byte */
# define MF_LOCKED	0x00000400	/* this map is currently locked */
# define MF_ALIASWAIT	0x00000800	/* alias map in aliaswait state */
# define MF_IMPL_HASH	0x00001000	/* implicit: underlying hash database */
# define MF_IMPL_NDBM	0x00002000	/* implicit: underlying NDBM database */
# define MF_UNSAFEDB	0x00004000	/* this map is world writable */
# define MF_APPEND	0x00008000	/* append new entry on rebuiled */
# define MF_KEEPQUOTES	0x00010000	/* don't dequote key before lookup */

/* indices for map_actions */
# define MA_NOTFOUND	0		/* member map returned "not found" */
# define MA_UNAVAIL	1		/* member map is not available */
# define MA_TRYAGAIN	2		/* member map returns temp failure */

/*
**  The class of a map -- essentially the functions to call
*/

MAPCLASS
{
	char	*map_cname;		/* name of this map class */
	char	*map_ext;		/* extension for database file */
	short	map_cflags;		/* flag bits, see below */
	bool	(*map_parse)__P((MAP *, char *));
					/* argument parsing function */
	char	*(*map_lookup)__P((MAP *, char *, char **, int *));
					/* lookup function */
	void	(*map_store)__P((MAP *, char *, char *));
					/* store function */
	bool	(*map_open)__P((MAP *, int));
					/* open function */
	void	(*map_close)__P((MAP *));
					/* close function */
};

/* bit values for map_cflags */
#define MCF_ALIASOK	0x0001		/* can be used for aliases */
#define MCF_ALIASONLY	0x0002		/* usable only for aliases */
#define MCF_REBUILDABLE	0x0004		/* can rebuild alias files */
#define MCF_OPTFILE	0x0008		/* file name is optional */

/* functions */
extern char	*map_rewrite __P((MAP *, char *, int, char **));
extern MAP	*makemapentry __P((char *));
/*
**  Symbol table definitions
*/

struct symtab
{
	char		*s_name;	/* name to be entered */
	char		s_type;		/* general type (see below) */
	struct symtab	*s_next;	/* pointer to next in chain */
	union
	{
		BITMAP		sv_class;	/* bit-map of word classes */
		ADDRESS		*sv_addr;	/* pointer to address header */
		MAILER		*sv_mailer;	/* pointer to mailer */
		char		*sv_alias;	/* alias */
		MAPCLASS	sv_mapclass;	/* mapping function class */
		MAP		sv_map;		/* mapping function */
		char		*sv_hostsig;	/* host signature */
		MCI		sv_mci;		/* mailer connection info */
		NAMECANON	sv_namecanon;	/* canonical name cache */
		int		sv_macro;	/* macro name => id mapping */
		int		sv_ruleset;	/* ruleset index */
	}	s_value;
};

typedef struct symtab	STAB;

/* symbol types */
# define ST_UNDEF	0	/* undefined type */
# define ST_CLASS	1	/* class map */
# define ST_ADDRESS	2	/* an address in parsed format */
# define ST_MAILER	3	/* a mailer header */
# define ST_ALIAS	4	/* an alias */
# define ST_MAPCLASS	5	/* mapping function class */
# define ST_MAP		6	/* mapping function */
# define ST_HOSTSIG	7	/* host signature */
# define ST_NAMECANON	8	/* cached canonical name */
# define ST_MACRO	9	/* macro name to id mapping */
# define ST_RULESET	10	/* ruleset index */
# define ST_MCI		16	/* mailer connection info (offset) */

# define s_class	s_value.sv_class
# define s_address	s_value.sv_addr
# define s_mailer	s_value.sv_mailer
# define s_alias	s_value.sv_alias
# define s_mci		s_value.sv_mci
# define s_mapclass	s_value.sv_mapclass
# define s_hostsig	s_value.sv_hostsig
# define s_map		s_value.sv_map
# define s_namecanon	s_value.sv_namecanon
# define s_macro	s_value.sv_macro
# define s_ruleset	s_value.sv_ruleset

extern STAB		*stab __P((char *, int, int));
extern void		stabapply __P((void (*)(STAB *, int), int));

/* opcodes to stab */
# define ST_FIND	0	/* find entry */
# define ST_ENTER	1	/* enter if not there */
/*
**  STRUCT EVENT -- event queue.
**
**	Maintained in sorted order.
**
**	We store the pid of the process that set this event to insure
**	that when we fork we will not take events intended for the parent.
*/

struct event
{
	time_t		ev_time;	/* time of the function call */
	void		(*ev_func)__P((int));
					/* function to call */
	int		ev_arg;		/* argument to ev_func */
	int		ev_pid;		/* pid that set this event */
	struct event	*ev_link;	/* link to next item */
};

typedef struct event	EVENT;

EXTERN EVENT	*EventQueue;		/* head of event queue */

/* functions */
extern EVENT	*setevent __P((time_t, void(*)(), int));
extern void	clrevent __P((EVENT *));
/*
**  Operation, send, error, and MIME modes
**
**	The operation mode describes the basic operation of sendmail.
**	This can be set from the command line, and is "send mail" by
**	default.
**
**	The send mode tells how to send mail.  It can be set in the
**	configuration file.  It's setting determines how quickly the
**	mail will be delivered versus the load on your system.  If the
**	-v (verbose) flag is given, it will be forced to SM_DELIVER
**	mode.
**
**	The error mode tells how to return errors.
*/

EXTERN char	OpMode;		/* operation mode, see below */

#define MD_DELIVER	'm'		/* be a mail sender */
#define MD_SMTP		's'		/* run SMTP on standard input */
#define MD_ARPAFTP	'a'		/* obsolete ARPANET mode (Grey Book) */
#define MD_DAEMON	'd'		/* run as a daemon */
#define MD_VERIFY	'v'		/* verify: don't collect or deliver */
#define MD_TEST		't'		/* test mode: resolve addrs only */
#define MD_INITALIAS	'i'		/* initialize alias database */
#define MD_PRINT	'p'		/* print the queue */
#define MD_FREEZE	'z'		/* freeze the configuration file */


/* values for e_sendmode -- send modes */
#define SM_DELIVER	'i'		/* interactive delivery */
#define SM_FORK		'b'		/* deliver in background */
#define SM_QUEUE	'q'		/* queue, don't deliver */
#define SM_DEFER	'd'		/* defer map lookups as well as queue */
#define SM_VERIFY	'v'		/* verify only (used internally) */

/* used only as a parameter to sendall */
#define SM_DEFAULT	'\0'		/* unspecified, use SendMode */


/* values for e_errormode -- error handling modes */
#define EM_PRINT	'p'		/* print errors */
#define EM_MAIL		'm'		/* mail back errors */
#define EM_WRITE	'w'		/* write back errors */
#define EM_BERKNET	'e'		/* special berknet processing */
#define EM_QUIET	'q'		/* don't print messages (stat only) */


/* MIME processing mode */
EXTERN int	MimeMode;

/* bit values for MimeMode */
#define MM_CVTMIME	0x0001		/* convert 8 to 7 bit MIME */
#define MM_PASS8BIT	0x0002		/* just send 8 bit data blind */
#define MM_MIME8BIT	0x0004		/* convert 8-bit data to MIME */

/* queue sorting order algorithm */
EXTERN int	QueueSortOrder;

#define QS_BYPRIORITY	0		/* sort by message priority */
#define QS_BYHOST	1		/* sort by first host name */


/* how to handle messages without any recipient addresses */
EXTERN int		NoRecipientAction;

#define NRA_NO_ACTION		0	/* just leave it as is */
#define NRA_ADD_TO		1	/* add To: header */
#define NRA_ADD_APPARENTLY_TO	2	/* add Apparently-To: header */
#define NRA_ADD_BCC		3	/* add empty Bcc: header */
#define NRA_ADD_TO_UNDISCLOSED	4	/* add To: undisclosed:; header */


/* flags to putxline */
#define PXLF_NOTHINGSPECIAL	0	/* no special mapping */
#define PXLF_MAPFROM		0x0001	/* map From_ to >From_ */
#define PXLF_STRIP8BIT		0x0002	/* strip 8th bit *e
/*
**  Additional definitions
*/


/*
**  Privacy flags
**	These are bit values for the PrivacyFlags word.
*/

#define PRIV_PUBLIC		0	/* what have I got to hide? */
#define PRIV_NEEDMAILHELO	0x0001	/* insist on HELO for MAIL, at least */
#define PRIV_NEEDEXPNHELO	0x0002	/* insist on HELO for EXPN */
#define PRIV_NEEDVRFYHELO	0x0004	/* insist on HELO for VRFY */
#define PRIV_NOEXPN		0x0008	/* disallow EXPN command entirely */
#define PRIV_NOVRFY		0x0010	/* disallow VRFY command entirely */
#define PRIV_AUTHWARNINGS	0x0020	/* flag possible authorization probs */
#define PRIV_NORECEIPTS		0x0040	/* disallow return receipts */
#define PRIV_RESTRICTMAILQ	0x1000	/* restrict mailq command */
#define PRIV_RESTRICTQRUN	0x2000	/* restrict queue run */
#define PRIV_GOAWAY		0x0fff	/* don't give no info, anyway, anyhow */

/* struct defining such things */
struct prival
{
	char	*pv_name;	/* name of privacy flag */
	int	pv_flag;	/* numeric level */
};


/*
**  Flags passed to remotename, parseaddr, allocaddr, and buildaddr.
*/

#define RF_SENDERADDR		0x001	/* this is a sender address */
#define RF_HEADERADDR		0x002	/* this is a header address */
#define RF_CANONICAL		0x004	/* strip comment information */
#define RF_ADDDOMAIN		0x008	/* OK to do domain extension */
#define RF_COPYPARSE		0x010	/* copy parsed user & host */
#define RF_COPYPADDR		0x020	/* copy print address */
#define RF_COPYALL		(RF_COPYPARSE|RF_COPYPADDR)
#define RF_COPYNONE		0


/*
**  Flags passed to safefile.
*/

#define SFF_ANYFILE		0	/* no special restrictions */
#define SFF_MUSTOWN		0x0001	/* user must own this file */
#define SFF_NOSLINK		0x0002	/* file cannot be a symbolic link */
#define SFF_ROOTOK		0x0004	/* ok for root to own this file */
#define SFF_RUNASREALUID	0x0008	/* if no ctladdr, run as real uid */
#define SFF_NOPATHCHECK		0x0010	/* don't bother checking dir path */
#define SFF_SETUIDOK		0x0020	/* setuid files are ok */
#define SFF_CREAT		0x0040	/* ok to create file if necessary */
#define SFF_REGONLY		0x0080	/* regular files only */

/* flags that are actually specific to safefopen */
#define SFF_OPENASROOT		0x1000	/* open as root instead of real user */


/*
**  Flags passed to mime8to7.
*/

#define M87F_OUTER		0	/* outer context */
#define M87F_NO8BIT		0x0001	/* can't have 8-bit in this section */
#define M87F_DIGEST		0x0002	/* processing multipart/digest */


/*
**  Regular UNIX sockaddrs are too small to handle ISO addresses, so
**  we are forced to declare a supertype here.
*/

#ifdef DAEMON
union bigsockaddr
{
	struct sockaddr		sa;	/* general version */
#if NETUNIX
	struct sockaddr_un	sunix;	/* UNIX family */
#endif
#if NETINET
	struct sockaddr_in	sin;	/* INET family */
#endif
#if NETISO
	struct sockaddr_iso	siso;	/* ISO family */
#endif
#if NETNS
	struct sockaddr_ns	sns;	/* XNS family */
#endif
#if NETX25
	struct sockaddr_x25	sx25;	/* X.25 family */
#endif
};

#define SOCKADDR	union bigsockaddr

EXTERN SOCKADDR RealHostAddr;	/* address of host we are talking to */
extern char	*anynet_ntoa __P((SOCKADDR *));

#endif


/*
**  Vendor codes
**
**	Vendors can customize sendmail to add special behaviour,
**	generally for back compatibility.  Ideally, this should
**	be set up in the .cf file using the "V" command.  However,
**	it's quite reasonable for some vendors to want the default
**	be their old version; this can be set using
**		-DVENDOR_DEFAULT=VENDOR_xxx
**	in the Makefile.
**
**	Vendors should apply to sendmail@CS.Berkeley.EDU for
**	unique vendor codes.
*/

#define VENDOR_BERKELEY	1	/* Berkeley-native configuration file */
#define VENDOR_SUN	2	/* Sun-native configuration file */
#define VENDOR_HP	3	/* Hewlett-Packard specific config syntax */
#define VENDOR_IBM	4	/* IBM specific config syntax */

EXTERN int	VendorCode;	/* vendor-specific operation enhancements */
/*
**  Global variables.
*/

EXTERN bool	FromFlag;	/* if set, "From" person is explicit */
EXTERN bool	MeToo;		/* send to the sender also */
EXTERN bool	IgnrDot;	/* don't let dot end messages */
EXTERN bool	SaveFrom;	/* save leading "From" lines */
EXTERN bool	Verbose;	/* set if blow-by-blow desired */
EXTERN bool	GrabTo;		/* if set, get recipients from msg */
EXTERN bool	SuprErrs;	/* set if we are suppressing errors */
EXTERN bool	HoldErrs;	/* only output errors to transcript */
EXTERN bool	NoConnect;	/* don't connect to non-local mailers */
EXTERN bool	SuperSafe;	/* be extra careful, even if expensive */
EXTERN bool	ForkQueueRuns;	/* fork for each job when running the queue */
EXTERN bool	AutoRebuild;	/* auto-rebuild the alias database as needed */
EXTERN bool	CheckAliases;	/* parse addresses during newaliases */
EXTERN bool	NoAlias;	/* suppress aliasing */
EXTERN bool	UseNameServer;	/* using DNS -- interpret h_errno & MX RRs */
EXTERN bool	UseHesiod;	/* using Hesiod -- interpret Hesiod errors */
EXTERN bool	SevenBitInput;	/* force 7-bit data on input */
EXTERN bool	HasEightBits;	/* has at least one eight bit input byte */
EXTERN time_t	SafeAlias;	/* interval to wait until @:@ in alias file */
EXTERN FILE	*InChannel;	/* input connection */
EXTERN FILE	*OutChannel;	/* output connection */
EXTERN char	*RealUserName;	/* real user name of caller */
EXTERN uid_t	RealUid;	/* real uid of caller */
EXTERN gid_t	RealGid;	/* real gid of caller */
EXTERN uid_t	DefUid;		/* default uid to run as */
EXTERN gid_t	DefGid;		/* default gid to run as */
EXTERN char	*DefUser;	/* default user to run as (from DefUid) */
EXTERN int	OldUmask;	/* umask when sendmail starts up */
EXTERN int	Errors;		/* set if errors (local to single pass) */
EXTERN int	ExitStat;	/* exit status code */
EXTERN int	LineNumber;	/* line number in current input */
EXTERN int	LogLevel;	/* level of logging to perform */
EXTERN int	FileMode;	/* mode on files */
EXTERN int	QueueLA;	/* load average starting forced queueing */
EXTERN int	RefuseLA;	/* load average refusing connections are */
EXTERN int	CurrentLA;	/* current load average */
EXTERN long	QueueFactor;	/* slope of queue function */
EXTERN time_t	QueueIntvl;	/* intervals between running the queue */
EXTERN char	*HelpFile;	/* location of SMTP help file */
EXTERN char	*ErrMsgFile;	/* file to prepend to all error messages */
EXTERN char	*StatFile;	/* location of statistics summary */
EXTERN char	*QueueDir;	/* location of queue directory */
EXTERN char	*FileName;	/* name to print on error messages */
EXTERN char	*SmtpPhase;	/* current phase in SMTP processing */
EXTERN char	*MyHostName;	/* name of this host for SMTP messages */
EXTERN char	*RealHostName;	/* name of host we are talking to */
EXTERN char	*CurHostName;	/* current host we are dealing with */
EXTERN jmp_buf	TopFrame;	/* branch-to-top-of-loop-on-error frame */
EXTERN bool	QuickAbort;	/*  .... but only if we want a quick abort */
EXTERN bool	LogUsrErrs;	/* syslog user errors (e.g., SMTP RCPT cmd) */
EXTERN bool	SendMIMEErrors;	/* send error messages in MIME format */
EXTERN bool	MatchGecos;	/* look for user names in gecos field */
EXTERN bool	UseErrorsTo;	/* use Errors-To: header (back compat) */
EXTERN bool	TryNullMXList;	/* if we are the best MX, try host directly */
EXTERN bool	InChild;	/* true if running in an SMTP subprocess */
EXTERN bool	DisConnected;	/* running with OutChannel redirected to xf */
EXTERN bool	ColonOkInAddr;	/* single colon legal in address */
EXTERN bool	HasWildcardMX;	/* don't use MX records when canonifying */
EXTERN char	SpaceSub;	/* substitution for <lwsp> */
EXTERN int	PrivacyFlags;	/* privacy flags */
EXTERN char	*ConfFile;	/* location of configuration file [conf.c] */
extern char	*PidFile;	/* location of proc id file [conf.c] */
extern ADDRESS	NullAddress;	/* a null (template) address [main.c] */
EXTERN long	WkClassFact;	/* multiplier for message class -> priority */
EXTERN long	WkRecipFact;	/* multiplier for # of recipients -> priority */
EXTERN long	WkTimeFact;	/* priority offset each time this job is run */
EXTERN char	*UdbSpec;	/* user database source spec */
EXTERN int	MaxHopCount;	/* max # of hops until bounce */
EXTERN int	ConfigLevel;	/* config file level */
EXTERN char	*TimeZoneSpec;	/* override time zone specification */
EXTERN char	*ForwardPath;	/* path to search for .forward files */
EXTERN long	MinBlocksFree;	/* min # of blocks free on queue fs */
EXTERN char	*FallBackMX;	/* fall back MX host */
EXTERN long	MaxMessageSize;	/* advertised max size we will accept */
EXTERN time_t	MaxHostStatAge;	/* max age of cached host status info */
EXTERN time_t	MinQueueAge;	/* min delivery interval */
EXTERN time_t	DialDelay;	/* delay between dial-on-demand tries */
EXTERN char	*SafeFileEnv;	/* chroot location for file delivery */
EXTERN char	*HostsFile;	/* path to /etc/hosts file */
EXTERN int	MaxQueueRun;	/* maximum number of jobs in one queue run */
EXTERN int	MaxChildren;	/* maximum number of daemonic children */
EXTERN int	CurChildren;	/* current number of daemonic children */
EXTERN char	*SmtpGreeting;	/* SMTP greeting message (old $e macro) */
EXTERN char	*UnixFromLine;	/* UNIX From_ line (old $l macro) */
EXTERN char	*OperatorChars;	/* operators (old $o macro) */
EXTERN bool	DontInitGroups;	/* avoid initgroups() because of NIS cost */
EXTERN bool	SingleLineFromHeader;	/* force From: header to be one line */
EXTERN int	MaxAliasRecursion;	/* maximum depth of alias recursion */
EXTERN int	MaxRuleRecursion;	/* maximum depth of ruleset recursion */
EXTERN char	*MustQuoteChars;	/* quote these characters in phrases */
EXTERN char	*ServiceSwitchFile;	/* backup service switch */
EXTERN char	*DefaultCharSet;	/* default character set for MIME */
EXTERN int	DeliveryNiceness;	/* how nice to be during delivery */
EXTERN char	*PostMasterCopy;	/* address to get errs cc's */
EXTERN int	CheckpointInterval;	/* queue file checkpoint interval */
EXTERN bool	DontPruneRoutes;	/* don't prune source routes */
EXTERN bool	DontExpandCnames;	/* do not $[...$] expand CNAMEs */
EXTERN int	MaxMciCache;		/* maximum entries in MCI cache */
EXTERN time_t	MciCacheTimeout;	/* maximum idle time on connections */
EXTERN time_t	MciInfoTimeout;		/* how long 'til we retry down hosts */
EXTERN char	*QueueLimitRecipient;	/* limit queue runs to this recipient */
EXTERN char	*QueueLimitSender;	/* limit queue runs to this sender */
EXTERN char	*QueueLimitId;		/* limit queue runs to this id */
EXTERN FILE	*TrafficLogFile;	/* file in which to log all traffic */
EXTERN char	*UserEnviron[MAXUSERENVIRON + 1];
					/* saved user environment */
extern int	errno;


/*
**  Timeouts
**
**	Indicated values are the MINIMUM per RFC 1123 section 5.3.2.
*/

EXTERN struct
{
			/* RFC 1123-specified timeouts [minimum value] */
	time_t	to_initial;	/* initial greeting timeout [5m] */
	time_t	to_mail;	/* MAIL command [5m] */
	time_t	to_rcpt;	/* RCPT command [5m] */
	time_t	to_datainit;	/* DATA initiation [2m] */
	time_t	to_datablock;	/* DATA block [3m] */
	time_t	to_datafinal;	/* DATA completion [10m] */
	time_t	to_nextcommand;	/* next command [5m] */
			/* following timeouts are not mentioned in RFC 1123 */
	time_t	to_connect;	/* initial connection timeout */
	time_t	to_rset;	/* RSET command */
	time_t	to_helo;	/* HELO command */
	time_t	to_quit;	/* QUIT command */
	time_t	to_miscshort;	/* misc short commands (NOOP, VERB, etc) */
	time_t	to_ident;	/* IDENT protocol requests */
	time_t	to_fileopen;	/* opening :include: and .forward files */
			/* following are per message */
	time_t	to_q_return[MAXTOCLASS];	/* queue return timeouts */
	time_t	to_q_warning[MAXTOCLASS];	/* queue warning timeouts */
} TimeOuts;

/* timeout classes for return and warning timeouts */
# define TOC_NORMAL	0	/* normal delivery */
# define TOC_URGENT	1	/* urgent delivery */
# define TOC_NONURGENT	2	/* non-urgent delivery */


/*
**  Trace information
*/

/* trace vector and macros for debugging flags */
EXTERN u_char	tTdvect[100];
# define tTd(flag, level)	(tTdvect[flag] >= level)
# define tTdlevel(flag)		(tTdvect[flag])
/*
**  Miscellaneous information.
*/



/*
**  Some in-line functions
*/

/* set exit status */
#define setstat(s)	{ \
				if (ExitStat == EX_OK || ExitStat == EX_TEMPFAIL) \
					ExitStat = s; \
			}

/* make a copy of a string */
#define newstr(s)	strcpy(xalloc(strlen(s) + 1), s)

#define STRUCTCOPY(s, d)	d = s


/*
**  Declarations of useful functions
*/

extern char	*xalloc __P((int));
extern FILE	*dfopen __P((char *, int, int));
extern char	*sfgets __P((char *, int, FILE *, time_t, char *));
extern char	*queuename __P((ENVELOPE *, int));
extern time_t	curtime __P(());
extern bool	transienterror __P((int));
extern char	*fgetfolded __P((char *, int, FILE *));
extern char	*username __P(());
extern char	*pintvl __P((time_t, bool));
extern bool	shouldqueue __P((long, time_t));
extern bool	lockfile __P((int, char *, char *, int));
extern char	*hostsignature __P((MAILER *, char *, ENVELOPE *));
extern void	openxscript __P((ENVELOPE *));
extern void	closexscript __P((ENVELOPE *));
extern char	*shortenstring __P((const char *, int));
extern bool	usershellok __P((char *, char *));
extern void	commaize __P((HDR *, char *, bool, MCI *, ENVELOPE *));
extern char	*hvalue __P((char *, HDR *));
extern char	*defcharset __P((ENVELOPE *));
extern bool	wordinclass __P((char *, int));
extern char	*denlstring __P((char *, bool, bool));
extern void	makelower __P((char *));
extern void	rebuildaliases __P((MAP *, bool));
extern void	readaliases __P((MAP *, FILE *, bool, bool));
extern void	finis __P(());
extern void	setsender __P((char *, ENVELOPE *, char **, bool));
extern FILE	*safefopen __P((char *, int, int, int));
extern void	xputs __P((const char *));
extern void	logsender __P((ENVELOPE *, char *));
extern void	smtprset __P((MAILER *, MCI *, ENVELOPE *));
extern void	smtpquit __P((MAILER *, MCI *, ENVELOPE *));
extern void	setuserenv __P((const char *, const char *));
extern void	disconnect __P((int, ENVELOPE *));
extern void	putxline __P((char *, MCI *, int));
extern void	dumpfd __P((int, bool, bool));
extern void	makemailer __P((char *));
extern void	putfromline __P((MCI *, ENVELOPE *));
extern void	setoption __P((int, char *, bool, bool, ENVELOPE *));
extern void	setclass __P((int, char *));
extern void	inittimeouts __P((char *));
extern void	logdelivery __P((MAILER *, MCI *, const char *, ADDRESS *, time_t, ENVELOPE *));
extern void	giveresponse __P((int, MAILER *, MCI *, ADDRESS *, time_t, ENVELOPE *));
extern void	buildfname __P((char *, char *, char *, int));

extern const char	*errstring __P((int));
extern sigfunc_t	setsignal __P((int, sigfunc_t));
extern struct hostent	*sm_gethostbyname __P((char *));
extern struct hostent	*sm_gethostbyaddr __P((char *, int, int));
extern struct passwd	*sm_getpwnam __P((char *));
extern struct passwd	*sm_getpwuid __P((UID_T));

#ifdef XDEBUG
extern void		checkfd012 __P((char *));
#endif

/* ellipsis is a different case though */
#ifdef __STDC__
extern void		auth_warning(ENVELOPE *, const char *, ...);
extern void		syserr(const char *, ...);
extern void		usrerr(const char *, ...);
extern void		message(const char *, ...);
extern void		nmessage(const char *, ...);
#else
extern void		auth_warning();
extern void		syserr();
extern void		usrerr();
extern void		message();
extern void		nmessage();
#endif

#if !HASSNPRINTF
# ifdef __STDC__
extern int		snprintf(char *, size_t, const char *, ...);
extern int		vsnprintf(char *, size_t, const char *, va_list);
# else
extern int		snprintf();
extern int		vsnprintf();
# endif
#endif
