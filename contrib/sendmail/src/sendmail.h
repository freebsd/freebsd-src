/*
 * Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

/*
**  SENDMAIL.H -- MTA-specific definitions for sendmail.
*/

#ifndef _SENDMAIL_H
#define _SENDMAIL_H 1

#ifdef _DEFINE
# define EXTERN
# ifndef lint
static char SmailId[] =	"@(#)$Id: sendmail.h,v 8.517.4.64 2001/05/23 17:49:13 ca Exp $";
# endif /* ! lint */
#else /* _DEFINE */
# define EXTERN extern
#endif /* _DEFINE */


#include <unistd.h>

#if SFIO
# include <sfio/stdio.h>
# if defined(SFIO_VERSION) && SFIO_VERSION > 20000000L
   ERROR README: SFIO 2000 does not work with sendmail, use SFIO 1999 instead.
# endif /* defined(SFIO_VERSION) && SFIO_VERSION > 20000000L */
#endif /* SFIO */

#include <stddef.h>
#include <stdlib.h>
#if !SFIO
# include <stdio.h>
#endif /* !SFIO */
#include <ctype.h>
#include <setjmp.h>
#include <string.h>
#include <time.h>
# ifdef EX_OK
#  undef EX_OK			/* for SVr4.2 SMP */
# endif /* EX_OK */
#include <sysexits.h>

#include "sendmail/sendmail.h"
#include "bf.h"
#include "timers.h"

#ifdef LOG
# include <syslog.h>
#endif /* LOG */



# if NETINET || NETINET6 || NETUNIX || NETISO || NETNS || NETX25
#  include <sys/socket.h>
# endif /* NETINET || NETINET6 || NETUNIX || NETISO || NETNS || NETX25 */
# if NETUNIX
#  include <sys/un.h>
# endif /* NETUNIX */
# if NETINET || NETINET6
#  include <netinet/in.h>
# endif /* NETINET || NETINET6 */
# if NETINET6
/*
**  There is no standard yet for IPv6 includes.
**  Specify OS specific implementation in conf.h
*/
# endif /* NETINET6 */
# if NETISO
#  include <netiso/iso.h>
# endif /* NETISO */
# if NETNS
#  include <netns/ns.h>
# endif /* NETNS */
# if NETX25
#  include <netccitt/x25.h>
# endif /* NETX25 */

# if NAMED_BIND
#  include <arpa/nameser.h>
#  ifdef NOERROR
#   undef NOERROR		/* avoid <sys/streams.h> conflict */
#  endif /* NOERROR */
#  include <resolv.h>
# endif /* NAMED_BIND */

# ifdef HESIOD
#  include <hesiod.h>
#  if !defined(HES_ER_OK) || defined(HESIOD_INTERFACES)
#   define HESIOD_INIT		/* support for the new interface */
#  endif /* !defined(HES_ER_OK) || defined(HESIOD_INTERFACES) */
# endif /* HESIOD */

#if STARTTLS
# if !SFIO && !_FFR_TLS_TOREK
  ERROR README: STARTTLS requires SFIO
# endif /* !SFIO && !_FFR_TLS_TOREK */
# if SFIO && _FFR_TLS_TOREK
  ERROR README: Can not do both SFIO and _FFR_TLS_TOREK
# endif /* SFIO && _FFR_TLS_TOREK */
#  include <openssl/ssl.h>
#endif /* STARTTLS */

#if SASL  /* include the sasl include files if we have them */
# include <sasl.h>
# if defined(SASL_VERSION_MAJOR) && defined(SASL_VERSION_MINOR) && defined(SASL_VERSION_STEP)
#  define SASL_VERSION (SASL_VERSION_MAJOR * 10000)  + (SASL_VERSION_MINOR * 100) + SASL_VERSION_STEP
#  if SASL == 1
#   undef SASL
#   define SASL SASL_VERSION
#  else /* SASL == 1 */
#   if SASL != SASL_VERSION
  ERROR README: -DSASL (SASL) does not agree with the version of the CYRUS_SASL library (SASL_VERSION)
  ERROR README: see README!
#   endif /* SASL != SASL_VERSION */
#  endif /* SASL == 1 */
# else /* defined(SASL_VERSION_MAJOR) && defined(SASL_VERSION_MINOR) && defined(SASL_VERSION_STEP) */
#  if SASL == 1
  ERROR README: please set -DSASL to the version of the CYRUS_SASL library
  ERROR README: see README!
#  endif /* SASL == 1 */
# endif /* defined(SASL_VERSION_MAJOR) && defined(SASL_VERSION_MINOR) && defined(SASL_VERSION_STEP) */
#endif /* SASL */

/*
**  Following are "sort of" configuration constants, but they should
**  be pretty solid on most architectures today.  They have to be
**  defined after <arpa/nameser.h> because some versions of that
**  file also define them.  In all cases, we can't use sizeof because
**  some systems (e.g., Crays) always treat everything as being at
**  least 64 bits.
*/

#ifndef INADDRSZ
# define INADDRSZ	4		/* size of an IPv4 address in bytes */
#endif /* ! INADDRSZ */
#ifndef IN6ADDRSZ
# define IN6ADDRSZ	16		/* size of an IPv6 address in bytes */
#endif /* ! IN6ADDRSZ */
#ifndef INT16SZ
# define INT16SZ	2		/* size of a 16 bit integer in bytes */
#endif /* ! INT16SZ */
#ifndef INT32SZ
# define INT32SZ	4		/* size of a 32 bit integer in bytes */
#endif /* ! INT32SZ */
#ifndef INADDR_LOOPBACK
# define INADDR_LOOPBACK	0x7f000001	/* loopback address */
#endif /* ! INADDR_LOOPBACK */

/*
**  Error return from inet_addr(3), in case not defined in /usr/include.
*/

#ifndef INADDR_NONE
# define INADDR_NONE	0xffffffff
#endif /* ! INADDR_NONE */


/* forward references for prototypes */
typedef struct envelope	ENVELOPE;
typedef struct mailer	MAILER;

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
	short		q_state;	/* address state, see below */
	short		q_specificity;	/* how "specific" this address is */
};

typedef struct address ADDRESS;

/* bit values for q_flags */
#define QGOODUID	0x00000001	/* the q_uid q_gid fields are good */
#define QPRIMARY	0x00000002	/* set from RCPT or argv */
#define QNOTREMOTE	0x00000004	/* address not for remote forwarding */
#define QSELFREF	0x00000008	/* this address references itself */
#define QBOGUSSHELL	0x00000010	/* user has no valid shell listed */
#define QUNSAFEADDR	0x00000020	/* address acquired via unsafe path */
#define QPINGONSUCCESS	0x00000040	/* give return on successful delivery */
#define QPINGONFAILURE	0x00000080	/* give return on failure */
#define QPINGONDELAY	0x00000100	/* give return on message delay */
#define QHASNOTIFY	0x00000200	/* propogate notify parameter */
#define QRELAYED	0x00000400	/* DSN: relayed to non-DSN aware sys */
#define QEXPANDED	0x00000800	/* DSN: undergone list expansion */
#define QDELIVERED	0x00001000	/* DSN: successful final delivery */
#define QDELAYED	0x00002000	/* DSN: message delayed */
#define QALIAS		0x00004000	/* expanded alias */
#define QTHISPASS	0x40000000	/* temp: address set this pass */
#define QRCPTOK		0x80000000	/* recipient() processed address */

#define Q_PINGFLAGS	(QPINGONSUCCESS|QPINGONFAILURE|QPINGONDELAY)

/* values for q_state */
#define QS_OK		0		/* address ok (for now)/not yet tried */
#define QS_SENT		1		/* good address, delivery complete */
#define QS_BADADDR	2		/* illegal address */
#define QS_QUEUEUP	3		/* save address in queue */
#define QS_VERIFIED	4		/* verified, but not expanded */
#define QS_DONTSEND	5		/* don't send to this address */
#define QS_EXPANDED	6		/* QS_DONTSEND: expanded */
#define QS_SENDER	7		/* QS_DONTSEND: message sender (MeToo) */
#define QS_CLONED	8		/* QS_DONTSEND: addr cloned to split envelope */
#define QS_DISCARDED	9		/* QS_DONTSEND: rcpt discarded (EF_DISCARD) */
#define QS_REPLACED	10		/* QS_DONTSEND: maplocaluser()/UserDB replaced */
#define QS_REMOVED	11		/* QS_DONTSEND: removed (removefromlist()) */
#define QS_DUPLICATE	12		/* QS_DONTSEND: duplicate suppressed */
#define QS_INCLUDED	13		/* QS_DONTSEND: :include: delivery */

/* address state testing primitives */
#define QS_IS_OK(s)		((s) == QS_OK)
#define QS_IS_SENT(s)		((s) == QS_SENT)
#define QS_IS_BADADDR(s)	((s) == QS_BADADDR)
#define QS_IS_QUEUEUP(s)	((s) == QS_QUEUEUP)
#define QS_IS_VERIFIED(s)	((s) == QS_VERIFIED)
#define QS_IS_EXPANDED(s)	((s) == QS_EXPANDED)
#define QS_IS_REMOVED(s)	((s) == QS_REMOVED)
#define QS_IS_UNDELIVERED(s)	((s) == QS_OK || \
				 (s) == QS_QUEUEUP || \
				 (s) == QS_VERIFIED)
#define QS_IS_SENDABLE(s)	((s) == QS_OK || \
				 (s) == QS_QUEUEUP)
#define QS_IS_ATTEMPTED(s)	((s) == QS_QUEUEUP || \
				 (s) == QS_SENT)
#define QS_IS_DEAD(s)		((s) == QS_DONTSEND || \
				 (s) == QS_CLONED || \
				 (s) == QS_SENDER || \
				 (s) == QS_DISCARDED || \
				 (s) == QS_REPLACED || \
				 (s) == QS_REMOVED || \
				 (s) == QS_DUPLICATE || \
				 (s) == QS_INCLUDED || \
				 (s) == QS_EXPANDED)


#define NULLADDR	((ADDRESS *) NULL)

extern ADDRESS	NullAddress;	/* a null (template) address [main.c] */

/* functions */
extern void	cataddr __P((char **, char **, char *, int, int));
extern char	*crackaddr __P((char *));
extern bool	emptyaddr __P((ADDRESS *));
extern ADDRESS	*getctladdr __P((ADDRESS *));
extern int	include __P((char *, bool, ADDRESS *, ADDRESS **, int, ENVELOPE *));
extern bool	invalidaddr __P((char *, char *));
extern ADDRESS	*parseaddr __P((char *, ADDRESS *, int, int, char **, ENVELOPE *));
extern char	**prescan __P((char *, int, char[], int, char **, u_char *));
extern void	printaddr __P((ADDRESS *, bool));
extern ADDRESS	*recipient __P((ADDRESS *, ADDRESS **, int, ENVELOPE *));
extern char	*remotename __P((char *, MAILER *, int, int *, ENVELOPE *));
extern int	rewrite __P((char **, int, int, ENVELOPE *));
extern bool	sameaddr __P((ADDRESS *, ADDRESS *));
extern int	sendtolist __P((char *, ADDRESS *, ADDRESS **, int, ENVELOPE *));
extern int	removefromlist __P((char *, ADDRESS **, ENVELOPE *));
extern void	setsender __P((char *, ENVELOPE *, char **, int, bool));

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
	BITMAP256 m_flags;	/* status flags, see below */
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
	int	m_maxdeliveries; /* max deliveries per mailer connection */
	char	*m_execdir;	/* directory to chdir to before execv */
	char	*m_rootdir;	/* directory to chroot to before execv */
	uid_t	m_uid;		/* UID to run as */
	gid_t	m_gid;		/* GID to run as */
	char	*m_defcharset;	/* default character set */
	time_t	m_wait;		/* timeout to wait for end */
#if _FFR_DYNAMIC_TOBUF
	int	m_maxrcpt;	/* max recipients per envelope client-side */
#endif /* _FFR_DYNAMIC_TOBUF */
};

/* bits for m_flags */
#define M_ESMTP		'a'	/* run Extended SMTP protocol */
#define M_ALIASABLE	'A'	/* user can be LHS of an alias */
#define M_BLANKEND	'b'	/* ensure blank line at end of message */
#define M_NOCOMMENT	'c'	/* don't include comment part of address */
#define M_CANONICAL	'C'	/* make addresses canonical "u@dom" */
#define M_NOBRACKET	'd'	/* never angle bracket envelope route-addrs */
		/*	'D'	   CF: include Date: */
#define M_EXPENSIVE	'e'	/* it costs to use this mailer.... */
#define M_ESCFROM	'E'	/* escape From lines to >From */
#define M_FOPT		'f'	/* mailer takes picky -f flag */
		/*	'F'	   CF: include From: or Resent-From: */
#define M_NO_NULL_FROM	'g'	/* sender of errors should be $g */
#define M_HST_UPPER	'h'	/* preserve host case distinction */
#define M_PREHEAD	'H'	/* MAIL11V3: preview headers */
#define M_UDBENVELOPE	'i'	/* do udbsender rewriting on envelope */
#define M_INTERNAL	'I'	/* SMTP to another sendmail site */
#define M_UDBRECIPIENT	'j'	/* do udbsender rewriting on recipient lines */
#define M_NOLOOPCHECK	'k'	/* don't check for loops in HELO command */
#define M_CHUNKING	'K'	/* CHUNKING: reserved for future use */
#define M_LOCALMAILER	'l'	/* delivery is to this host */
#define M_LIMITS	'L'	/* must enforce SMTP line limits */
#define M_MUSER		'm'	/* can handle multiple users at once */
		/*	'M'	   CF: include Message-Id: */
#define M_NHDR		'n'	/* don't insert From line */
#define M_MANYSTATUS	'N'	/* MAIL11V3: DATA returns multi-status */
#define M_RUNASRCPT	'o'	/* always run mailer as recipient */
#define M_FROMPATH	'p'	/* use reverse-path in MAIL FROM: */
		/*	'P'	   CF: include Return-Path: */
#define M_VRFY250	'q'	/* VRFY command returns 250 instead of 252 */
#define M_ROPT		'r'	/* mailer takes picky -r flag */
#define M_SECURE_PORT	'R'	/* try to send on a reserved TCP port */
#define M_STRIPQ	's'	/* strip quote chars from user/host */
#define M_SPECIFIC_UID	'S'	/* run as specific uid/gid */
#define M_USR_UPPER	'u'	/* preserve user case distinction */
#define M_UGLYUUCP	'U'	/* this wants an ugly UUCP from line */
#define M_CONTENT_LEN	'v'	/* add Content-Length: header (SVr4) */
		/*	'V'	   UIUC: !-relativize all addresses */
#define M_HASPWENT	'w'	/* check for /etc/passwd entry */
		/*	'x'	   CF: include Full-Name: */
#define M_XDOT		'X'	/* use hidden-dot algorithm */
#define M_LMTP		'z'	/* run Local Mail Transport Protocol */
#define M_NOMX		'0'	/* turn off MX lookups */
#define M_NONULLS	'1'	/* don't send null bytes */
#define M_EBCDIC	'3'	/* extend Q-P encoding for EBCDIC */
#define M_TRYRULESET5	'5'	/* use ruleset 5 after local aliasing */
#define M_7BITHDRS	'6'	/* strip headers to 7 bits even in 8 bit path */
#define M_7BITS		'7'	/* use 7-bit path */
#define M_8BITS		'8'	/* force "just send 8" behaviour */
#define M_MAKE8BIT	'9'	/* convert 7 -> 8 bit if appropriate */
#define M_CHECKINCLUDE	':'	/* check for :include: files */
#define M_CHECKPROG	'|'	/* check for |program addresses */
#define M_CHECKFILE	'/'	/* check for /file addresses */
#define M_CHECKUDB	'@'	/* user can be user database key */
#define M_CHECKHDIR	'~'	/* SGI: check for valid home directory */
#define M_HOLD		'%'	/* Hold delivery until ETRN/-qI/-qR/-qS */
#define M_PLUS		'+'	/* Reserved: Used in mc for adding new flags */
#define M_MINUS		'-'	/* Reserved: Used in mc for removing flags */

/* functions */
extern void	initerrmailers __P((void));
extern void	makemailer __P((char *));

/*
**  Information about currently open connections to mailers, or to
**  hosts that we have looked up recently.
*/

#define MCI		struct mailer_con_info

MCI
{
	u_long		mci_flags;	/* flag bits, see below */
	short		mci_errno;	/* error number on last connection */
	short		mci_herrno;	/* h_errno from last DNS lookup */
	short		mci_exitstat;	/* exit status from last connection */
	short		mci_state;	/* SMTP state */
	int		mci_deliveries;	/* delivery attempts for connection */
	long		mci_maxsize;	/* max size this server will accept */
#if SFIO
	Sfio_t		*mci_in;	/* input side of connection */
	Sfio_t		*mci_out;	/* output side of connection */
#else /* SFIO */
	FILE		*mci_in;	/* input side of connection */
	FILE		*mci_out;	/* output side of connection */
#endif /* SFIO */
	pid_t		mci_pid;	/* process id of subordinate proc */
	char		*mci_phase;	/* SMTP phase string */
	struct mailer	*mci_mailer;	/* ptr to the mailer for this conn */
	char		*mci_host;	/* host name */
	char		*mci_status;	/* DSN status to be copied to addrs */
	char		*mci_rstatus;	/* SMTP status to be copied to addrs */
	time_t		mci_lastuse;	/* last usage time */
	FILE		*mci_statfile;	/* long term status file */
	char		*mci_heloname;	/* name to use as HELO arg */
#if SASL
	bool		mci_sasl_auth;	/* authenticated? */
	int		mci_sasl_string_len;
	char		*mci_sasl_string;	/* sasl reply string */
	char		*mci_saslcap;	/* SASL list of mechanisms */
	sasl_conn_t	*mci_conn;	/* SASL connection */
#endif /* SASL */
#if STARTTLS
	SSL		*mci_ssl;	/* SSL connection */
#endif /* STARTTLS */
};


/* flag bits */
#define MCIF_VALID	0x00000001	/* this entry is valid */
#define MCIF_TEMP	0x00000002	/* don't cache this connection */
#define MCIF_CACHED	0x00000004	/* currently in open cache */
#define MCIF_ESMTP	0x00000008	/* this host speaks ESMTP */
#define MCIF_EXPN	0x00000010	/* EXPN command supported */
#define MCIF_SIZE	0x00000020	/* SIZE option supported */
#define MCIF_8BITMIME	0x00000040	/* BODY=8BITMIME supported */
#define MCIF_7BIT	0x00000080	/* strip this message to 7 bits */
#define MCIF_MULTSTAT	0x00000100	/* MAIL11V3: handles MULT status */
#define MCIF_INHEADER	0x00000200	/* currently outputing header */
#define MCIF_CVT8TO7	0x00000400	/* convert from 8 to 7 bits */
#define MCIF_DSN	0x00000800	/* DSN extension supported */
#define MCIF_8BITOK	0x00001000	/* OK to send 8 bit characters */
#define MCIF_CVT7TO8	0x00002000	/* convert from 7 to 8 bits */
#define MCIF_INMIME	0x00004000	/* currently reading MIME header */
#define MCIF_AUTH	0x00008000	/* AUTH= supported */
#define MCIF_AUTHACT	0x00010000	/* SASL (AUTH) active */
#define MCIF_ENHSTAT	0x00020000	/* ENHANCEDSTATUSCODES supported */
#if STARTTLS
#define MCIF_TLS	0x00100000	/* STARTTLS supported */
#define MCIF_TLSACT	0x00200000	/* STARTTLS active */
#define MCIF_EXTENS	(MCIF_EXPN | MCIF_SIZE | MCIF_8BITMIME | MCIF_DSN | MCIF_8BITOK | MCIF_AUTH | MCIF_ENHSTAT | MCIF_TLS)
#else /* STARTTLS */
#define MCIF_EXTENS	(MCIF_EXPN | MCIF_SIZE | MCIF_8BITMIME | MCIF_DSN | MCIF_8BITOK | MCIF_AUTH | MCIF_ENHSTAT)
#endif /* STARTTLS */
#define MCIF_ONLY_EHLO	0x10000000	/* use only EHLO in smtpinit */


/* states */
#define MCIS_CLOSED	0		/* no traffic on this connection */
#define MCIS_OPENING	1		/* sending initial protocol */
#define MCIS_OPEN	2		/* open, initial protocol sent */
#define MCIS_ACTIVE	3		/* message being sent */
#define MCIS_QUITING	4		/* running quit protocol */
#define MCIS_SSD	5		/* service shutting down */
#define MCIS_ERROR	6		/* I/O error on connection */

/* functions */
extern void	mci_cache __P((MCI *));
extern void	mci_dump __P((MCI *, bool));
extern void	mci_dump_all __P((bool));
extern void	mci_flush __P((bool, MCI *));
extern MCI	*mci_get __P((char *, MAILER *));
extern int	mci_lock_host __P((MCI *));
extern bool	mci_match __P((char *, MAILER *));
extern int	mci_print_persistent __P((char *, char *));
extern int	mci_purge_persistent __P((char *, char *));
extern MCI	**mci_scan __P((MCI *));
extern void	mci_setstat __P((MCI *, int, char *, char *));
extern void	mci_store_persistent __P((MCI *));
extern int	mci_traverse_persistent __P((int (*)(), char *));
extern void	mci_unlock_host __P((MCI *));

/*
**  Header structure.
**	This structure is used internally to store header items.
*/

struct header
{
	char		*h_field;	/* the name of the field */
	char		*h_value;	/* the value of that field */
	struct header	*h_link;	/* the next header */
	u_char		h_macro;	/* include header if macro defined */
	u_long		h_flags;	/* status bits, see below */
	BITMAP256	h_mflags;	/* m_flags bits needed */
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
	u_long	hi_flags;	/* status bits, see below */
	char	*hi_ruleset;	/* validity check ruleset */
};

extern struct hdrinfo	HdrInfo[];

/* bits for h_flags and hi_flags */
#define H_EOH		0x00000001	/* field terminates header */
#define H_RCPT		0x00000002	/* contains recipient addresses */
#define H_DEFAULT	0x00000004	/* if another value is found, drop this */
#define H_RESENT	0x00000008	/* this address is a "Resent-..." address */
#define H_CHECK		0x00000010	/* check h_mflags against m_flags */
#define H_ACHECK	0x00000020	/* ditto, but always (not just default) */
#define H_FORCE		0x00000040	/* force this field, even if default */
#define H_TRACE		0x00000080	/* this field contains trace information */
#define H_FROM		0x00000100	/* this is a from-type field */
#define H_VALID		0x00000200	/* this field has a validated value */
#define H_RECEIPTTO	0x00000400	/* field has return receipt info */
#define H_ERRORSTO	0x00000800	/* field has error address info */
#define H_CTE		0x00001000	/* field is a content-transfer-encoding */
#define H_CTYPE		0x00002000	/* this is a content-type field */
#define H_BCC		0x00004000	/* Bcc: header: strip value or delete */
#define H_ENCODABLE	0x00008000	/* field can be RFC 1522 encoded */
#define H_STRIPCOMM	0x00010000	/* header check: strip comments */
#define H_BINDLATE	0x00020000	/* only expand macros at deliver */
#define H_USER		0x00040000	/* header came from the user/SMTP */

/* bits for chompheader() */
#define CHHDR_DEF	0x0001	/* default header */
#define CHHDR_CHECK	0x0002	/* call ruleset for header */
#define CHHDR_USER	0x0004	/* header from user */
#define CHHDR_QUEUE	0x0008	/* header from qf file */

/* functions */
extern void	addheader __P((char *, char *, int, HDR **));
extern u_long	chompheader __P((char *, int, HDR **, ENVELOPE *));
extern void	commaize __P((HDR *, char *, bool, MCI *, ENVELOPE *));
extern HDR	*copyheader __P((HDR *));
extern void	eatheader __P((ENVELOPE *, bool));
extern char	*hvalue __P((char *, HDR *));
extern bool	isheader __P((char *));
extern void	putfromline __P((MCI *, ENVELOPE *));
extern void	setupheaders __P((void));

/*
**  Performance monitoring
*/

#define TIMERS		struct sm_timers

TIMERS
{
	TIMER	ti_overall;	/* the whole process */
};


#define PUSHTIMER(l, t)	{ if (tTd(98, l)) pushtimer(&t); }
#define POPTIMER(l, t)	{ if (tTd(98, l)) poptimer(&t); }

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
	ADDRESS		e_from;		/* the person it is from */
	char		*e_sender;	/* e_from.q_paddr w comments stripped */
	char		**e_fromdomain;	/* the domain part of the sender */
	ADDRESS		*e_sendqueue;	/* list of message recipients */
	ADDRESS		*e_errorqueue;	/* the queue for error responses */

	/*
	**  Overflow detection is based on < 0, so don't change this
	**  to unsigned.  We don't use unsigned and == ULONG_MAX because
	**  some libc's don't have strtoul(), see mail_esmtp_args().
	*/
	long		e_msgsize;	/* size of the message in bytes */
	long		e_flags;	/* flags, see below */
	int		e_nrcpts;	/* number of recipients */
	short		e_class;	/* msg class (priority, junk, etc.) */
	short		e_hopcount;	/* number of times processed */
	short		e_nsent;	/* number of sends since checkpoint */
	short		e_sendmode;	/* message send mode */
	short		e_errormode;	/* error return mode */
	short		e_timeoutclass;	/* message timeout class */
	void		(*e_puthdr)__P((MCI *, HDR *, ENVELOPE *, int));
					/* function to put header of message */
	void		(*e_putbody)__P((MCI *, ENVELOPE *, char *));
					/* function to put body of message */
	ENVELOPE	*e_parent;	/* the message this one encloses */
	ENVELOPE	*e_sibling;	/* the next envelope of interest */
	char		*e_bodytype;	/* type of message body */
	FILE		*e_dfp;		/* data file */
	char		*e_id;		/* code for this entry in queue */
	int		e_queuedir;	/* index into queue directories */
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
	char		*e_macro[MAXMACROID + 1]; /* macro definitions */
	char		*e_if_macros[2]; /* HACK: incoming interface info */
	char		*e_auth_param;
	TIMERS		e_timers;	/* per job timers */
#if _FFR_QUEUEDELAY
	int		e_queuealg;	/* algorithm for queue delay */
	time_t		e_queuedelay;	/* current delay */
#endif /* _FFR_QUEUEDELAY */
};

/* values for e_flags */
#define EF_OLDSTYLE	0x0000001L	/* use spaces (not commas) in hdrs */
#define EF_INQUEUE	0x0000002L	/* this message is fully queued */
#define EF_NO_BODY_RETN	0x0000004L	/* omit message body on error */
#define EF_CLRQUEUE	0x0000008L	/* disk copy is no longer needed */
#define EF_SENDRECEIPT	0x0000010L	/* send a return receipt */
#define EF_FATALERRS	0x0000020L	/* fatal errors occurred */
#define EF_DELETE_BCC	0x0000040L	/* delete Bcc: headers entirely */
#define EF_RESPONSE	0x0000080L	/* this is an error or return receipt */
#define EF_RESENT	0x0000100L	/* this message is being forwarded */
#define EF_VRFYONLY	0x0000200L	/* verify only (don't expand aliases) */
#define EF_WARNING	0x0000400L	/* warning message has been sent */
#define EF_QUEUERUN	0x0000800L	/* this envelope is from queue */
#define EF_GLOBALERRS	0x0001000L	/* treat errors as global */
#define EF_PM_NOTIFY	0x0002000L	/* send return mail to postmaster */
#define EF_METOO	0x0004000L	/* send to me too */
#define EF_LOGSENDER	0x0008000L	/* need to log the sender */
#define EF_NORECEIPT	0x0010000L	/* suppress all return-receipts */
#define EF_HAS8BIT	0x0020000L	/* at least one 8-bit char in body */
#define EF_NL_NOT_EOL	0x0040000L	/* don't accept raw NL as EOLine */
#define EF_CRLF_NOT_EOL	0x0080000L	/* don't accept CR-LF as EOLine */
#define EF_RET_PARAM	0x0100000L	/* RCPT command had RET argument */
#define EF_HAS_DF	0x0200000L	/* set when df file is instantiated */
#define EF_IS_MIME	0x0400000L	/* really is a MIME message */
#define EF_DONT_MIME	0x0800000L	/* never MIME this message */
#define EF_DISCARD	0x1000000L	/* discard the message */
#define EF_TOOBIG	0x2000000L	/* message is too big */

/* values for e_if_macros */
#define EIF_ADDR	0		/* ${if_addr} */

/* functions */
extern void	clearenvelope __P((ENVELOPE *, bool));
extern void	dropenvelope __P((ENVELOPE *, bool));
extern ENVELOPE	*newenvelope __P((ENVELOPE *, ENVELOPE *));
extern void	printenvflags __P((ENVELOPE *));
extern void	putbody __P((MCI *, ENVELOPE *, char *));
extern void	putheader __P((MCI *, HDR *, ENVELOPE *, int));

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

/*
**  Rewrite rules.
*/

struct rewrite
{
	char	**r_lhs;	/* pattern match */
	char	**r_rhs;	/* substitution value */
	struct rewrite	*r_next;/* next in chain */
	int	r_line;		/* rule line in sendmail.cf */
};

/*
**  Special characters in rewriting rules.
**	These are used internally only.
**	The COND* rules are actually used in macros rather than in
**		rewriting rules, but are given here because they
**		cannot conflict.
*/

/* left hand side items */
#define MATCHZANY	((u_char)0220)	/* match zero or more tokens */
#define MATCHANY	((u_char)0221)	/* match one or more tokens */
#define MATCHONE	((u_char)0222)	/* match exactly one token */
#define MATCHCLASS	((u_char)0223)	/* match one token in a class */
#define MATCHNCLASS	((u_char)0224)	/* match anything not in class */
#define MATCHREPL	((u_char)0225)	/* replacement on RHS for above */

/* right hand side items */
#define CANONNET	((u_char)0226)	/* canonical net, next token */
#define CANONHOST	((u_char)0227)	/* canonical host, next token */
#define CANONUSER	((u_char)0230)	/* canonical user, next N tokens */
#define CALLSUBR	((u_char)0231)	/* call another rewriting set */

/* conditionals in macros */
#define CONDIF		((u_char)0232)	/* conditional if-then */
#define CONDELSE	((u_char)0233)	/* conditional else */
#define CONDFI		((u_char)0234)	/* conditional fi */

/* bracket characters for host name lookup */
#define HOSTBEGIN	((u_char)0235)	/* hostname lookup begin */
#define HOSTEND	((u_char)0236)	/* hostname lookup end */

/* bracket characters for generalized lookup */
#define LOOKUPBEGIN	((u_char)0205)	/* generalized lookup begin */
#define LOOKUPEND	((u_char)0206)	/* generalized lookup end */

/* macro substitution character */
#define MACROEXPAND	((u_char)0201)	/* macro expansion */
#define MACRODEXPAND	((u_char)0202)	/* deferred macro expansion */

/* to make the code clearer */
#define MATCHZERO	CANONHOST

/* external <==> internal mapping table */
struct metamac
{
	char	metaname;	/* external code (after $) */
	u_char	metaval;	/* internal code (as above) */
};

/* values for macros with external names only */
#define MID_OPMODE	0202	/* operation mode */

/* functions */
extern void	define __P((int, char *, ENVELOPE *));
extern void	expand __P((char *, char *, size_t, ENVELOPE *));
extern int	macid __P((char *, char **));
extern char	*macname __P((int));
extern char	*macvalue __P((int, ENVELOPE *));
extern int	rscheck __P((char *, char *, char *, ENVELOPE *, bool, bool, int, char *));
extern void	setclass __P((int, char *));
extern int	strtorwset __P((char *, char **, int));
extern void	translate_dollars __P((char *));
extern bool	wordinclass __P((char *, int));

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

/* functions */
extern bool	getcanonname __P((char *, int, bool));
extern int	getmxrr __P((char *, char **, u_short *, bool, int *));

/*
**  Mapping functions
**
**	These allow arbitrary mappings in the config file.  The idea
**	(albeit not the implementation) comes from IDA sendmail.
*/

#define MAPCLASS	struct _mapclass
#define MAP		struct _map
#define MAXMAPACTIONS	5		/* size of map_actions array */


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
	char		map_spacesub;	/* spacesub */
	char		*map_app;	/* to append to successful matches */
	char		*map_tapp;	/* to append to "tempfail" matches */
	char		*map_domain;	/* the (nominal) NIS domain */
	char		*map_rebuild;	/* program to run to do auto-rebuild */
	time_t		map_mtime;	/* last database modification time */
	pid_t		map_pid;	/* PID of process which opened map */
	int		map_lockfd;	/* auxiliary lock file descriptor */
	short		map_specificity;	/* specificity of aliases */
	MAP		*map_stack[MAXMAPSTACK];   /* list for stacked maps */
	short		map_return[MAXMAPACTIONS]; /* return bitmaps for stacked maps */
};


/* bit values for map_mflags */
#define MF_VALID	0x00000001	/* this entry is valid */
#define MF_INCLNULL	0x00000002	/* include null byte in key */
#define MF_OPTIONAL	0x00000004	/* don't complain if map not found */
#define MF_NOFOLDCASE	0x00000008	/* don't fold case in keys */
#define MF_MATCHONLY	0x00000010	/* don't use the map value */
#define MF_OPEN		0x00000020	/* this entry is open */
#define MF_WRITABLE	0x00000040	/* open for writing */
#define MF_ALIAS	0x00000080	/* this is an alias file */
#define MF_TRY0NULL	0x00000100	/* try with no null byte */
#define MF_TRY1NULL	0x00000200	/* try with the null byte */
#define MF_LOCKED	0x00000400	/* this map is currently locked */
#define MF_ALIASWAIT	0x00000800	/* alias map in aliaswait state */
#define MF_IMPL_HASH	0x00001000	/* implicit: underlying hash database */
#define MF_IMPL_NDBM	0x00002000	/* implicit: underlying NDBM database */
#define MF_UNSAFEDB	0x00004000	/* this map is world writable */
#define MF_APPEND	0x00008000	/* append new entry on rebuild */
#define MF_KEEPQUOTES	0x00010000	/* don't dequote key before lookup */
#define MF_NODEFER	0x00020000	/* don't defer if map lookup fails */
#define MF_REGEX_NOT	0x00040000	/* regular expression negation */
#define MF_DEFER	0x00080000	/* don't lookup map in defer mode */
#define MF_SINGLEMATCH	0x00100000	/* successful only if match one key */
#define MF_NOREWRITE	0x00200000	/* don't rewrite result, return as-is */
#define MF_CLOSING	0x00400000	/* map is being closed */

#define DYNOPENMAP(map) if (!bitset(MF_OPEN, (map)->map_mflags)) \
	{	\
		if (!openmap(map))	\
			return NULL;	\
	}


/* indices for map_actions */
#define MA_NOTFOUND	0		/* member map returned "not found" */
#define MA_UNAVAIL	1		/* member map is not available */
#define MA_TRYAGAIN	2		/* member map returns temp failure */

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
extern void	closemaps __P((void));
extern bool	impl_map_open __P((MAP *, int));
extern void	initmaps __P((void));
extern MAP	*makemapentry __P((char *));
extern void	maplocaluser __P((ADDRESS *, ADDRESS **, int, ENVELOPE *));
extern char	*map_rewrite __P((MAP *, const char *, size_t, char **));
#if NETINFO
extern char	*ni_propval __P((char *, char *, char *, char *, int));
#endif /* NETINFO */
extern bool	openmap __P((MAP *));
#if USERDB
extern void	_udbx_close __P((void));
extern int	udbexpand __P((ADDRESS *, ADDRESS **, int, ENVELOPE *));
extern char	*udbsender __P((char *));
#endif /* USERDB */
/*
**  LDAP related items
*/
#ifdef LDAPMAP
struct ldapmap_struct
{
	/* needed for ldap_open or ldap_init */
	char		*ldap_host;
	int		ldap_port;

	/* options set in ld struct before ldap_bind_s */
	int		ldap_deref;
	time_t		ldap_timelimit;
	int		ldap_sizelimit;
	int		ldap_options;

	/* args for ldap_bind_s */
	LDAP		*ldap_ld;
	char		*ldap_binddn;
	char		*ldap_secret;
	int		ldap_method;

	/* args for ldap_search */
	char		*ldap_base;
	int		ldap_scope;
	char		*ldap_filter;
	char		*ldap_attr[LDAPMAP_MAX_ATTR + 1];
	bool		ldap_attrsonly;

	/* args for ldap_result */
	struct timeval	ldap_timeout;
	LDAPMessage	*ldap_res;

	/* Linked list of maps sharing the same LDAP binding */
	MAP		*ldap_next;
};

typedef struct ldapmap_struct	LDAPMAP_STRUCT;

/* struct defining LDAP Auth Methods */
struct lamvalues
{
	char	*lam_name;	/* name of LDAP auth method */
	int	lam_code;	/* numeric code */
};

/* struct defining LDAP Alias Dereferencing */
struct ladvalues
{
	char	*lad_name;	/* name of LDAP alias dereferencing method */
	int	lad_code;	/* numeric code */
};

/* struct defining LDAP Search Scope */
struct lssvalues
{
	char	*lss_name;	/* name of LDAP search scope */
	int	lss_code;	/* numeric code */
};

/* functions */
extern bool	ldapmap_parseargs __P((MAP *, char *));
extern void	ldapmap_set_defaults __P((char *));
#endif /* LDAPMAP */

/*
**  PH related items
*/

#ifdef PH_MAP
struct ph_map_struct
{
	char	*ph_servers;	/* list of ph servers */
	char	*ph_field_list;	/* list of fields to search for match */
	FILE	*ph_to_server;
	FILE	*ph_from_server;
	int	ph_sockfd;
	time_t	ph_timeout;
};
typedef struct ph_map_struct	PH_MAP_STRUCT;

# define DEFAULT_PH_MAP_FIELDS		"alias callsign name spacedname"
#endif /* PH_MAP */
/*
**  Process List (proclist)
*/

struct procs
{
	pid_t	proc_pid;
	char	*proc_task;
	int	proc_type;
};

#define NO_PID		((pid_t) 0)
#ifndef PROC_LIST_SEG
# define PROC_LIST_SEG	32		/* number of pids to alloc at a time */
#endif /* ! PROC_LIST_SEG */

/* process types */
#define PROC_NONE		0
#define PROC_DAEMON		1
#define PROC_DAEMON_CHILD	2
#define PROC_QUEUE		3
#define PROC_QUEUE_CHILD	3
#define PROC_CONTROL		4
#define PROC_CONTROL_CHILD	5

/* functions */
extern void	proc_list_add __P((pid_t, char *, int));
extern void	proc_list_clear __P((void));
extern void	proc_list_display __P((FILE *));
extern int	proc_list_drop __P((pid_t));
extern void	proc_list_probe __P((void));
extern void	proc_list_set __P((pid_t, char *));

/*
**  Symbol table definitions
*/

struct symtab
{
	char		*s_name;	/* name to be entered */
	short		s_type;		/* general type (see below) */
	short		s_len;		/* length of this entry */
	struct symtab	*s_next;	/* pointer to next in chain */
	union
	{
		BITMAP256	sv_class;	/* bit-map of word classes */
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
		struct hdrinfo	sv_header;	/* header metainfo */
		char		*sv_service[MAXMAPSTACK]; /* service switch */
#ifdef LDAPMAP
		MAP		*sv_lmap;	/* Maps for LDAP connection */
#endif /* LDAPMAP */
#if _FFR_MILTER
		struct milter	*sv_milter;	/* milter filter name */
#endif /* _FFR_MILTER */
	}	s_value;
};

typedef struct symtab	STAB;

/* symbol types */
#define ST_UNDEF	0	/* undefined type */
#define ST_CLASS	1	/* class map */
#define ST_ADDRESS	2	/* an address in parsed format */
#define ST_MAILER	3	/* a mailer header */
#define ST_ALIAS	4	/* an alias */
#define ST_MAPCLASS	5	/* mapping function class */
#define ST_MAP		6	/* mapping function */
#define ST_HOSTSIG	7	/* host signature */
#define ST_NAMECANON	8	/* cached canonical name */
#define ST_MACRO	9	/* macro name to id mapping */
#define ST_RULESET	10	/* ruleset index */
#define ST_SERVICE	11	/* service switch entry */
#define ST_HEADER	12	/* special header flags */
#ifdef LDAPMAP
# define ST_LMAP	13	/* List head of maps for LDAP connection */
#endif /* LDAPMAP */
#if _FFR_MILTER
# define ST_MILTER	14	/* milter filter */
#endif /* _FFR_MILTER */
#define ST_MCI		16	/* mailer connection info (offset) */

#define s_class		s_value.sv_class
#define s_address	s_value.sv_addr
#define s_mailer	s_value.sv_mailer
#define s_alias		s_value.sv_alias
#define s_mci		s_value.sv_mci
#define s_mapclass	s_value.sv_mapclass
#define s_hostsig	s_value.sv_hostsig
#define s_map		s_value.sv_map
#define s_namecanon	s_value.sv_namecanon
#define s_macro		s_value.sv_macro
#define s_ruleset	s_value.sv_ruleset
#define s_service	s_value.sv_service
#define s_header	s_value.sv_header
#ifdef LDAPMAP
# define s_lmap		s_value.sv_lmap
#endif /* LDAPMAP */
#if _FFR_MILTER
# define s_milter	s_value.sv_milter
#endif /* _FFR_MILTER */

/* opcodes to stab */
#define ST_FIND		0	/* find entry */
#define ST_ENTER	1	/* enter if not there */

/* functions */
extern STAB	*stab __P((char *, int, int));
extern void	stabapply __P((void (*)(STAB *, int), int));

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
	pid_t		ev_pid;		/* pid that set this event */
	struct event	*ev_link;	/* link to next item */
};

typedef struct event	EVENT;

/* functions */
extern void	clrevent __P((EVENT *));
extern void	clear_events __P((void));
extern EVENT	*setevent __P((time_t, void(*)(), int));
extern EVENT	*sigsafe_setevent __P((time_t, void(*)(), int));

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

#define MD_DELIVER	'm'		/* be a mail sender */
#define MD_SMTP		's'		/* run SMTP on standard input */
#define MD_ARPAFTP	'a'		/* obsolete ARPANET mode (Grey Book) */
#define MD_DAEMON	'd'		/* run as a daemon */
#define MD_FGDAEMON	'D'		/* run daemon in foreground */
#define MD_VERIFY	'v'		/* verify: don't collect or deliver */
#define MD_TEST		't'		/* test mode: resolve addrs only */
#define MD_INITALIAS	'i'		/* initialize alias database */
#define MD_PRINT	'p'		/* print the queue */
#define MD_FREEZE	'z'		/* freeze the configuration file */
#define MD_HOSTSTAT	'h'		/* print persistent host stat info */
#define MD_PURGESTAT	'H'		/* purge persistent host stat info */
#define MD_QUEUERUN	'q'		/* queue run */

/* values for e_sendmode -- send modes */
#define SM_DELIVER	'i'		/* interactive delivery */
#define SM_FORK		'b'		/* deliver in background */
#define SM_QUEUE	'q'		/* queue, don't deliver */
#define SM_DEFER	'd'		/* defer map lookups as well as queue */
#define SM_VERIFY	'v'		/* verify only (used internally) */


/* used only as a parameter to sendall */
#define SM_DEFAULT	'\0'		/* unspecified, use SendMode */

/* functions */
extern void	set_delivery_mode __P((int, ENVELOPE *));

/* values for e_errormode -- error handling modes */
#define EM_PRINT	'p'		/* print errors */
#define EM_MAIL		'm'		/* mail back errors */
#define EM_WRITE	'w'		/* write back errors */
#define EM_BERKNET	'e'		/* special berknet processing */
#define EM_QUIET	'q'		/* don't print messages (stat only) */


/* bit values for MimeMode */
#define MM_CVTMIME	0x0001		/* convert 8 to 7 bit MIME */
#define MM_PASS8BIT	0x0002		/* just send 8 bit data blind */
#define MM_MIME8BIT	0x0004		/* convert 8-bit data to MIME */


/* how to handle messages without any recipient addresses */
#define NRA_NO_ACTION		0	/* just leave it as is */
#define NRA_ADD_TO		1	/* add To: header */
#define NRA_ADD_APPARENTLY_TO	2	/* add Apparently-To: header */
#define NRA_ADD_BCC		3	/* add empty Bcc: header */
#define NRA_ADD_TO_UNDISCLOSED	4	/* add To: undisclosed:; header */


/* flags to putxline */
#define PXLF_NOTHINGSPECIAL	0	/* no special mapping */
#define PXLF_MAPFROM		0x0001	/* map From_ to >From_ */
#define PXLF_STRIP8BIT		0x0002	/* strip 8th bit */
#define PXLF_HEADER		0x0004	/* map newlines in headers */

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
#define PRIV_NOVERB		0x0100	/* disallow VERB command entirely */
#define PRIV_RESTRICTMAILQ	0x1000	/* restrict mailq command */
#define PRIV_RESTRICTQRUN	0x2000	/* restrict queue run */
#define PRIV_NOETRN		0x4000	/* disallow ETRN command entirely */
#define PRIV_NOBODYRETN		0x8000	/* do not return bodies on bounces */

/* don't give no info, anyway, anyhow */
#define PRIV_GOAWAY		(0x0fff & ~PRIV_NORECEIPTS)

/* struct defining such things */
struct prival
{
	char	*pv_name;	/* name of privacy flag */
	u_short	pv_flag;	/* numeric level */
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
**  Flags passed to mime8to7 and putheader.
*/

#define M87F_OUTER		0	/* outer context */
#define M87F_NO8BIT		0x0001	/* can't have 8-bit in this section */
#define M87F_DIGEST		0x0002	/* processing multipart/digest */
#define M87F_NO8TO7		0x0004	/* don't do 8->7 bit conversions */

/* functions */
extern void	mime7to8 __P((MCI *, HDR *, ENVELOPE *));
extern int	mime8to7 __P((MCI *, HDR *, ENVELOPE *, char **, int));

/*
**  Flags passed to returntosender.
*/

#define RTSF_NO_BODY		0	/* send headers only */
#define RTSF_SEND_BODY		0x0001	/* include body of message in return */
#define RTSF_PM_BOUNCE		0x0002	/* this is a postmaster bounce */

/* functions */
extern int	returntosender __P((char *, ADDRESS *, int, ENVELOPE *));

/*
**  Regular UNIX sockaddrs are too small to handle ISO addresses, so
**  we are forced to declare a supertype here.
*/

#if NETINET || NETINET6 || NETUNIX || NETISO || NETNS || NETX25
union bigsockaddr
{
	struct sockaddr		sa;	/* general version */
# if NETUNIX
	struct sockaddr_un	sunix;	/* UNIX family */
# endif /* NETUNIX */
# if NETINET
	struct sockaddr_in	sin;	/* INET family */
# endif /* NETINET */
# if NETINET6
	struct sockaddr_in6	sin6;	/* INET/IPv6 */
# endif /* NETINET6 */
# if NETISO
	struct sockaddr_iso	siso;	/* ISO family */
# endif /* NETISO */
# if NETNS
	struct sockaddr_ns	sns;	/* XNS family */
# endif /* NETNS */
# if NETX25
	struct sockaddr_x25	sx25;	/* X.25 family */
# endif /* NETX25 */
};

# define SOCKADDR	union bigsockaddr

/* functions */
extern char	*anynet_ntoa __P((SOCKADDR *));
# if NETINET6
extern char	*anynet_ntop __P((struct in6_addr *, char *, size_t));
# endif /* NETINET6 */
extern char	*hostnamebyanyaddr __P((SOCKADDR *));
# if DAEMON
extern char	*validate_connection __P((SOCKADDR *, char *, ENVELOPE *));
# endif /* DAEMON */

#endif /* NETINET || NETINET6 || NETUNIX || NETISO || NETNS || NETX25 */

#if _FFR_MILTER
/*
**  Mail Filters (milter)
*/

#include <libmilter/milter.h>

#define SMFTO_WRITE	0		/* Timeout for sending information */
#define SMFTO_READ	1		/* Timeout waiting for a response */
#define SMFTO_EOM	2		/* Timeout for ACK/NAK to EOM */

#define SMFTO_NUM_TO	3		/* Total number of timeouts */

struct milter
{
	char		*mf_name;	/* filter name */
	BITMAP256	mf_flags;	/* MTA flags */
	u_long		mf_fvers;	/* filter version */
	u_long		mf_fflags;	/* filter flags */
	u_long		mf_pflags;	/* protocol flags */
	char		*mf_conn;	/* connection info */
	int		mf_sock;	/* connected socket */
	char		mf_state;	/* state of filter */
	time_t		mf_timeout[SMFTO_NUM_TO]; /* timeouts */
};

/* MTA flags */
# define SMF_REJECT		'R'	/* Reject connection on filter fail */
# define SMF_TEMPFAIL		'T'	/* tempfail connection on failure */

/* states */
# define SMFS_CLOSED		'C'	/* closed for all further actions */
# define SMFS_OPEN		'O'	/* connected to remote milter filter */
# define SMFS_INMSG		'M'	/* currently servicing a message */
# define SMFS_DONE		'D'	/* done with current message */
# define SMFS_CLOSABLE		'Q'	/* done with current connection */
# define SMFS_ERROR		'E'	/* error state */
# define SMFS_READY		'R'	/* ready for action */

/* 32-bit type used by milter */
typedef SM_INT32	mi_int32;

EXTERN struct milter	*InputFilters[MAXFILTERS];
EXTERN char		*InputFilterList;
#endif /* _FFR_MILTER */

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
**	Vendors should apply to sendmail@sendmail.org for
**	unique vendor codes.
*/

#define VENDOR_BERKELEY	1	/* Berkeley-native configuration file */
#define VENDOR_SUN	2	/* Sun-native configuration file */
#define VENDOR_HP	3	/* Hewlett-Packard specific config syntax */
#define VENDOR_IBM	4	/* IBM specific config syntax */
#define VENDOR_SENDMAIL	5	/* Sendmail, Inc. specific config syntax */

/* prototypes for vendor-specific hook routines */
extern void	vendor_daemon_setup __P((ENVELOPE *));
extern void	vendor_set_uid __P((UID_T));


/*
**  Terminal escape codes.
**
**	To make debugging output clearer.
*/

struct termescape
{
	char	*te_rv_on;	/* turn reverse-video on */
	char	*te_rv_off;	/* turn reverse-video off */
};

/*
**  Additional definitions
*/

/* d_flags, see daemon.c */
/* general rule: lower case: required, upper case: No */
#define D_AUTHREQ	'a'	/* authentication required */
#define D_BINDIF	'b'	/* use if_addr for outgoing connection */
#define D_CANONREQ	'c'	/* canonification required (cf) */
#define D_IFNHELO	'h'	/* use if name for HELO */
#define D_FQMAIL	'f'	/* fq sender address required (cf) */
#if _FFR_TLS_CLT1
#define D_CLTNOTLS	'S'	/* don't use STARTTLS in client */
#endif /* _FFR_TLS_CLT1 */
#define D_FQRCPT	'r'	/* fq recipient address required (cf) */
#define D_UNQUALOK	'u'	/* unqualified address is ok (cf) */
#define D_NOCANON	'C'	/* no canonification (cf) */
#define D_NOETRN	'E'	/* no ETRN (MSA) */
#define D_ETRNONLY	((char)0x01)	/* allow only ETRN (disk low) */

/* Flags for submitmode */
#define SUBMIT_UNKNOWN	0x0000	/* unknown agent type */
#define SUBMIT_MTA	0x0001	/* act like a message transfer agent */
#define SUBMIT_MSA	0x0002	/* act like a message submission agent */

#if SASL
/*
**  SASL
*/

/* authenticated? */
# define SASL_NOT_AUTH	0		/* not authenticated */
# define SASL_PROC_AUTH	1		/* in process of authenticating */
# define SASL_IS_AUTH	2		/* authenticated */

/* SASL options */
# define SASL_AUTH_AUTH	0x1000		/* use auth= only if authenticated */
# if _FFR_SASL_OPTS
#  define SASL_SEC_MASK	0x0fff		/* mask for SASL_SEC_* values: sasl.h */
#  if (SASL_SEC_NOPLAINTEXT & SASL_SEC_MASK) == 0 || \
	(SASL_SEC_NOACTIVE & SASL_SEC_MASK) == 0 || \
	(SASL_SEC_NODICTIONARY & SASL_SEC_MASK) == 0 || \
	(SASL_SEC_FORWARD_SECRECY & SASL_SEC_MASK) == 0 || \
	(SASL_SEC_NOANONYMOUS & SASL_SEC_MASK) == 0 || \
	(SASL_SEC_PASS_CREDENTIALS & SASL_SEC_MASK) == 0
ERROR: change SASL_SEC_MASK_ notify sendmail.org!
#  endif
# endif /* _FFR_SASL_OPTS */

# define MAXOUTLEN 1024			/* length of output buffer */
#endif /* SASL */

#if STARTTLS
/*
**  TLS
*/

/* what to do in the TLS initialization */
#define TLS_I_NONE	0x00000000	/* no requirements... */
#define TLS_I_CERT_EX	0x00000001	/* CERT must exist */
#define TLS_I_CERT_UNR	0x00000002	/* CERT must be g/o unreadable */
#define TLS_I_KEY_EX	0x00000004	/* KEY must exist */
#define TLS_I_KEY_UNR	0x00000008	/* KEY must be g/o unreadable */
#define TLS_I_CERTP_EX	0x00000010	/* CA CERT PATH must exist */
#define TLS_I_CERTP_UNR	0x00000020	/* CA CERT PATH must be g/o unreadable */
#define TLS_I_CERTF_EX	0x00000040	/* CA CERT FILE must exist */
#define TLS_I_CERTF_UNR	0x00000080	/* CA CERT FILE must be g/o unreadable */
#define TLS_I_RSA_TMP	0x00000100	/* RSA TMP must be generated */
#define TLS_I_USE_KEY	0x00000200	/* private key must usable */
#define TLS_I_USE_CERT	0x00000400	/* certificate must be usable */
#define TLS_I_VRFY_PATH	0x00000800	/* load verify path must succeed */
#define TLS_I_VRFY_LOC	0x00001000	/* load verify default must succeed */
#define TLS_I_CACHE	0x00002000	/* require cache */
#define TLS_I_TRY_DH	0x00004000	/* try DH certificate */
#define TLS_I_REQ_DH	0x00008000	/* require DH certificate */
#define TLS_I_DHPAR_EX	0x00010000	/* require DH parameters */
#define TLS_I_DHPAR_UNR	0x00020000	/* DH param. must be g/o unreadable */
#define TLS_I_DH512	0x00040000	/* generate 512bit DH param */
#define TLS_I_DH1024	0x00080000	/* generate 1024bit DH param */
#define TLS_I_DH2048	0x00100000	/* generate 2048bit DH param */

/* server requirements */
#define TLS_I_SRV	(TLS_I_CERT_EX | TLS_I_KEY_EX | TLS_I_KEY_UNR | \
			 TLS_I_CERTP_EX | TLS_I_CERTF_EX | TLS_I_RSA_TMP | \
			 TLS_I_USE_KEY | TLS_I_USE_CERT | TLS_I_VRFY_PATH | \
			 TLS_I_VRFY_LOC | TLS_I_TRY_DH | \
			 TLS_I_DH512)

/* client requirements */
#define TLS_I_CLT	(TLS_I_KEY_UNR)

#define TLS_AUTH_OK	0
#define TLS_AUTH_NO	1
#define TLS_AUTH_FAIL	(-1)
#endif /* STARTTLS */


/*
**  Queue related items
*/

/* queue sort order */
#define QSO_BYPRIORITY	0		/* sort by message priority */
#define QSO_BYHOST	1		/* sort by first host name */
#define QSO_BYTIME	2		/* sort by submission time */
#define QSO_BYFILENAME	3		/* sort by file name only */

#if _FFR_QUEUEDELAY
#define QD_LINEAR	0		/* linear (old) delay alg */
#define QD_EXP		1		/* exponential delay alg */
#endif /* _FFR_QUEUEDELAY */

#define	NOQDIR	(-1)			/* no queue directory (yet) */

#define	NOW	((time_t) (-1))		/* queue return: now */

/* Queue Run Limitations */
struct queue_char
{
	char *queue_match;		/* string to match */
	struct queue_char *queue_next;
};

typedef struct queue_char QUEUE_CHAR;

/* functions */
extern void	assign_queueid __P((ENVELOPE *));
extern ADDRESS	*copyqueue __P((ADDRESS *));
extern void	initsys __P((ENVELOPE *));
extern void	loseqfile __P((ENVELOPE *, char *));
extern void	multiqueue_cache __P((void));
extern char	*qid_printname __P((ENVELOPE *));
extern char	*qid_printqueue __P((int));
extern char	*queuename __P((ENVELOPE *, int));
extern void	queueup __P((ENVELOPE *, bool));
extern bool	runqueue __P((bool, bool));
extern void	setnewqueue __P((ENVELOPE *));
extern bool	shouldqueue __P((long, time_t));
extern void	sync_queue_time __P((void));

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
	time_t	to_iconnect;	/* initial connection timeout (first try) */
	time_t	to_connect;	/* initial connection timeout (later tries) */
	time_t	to_rset;	/* RSET command */
	time_t	to_helo;	/* HELO command */
	time_t	to_quit;	/* QUIT command */
	time_t	to_miscshort;	/* misc short commands (NOOP, VERB, etc) */
	time_t	to_ident;	/* IDENT protocol requests */
	time_t	to_fileopen;	/* opening :include: and .forward files */
	time_t	to_control;	/* process a control socket command */
			/* following are per message */
	time_t	to_q_return[MAXTOCLASS];	/* queue return timeouts */
	time_t	to_q_warning[MAXTOCLASS];	/* queue warning timeouts */
	time_t	res_retrans[MAXRESTOTYPES];	/* resolver retransmit */
	int	res_retry[MAXRESTOTYPES];	/* resolver retry */
} TimeOuts;

/* timeout classes for return and warning timeouts */
#define TOC_NORMAL	0	/* normal delivery */
#define TOC_URGENT	1	/* urgent delivery */
#define TOC_NONURGENT	2	/* non-urgent delivery */

/* resolver timeout specifiers */
#define RES_TO_FIRST	0	/* first attempt */
#define RES_TO_NORMAL	1	/* subsequent attempts */
#define RES_TO_DEFAULT	2	/* default value */

/* functions */
extern void	inittimeouts __P((char *, bool));

/*
**  Trace information
*/

/* macros for debugging flags */
#define tTd(flag, level)	(tTdvect[flag] >= (u_char)level)
#define tTdlevel(flag)		(tTdvect[flag])

/* variables */
extern u_char	tTdvect[100];	/* trace vector */
/*
**  Critical signal sections
*/

#define PEND_SIGHUP	0x0001
#define PEND_SIGINT	0x0002
#define PEND_SIGTERM	0x0004
#define PEND_SIGUSR1	0x0008

#define ENTER_CRITICAL()	InCriticalSection++

#define LEAVE_CRITICAL()						\
do									\
{									\
	if (InCriticalSection > 0)					\
		InCriticalSection--;					\
} while (0)

#define CHECK_CRITICAL(sig)						\
{									\
	if (InCriticalSection > 0 && (sig) != 0)			\
	{								\
		pend_signal((sig));					\
		return SIGFUNC_RETURN;					\
	}								\
}

/* reset signal in case System V semantics */
#ifdef SYS5SIGNALS
# define FIX_SYSV_SIGNAL(sig, handler)					\
{									\
	if ((sig) != 0)							\
		(void) setsignal((sig), (handler));			\
}
#else /* SYS5SIGNALS */
# define FIX_SYSV_SIGNAL(sig, handler)	{ /* EMPTY */ }
#endif /* SYS5SIGNALS */

/* variables */
EXTERN u_int	volatile InCriticalSection;	/* >0 if in a critical section */
EXTERN int	volatile PendingSignal;	/* pending signal to resend */

/* functions */
extern void	pend_signal __P((int));

/*
**  Miscellaneous information.
*/

/*
**  The "no queue id" queue id for sm_syslog
*/

#define NOQID		"*~*"


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
**  Global variables.
*/

EXTERN bool	AllowBogusHELO;	/* allow syntax errors on HELO command */
#if !_FFR_REMOVE_AUTOREBUILD
EXTERN bool	AutoRebuild;	/* auto-rebuild the alias database as needed */
#endif /* !_FFR_REMOVE_AUTOREBUILD */
EXTERN bool	CheckAliases;	/* parse addresses during newaliases */
EXTERN bool	ChownAlwaysSafe;	/* treat chown(2) as safe */
EXTERN bool	ColonOkInAddr;	/* single colon legal in address */
EXTERN bool	ConfigFileRead;	/* configuration file has been read */
EXTERN bool	volatile DataProgress;	/* have we sent anything since last check */
EXTERN bool	DisConnected;	/* running with OutChannel redirected to xf */
EXTERN bool	volatile DoQueueRun;	/* non-interrupt time queue run needed */
EXTERN bool	DontExpandCnames;	/* do not $[...$] expand CNAMEs */
EXTERN bool	DontInitGroups;	/* avoid initgroups() because of NIS cost */
EXTERN bool	DontLockReadFiles;	/* don't read lock support files */
EXTERN bool	DontProbeInterfaces;	/* don't probe interfaces for names */
EXTERN bool	DontPruneRoutes;	/* don't prune source routes */
EXTERN bool	ForkQueueRuns;	/* fork for each job when running the queue */
EXTERN bool	FromFlag;	/* if set, "From" person is explicit */
EXTERN bool	GrabTo;		/* if set, get recipients from msg */
EXTERN bool	HasEightBits;	/* has at least one eight bit input byte */
EXTERN bool	HasWildcardMX;	/* don't use MX records when canonifying */
EXTERN bool	HoldErrs;	/* only output errors to transcript */
EXTERN bool	IgnoreHostStatus;	/* ignore long term host status files */
EXTERN bool	IgnrDot;	/* don't let dot end messages */
EXTERN bool	InChild;	/* true if running in an SMTP subprocess */
EXTERN bool	LogUsrErrs;	/* syslog user errors (e.g., SMTP RCPT cmd) */
EXTERN bool	MapOpenErr;	/* error opening a non-optional map */
EXTERN bool	MatchGecos;	/* look for user names in gecos field */
EXTERN bool	MeToo;		/* send to the sender also */
EXTERN bool	NoAlias;	/* suppress aliasing */
EXTERN bool	NoConnect;	/* don't connect to non-local mailers */
EXTERN bool	OnlyOneError;	/*  .... or only want to give one SMTP reply */
EXTERN bool	QuickAbort;	/*  .... but only if we want a quick abort */
EXTERN bool	RrtImpliesDsn;	/* turn Return-Receipt-To: into DSN */
EXTERN bool	SaveFrom;	/* save leading "From" lines */
EXTERN bool	SendMIMEErrors;	/* send error messages in MIME format */
EXTERN bool	SevenBitInput;	/* force 7-bit data on input */
EXTERN bool	SingleLineFromHeader;	/* force From: header to be one line */
EXTERN bool	SingleThreadDelivery;	/* single thread hosts on delivery */
EXTERN bool	volatile StopRequest;	/* stop sending output */
EXTERN bool	SuperSafe;	/* be extra careful, even if expensive */
EXTERN bool	SuprErrs;	/* set if we are suppressing errors */
EXTERN bool	TryNullMXList;	/* if we are the best MX, try host directly */
#if _FFR_WORKAROUND_BROKEN_NAMESERVERS
EXTERN bool	WorkAroundBrokenAAAA;	/* some nameservers return SERVFAIL on AAAA queries */
#endif /* _FFR_WORKAROUND_BROKEN_NAMESERVERS */
EXTERN bool	UseErrorsTo;	/* use Errors-To: header (back compat) */
EXTERN bool	UseHesiod;	/* using Hesiod -- interpret Hesiod errors */
EXTERN bool	UseNameServer;	/* using DNS -- interpret h_errno & MX RRs */
EXTERN char	InetMode;		/* default network for daemon mode */
EXTERN char	OpMode;		/* operation mode, see below */
EXTERN char	SpaceSub;	/* substitution for <lwsp> */
EXTERN int	CheckpointInterval;	/* queue file checkpoint interval */
EXTERN int	ConfigLevel;	/* config file level */
EXTERN int	ConnRateThrottle;	/* throttle for SMTP connection rate */
EXTERN int	volatile CurChildren;	/* current number of daemonic children */
EXTERN int	CurrentLA;	/* current load average */
EXTERN int	DefaultNotify;	/* default DSN notification flags */
EXTERN int	Errors;		/* set if errors (local to single pass) */
EXTERN int	ExitStat;	/* exit status code */
EXTERN int	FileMode;	/* mode on files */
EXTERN int	LineNumber;	/* line number in current input */
EXTERN int	LogLevel;	/* level of logging to perform */
EXTERN int	MaxAliasRecursion;	/* maximum depth of alias recursion */
EXTERN int	MaxChildren;	/* maximum number of daemonic children */
EXTERN int	MaxForwardEntries;	/* maximum number of forward entries */
EXTERN int	MaxHeadersLength;	/* max length of headers */
EXTERN int	MaxHopCount;	/* max # of hops until bounce */
EXTERN int	MaxMacroRecursion;	/* maximum depth of macro recursion */
EXTERN int	MaxMciCache;		/* maximum entries in MCI cache */
EXTERN int	MaxMimeFieldLength;	/* maximum MIME field length */
EXTERN int	MaxMimeHeaderLength;	/* maximum MIME header length */


EXTERN int	MaxQueueRun;	/* maximum number of jobs in one queue run */
EXTERN int	MaxRcptPerMsg;	/* max recipients per SMTP message */
EXTERN int	MaxRuleRecursion;	/* maximum depth of ruleset recursion */
EXTERN int	MimeMode;	/* MIME processing mode */
EXTERN int	NoRecipientAction;
EXTERN int	NumPriorities;	/* pointer into Priorities */
EXTERN u_short	PrivacyFlags;	/* privacy flags */
#if _FFR_QUEUE_FILE_MODE
EXTERN int	QueueFileMode;	/* mode on qf/tf/df files */
#endif /* _FFR_QUEUE_FILE_MODE */
EXTERN int	QueueLA;	/* load average starting forced queueing */
EXTERN int	QueueSortOrder;	/* queue sorting order algorithm */
EXTERN int	RefuseLA;	/* load average refusing connections are */
EXTERN int	VendorCode;	/* vendor-specific operation enhancements */
EXTERN int	Verbose;	/* set if blow-by-blow desired */
EXTERN gid_t	DefGid;		/* default gid to run as */
EXTERN gid_t	RealGid;	/* real gid of caller */
EXTERN gid_t	RunAsGid;	/* GID to become for bulk of run */
EXTERN uid_t	DefUid;		/* default uid to run as */
EXTERN uid_t	RealUid;	/* real uid of caller */
EXTERN uid_t	RunAsUid;	/* UID to become for bulk of run */
EXTERN uid_t	TrustedUid;	/* uid of trusted user for files and startup */
EXTERN size_t	DataFileBufferSize;	/* size of buffer for in-core df */
EXTERN size_t	XscriptFileBufferSize;	/* size of buffer for in-core xf */
EXTERN time_t	DialDelay;	/* delay between dial-on-demand tries */
EXTERN time_t	MciCacheTimeout;	/* maximum idle time on connections */
EXTERN time_t	MciInfoTimeout;		/* how long 'til we retry down hosts */
EXTERN time_t	MinQueueAge;	/* min delivery interval */
EXTERN time_t	QueueIntvl;	/* intervals between running the queue */
EXTERN time_t	SafeAlias;	/* interval to wait until @:@ in alias file */
EXTERN time_t	ServiceCacheMaxAge;	/* refresh interval for cache */
EXTERN time_t	ServiceCacheTime;	/* time service switch was cached */
EXTERN MODE_T	OldUmask;	/* umask when sendmail starts up */
EXTERN long	MaxMessageSize;	/* advertised max size we will accept */
EXTERN long	MinBlocksFree;	/* min # of blocks free on queue fs */
EXTERN long	QueueFactor;	/* slope of queue function */
EXTERN long	WkClassFact;	/* multiplier for message class -> priority */
EXTERN long	WkRecipFact;	/* multiplier for # of recipients -> priority */
EXTERN long	WkTimeFact;	/* priority offset each time this job is run */
#if SASL
EXTERN char	*AuthMechanisms;	/* AUTH mechanisms */
EXTERN char	*SASLInfo;		/* file with AUTH info */
#endif /* SASL */
EXTERN int	SASLOpts;		/* options for SASL */
#if STARTTLS
EXTERN char	*CACERTpath;	/* path to CA certificates (dir. with hashes) */
EXTERN char	*CACERTfile;	/* file with CA certificate */
EXTERN char	*SrvCERTfile;	/* file with server certificate */
EXTERN char	*Srvkeyfile;	/* file with server private key */
EXTERN char	*CltCERTfile;	/* file with client certificate */
EXTERN char	*Cltkeyfile;	/* file with client private key */
EXTERN char	*DHParams;	/* file with DH parameters */
EXTERN char	*RandFile;	/* source of random data */
# if _FFR_TLS_1
EXTERN char	*DHParams5;	/* file with DH parameters (512) */
EXTERN char	*CipherList;	/* list of ciphers */
# endif /* _FFR_TLS_1 */
#endif /* STARTTLS */
EXTERN char	*ConfFile;	/* location of configuration file [conf.c] */
EXTERN char	*ControlSocketName; /* control socket filename [control.c] */
EXTERN char	*CurHostName;	/* current host we are dealing with */
EXTERN char	*DeadLetterDrop;	/* path to dead letter office */
EXTERN char	*DefUser;	/* default user to run as (from DefUid) */
EXTERN char	*DefaultCharSet;	/* default character set for MIME */
EXTERN char	*DoubleBounceAddr;	/* where to send double bounces */
EXTERN char	*ErrMsgFile;	/* file to prepend to all error messages */
EXTERN char	*FallBackMX;	/* fall back MX host */
EXTERN char	*FileName;	/* name to print on error messages */
EXTERN char	*ForwardPath;	/* path to search for .forward files */
EXTERN char	*HelpFile;	/* location of SMTP help file */
EXTERN char	*HostStatDir;	/* location of host status information */
EXTERN char	*HostsFile;	/* path to /etc/hosts file */
EXTERN char	*MustQuoteChars;	/* quote these characters in phrases */
EXTERN char	*MyHostName;	/* name of this host for SMTP messages */
EXTERN char	*OperatorChars;	/* operators (old $o macro) */
EXTERN char	*PidFile;	/* location of proc id file [conf.c] */
EXTERN char	*PostMasterCopy;	/* address to get errs cc's */
EXTERN char	*ProcTitlePrefix; /* process title prefix */
EXTERN char	*QueueDir;	/* location of queue directory */
#if _FFR_QUEUEDELAY
EXTERN int	QueueAlg;	/* algorithm for queue delays */
EXTERN time_t	QueueInitDelay;	/* initial queue delay */
EXTERN time_t	QueueMaxDelay;	/* maximum queue delay */
#endif /* _FFR_QUEUEDELAY */
EXTERN char	*RealHostName;	/* name of host we are talking to */
EXTERN char	*RealUserName;	/* real user name of caller */
EXTERN char	*volatile RestartRequest;/* a sendmail restart has been requested */
EXTERN char	*RunAsUserName;	/* user to become for bulk of run */
EXTERN char	*SafeFileEnv;	/* chroot location for file delivery */
EXTERN char	*ServiceSwitchFile;	/* backup service switch */
EXTERN char	*volatile ShutdownRequest;/* a sendmail shutdown has been requested */
EXTERN char	*SmtpGreeting;	/* SMTP greeting message (old $e macro) */
EXTERN char	*SmtpPhase;	/* current phase in SMTP processing */
EXTERN char	SmtpError[MAXLINE];	/* save failure error messages */
EXTERN char	*StatFile;	/* location of statistics summary */
EXTERN char	*TimeZoneSpec;	/* override time zone specification */
EXTERN char	*UdbSpec;	/* user database source spec */
EXTERN char	*UnixFromLine;	/* UNIX From_ line (old $l macro) */
EXTERN char	**ExternalEnviron;	/* input environment */
					/* saved user environment */
EXTERN char	**SaveArgv;	/* argument vector for re-execing */
EXTERN BITMAP256	DontBlameSendmail;	/* DontBlameSendmail bits */
#if SFIO
EXTERN Sfio_t	*InChannel;	/* input connection */
EXTERN Sfio_t	*OutChannel;	/* output connection */
#else /* SFIO */
EXTERN FILE	*InChannel;	/* input connection */
EXTERN FILE	*OutChannel;	/* output connection */
#endif /* SFIO */
EXTERN FILE	*TrafficLogFile;	/* file in which to log all traffic */
#ifdef HESIOD
EXTERN void	*HesiodContext;
#endif /* HESIOD */
EXTERN ENVELOPE	*CurEnv;	/* envelope currently being processed */
EXTERN MAILER	*LocalMailer;	/* ptr to local mailer */
EXTERN MAILER	*ProgMailer;	/* ptr to program mailer */
EXTERN MAILER	*FileMailer;	/* ptr to *file* mailer */
EXTERN MAILER	*InclMailer;	/* ptr to *include* mailer */
EXTERN QUEUE_CHAR	*QueueLimitRecipient;	/* limit queue run to rcpt */
EXTERN QUEUE_CHAR	*QueueLimitSender;	/* limit queue run to sender */
EXTERN QUEUE_CHAR	*QueueLimitId;		/* limit queue run to id */
EXTERN MAILER	*Mailer[MAXMAILERS + 1];
EXTERN struct rewrite	*RewriteRules[MAXRWSETS];
EXTERN char	*RuleSetNames[MAXRWSETS];	/* ruleset number to name */
EXTERN char	*UserEnviron[MAXUSERENVIRON + 1];
EXTERN struct priority	Priorities[MAXPRIORITIES];
EXTERN struct termescape	TermEscape;	/* terminal escape codes */
EXTERN SOCKADDR	ConnectOnlyTo;	/* override connection address (for testing) */
EXTERN SOCKADDR RealHostAddr;	/* address of host we are talking to */
EXTERN jmp_buf	TopFrame;	/* branch-to-top-of-loop-on-error frame */
EXTERN TIMERS	Timers;

/*
**  Declarations of useful functions
*/

#if SASL
extern char	*intersect __P((char *, char *));
extern char	*iteminlist __P((char *, char *, char *));
extern int	proxy_policy __P((void *, const char *, const char *, const char **, const char **));
# if SASL > 10515
extern int	safesaslfile __P((void *, char *, int));
# else /* SASL > 10515 */
extern int	safesaslfile __P((void *, char *));
# endif /* SASL > 10515 */
extern int	sasl_decode64 __P((const char *, unsigned, char *, unsigned *));
extern int	sasl_encode64 __P((const char *, unsigned, char *, unsigned, unsigned *));
#endif /* SASL */

#if STARTTLS
extern void	apps_ssl_info_cb __P((SSL *, int , int));
extern bool	init_tls_library __P((void));
extern bool	inittls __P((SSL_CTX **, u_long, bool, char *, char *, char *, char *, char *));
extern bool	initclttls __P((void));
extern bool	initsrvtls __P((void));
extern int	tls_get_info __P((SSL *, ENVELOPE *, bool, char *, bool));
extern int	endtls __P((SSL *, char *));
extern int	endtlsclt __P((MCI *));
extern void	tlslogerr __P((void));
extern bool	tls_rand_init __P((char *, int));
#endif /* STARTTLS */

/* Transcript file */
extern void	closexscript __P((ENVELOPE *));
extern void	openxscript __P((ENVELOPE *));

/* error related */
extern void	buffer_errors __P((void));
extern void	flush_errors __P((bool));
extern void	message __P((const char *, ...));
extern void	nmessage __P((const char *, ...));
extern void	syserr __P((const char *, ...));
extern void	usrerrenh __P((char *, const char *, ...));
extern void	usrerr __P((const char *, ...));
extern int	isenhsc __P((const char *, int));
extern int	extenhsc __P((const char *, int, char *));

/* alias file */
extern void	alias __P((ADDRESS *, ADDRESS **, int, ENVELOPE *));
extern bool	aliaswait __P((MAP *, char *, bool));
extern void	forward __P((ADDRESS *, ADDRESS **, int, ENVELOPE *));
extern void	readaliases __P((MAP *, FILE *, bool, bool));
extern bool	rebuildaliases __P((MAP *, bool));
extern void	setalias __P((char *));

/* logging */
extern void	logdelivery __P((MAILER *, MCI *, char *, const char *, ADDRESS *, time_t, ENVELOPE *));
extern void	logsender __P((ENVELOPE *, char *));
extern void	sm_syslog __P((int, const char *, const char *, ...));

/* SMTP */
extern void	giveresponse __P((int, char *, MAILER *, MCI *, ADDRESS *, time_t, ENVELOPE *));
extern int	reply __P((MAILER *, MCI *, ENVELOPE *, time_t, void (*)(), char **));
extern void	smtp __P((char *volatile, BITMAP256, ENVELOPE *volatile));
#if SASL
extern int	smtpauth __P((MAILER *, MCI *, ENVELOPE *));
#endif /* SASL */
extern int	smtpdata __P((MAILER *, MCI *, ENVELOPE *));
extern int	smtpgetstat __P((MAILER *, MCI *, ENVELOPE *));
extern int	smtpmailfrom __P((MAILER *, MCI *, ENVELOPE *));
extern void	smtpmessage __P((char *, MAILER *, MCI *, ...));
extern void	smtpinit __P((MAILER *, MCI *, ENVELOPE *, bool));
extern char	*smtptodsn __P((int));
extern int	smtpprobe __P((MCI *));
extern void	smtpquit __P((MAILER *, MCI *, ENVELOPE *));
extern int	smtprcpt __P((ADDRESS *, MAILER *, MCI *, ENVELOPE *));
extern void	smtprset __P((MAILER *, MCI *, ENVELOPE *));

#define ISSMTPCODE(c)	(isascii(c[0]) && isdigit(c[0]) && \
		    isascii(c[1]) && isdigit(c[1]) && \
		    isascii(c[2]) && isdigit(c[2]))
#define ISSMTPREPLY(c)	(ISSMTPCODE(c) && \
		    (c[3] == ' ' || c[3] == '-' || c[3] == '\0'))

/* delivery */
extern pid_t	dowork __P((int, char *, bool, bool, ENVELOPE *));
extern int	endmailer __P((MCI *, ENVELOPE *, char **));
extern int	mailfile __P((char *volatile, MAILER *volatile, ADDRESS *, volatile long, ENVELOPE *));
extern void	sendall __P((ENVELOPE *, int));

/* stats */
extern void	markstats __P((ENVELOPE *, ADDRESS *, bool));
extern void	clearstats __P((void));
extern void	poststats __P((char *));

/* control socket */
extern void	closecontrolsocket  __P((bool));
extern void	clrcontrol  __P((void));
extern void	control_command __P((int, ENVELOPE *));
extern int	opencontrolsocket __P((void));

#if _FFR_MILTER
/* milter functions */
extern void	milter_parse_list __P((char *, struct milter **, int));
extern void	milter_setup __P((char *));
extern void	milter_set_option __P((char *, char *, bool));
extern bool	milter_can_delrcpts __P((void));
extern void	milter_init __P((ENVELOPE *, char *));
extern void	milter_quit __P((ENVELOPE *));
extern void	milter_abort __P((ENVELOPE *));
extern char	*milter_connect __P((char *, SOCKADDR, ENVELOPE *, char *));
extern char	*milter_helo __P((char *, ENVELOPE *, char *));
extern char	*milter_envfrom __P((char **, ENVELOPE *, char *));
extern char	*milter_envrcpt __P((char **, ENVELOPE *, char *));
extern char	*milter_data __P((ENVELOPE *, char *));
#endif /* _FFR_MILTER */

extern char	*addquotes __P((char *));
extern void	allsignals __P((bool));
extern char	*arpadate __P((char *));
extern bool	atobool __P((char *));
extern int	atooct __P((char *));
extern void	auth_warning __P((ENVELOPE *, const char *, ...));
extern int	blocksignal __P((int));
extern bool	bitintersect __P((BITMAP256, BITMAP256));
extern bool	bitzerop __P((BITMAP256));
extern void	buildfname __P((char *, char *, char *, int));
extern int	checkcompat __P((ADDRESS *, ENVELOPE *));
#ifdef XDEBUG
extern void	checkfd012 __P((char *));
extern void	checkfdopen __P((int, char *));
#endif /* XDEBUG */
extern void	checkfds __P((char *));
extern bool	chownsafe __P((int, bool));
extern void	cleanstrcpy __P((char *, char *, int));
extern void	clrdaemon __P((void));
extern void	collect __P((FILE *, bool, HDR **, ENVELOPE *));
extern time_t	convtime __P((char *, int));
extern char	**copyplist __P((char **, bool));
extern void	copy_class __P((int, int));
extern time_t	curtime __P((void));
extern char	*defcharset __P((ENVELOPE *));
extern char	*denlstring __P((char *, bool, bool));
extern void	disconnect __P((int, ENVELOPE *));
extern bool	dns_getcanonname __P((char *, int, bool, int *));
extern pid_t	dofork __P((void));
extern int	drop_privileges __P((bool));
extern int	dsntoexitstat __P((char *));
extern void	dumpfd __P((int, bool, bool));
extern void	dumpstate __P((char *));
extern bool	enoughdiskspace __P((long, bool));
extern char	*exitstat __P((char *));
extern char	*fgetfolded __P((char *, int, FILE *));
extern void	fill_fd __P((int, char *));
extern char	*find_character __P((char *, int));
extern struct passwd	*finduser __P((char *, bool *));
extern void	finis __P((bool, volatile int));
extern void	fixcrlf __P((char *, bool));
extern long	freediskspace __P((char *, long *));
#if NETINET6 && NEEDSGETIPNODE
# if _FFR_FREEHOSTENT
extern void	freehostent __P((struct hostent *));
# endif /* _FFR_FREEHOSTENT */
#endif /* NEEDSGETIPNODE && NETINET6 */
extern char	*get_column __P((char *, int, int, char *, int));
extern char	*getauthinfo __P((int, bool *));
extern char	*getcfname __P((void));
extern char	*getextenv __P((const char *));
extern int	getdtsize __P((void));
extern BITMAP256	*getrequests __P((ENVELOPE *));
extern char	*getvendor __P((int));
extern void	help __P((char *, ENVELOPE *));
extern void	init_md __P((int, char **));
extern void	initdaemon __P((void));
extern void	inithostmaps __P((void));
extern void	initmacros __P((ENVELOPE *));
extern void	initsetproctitle __P((int, char **, char **));
extern void	init_vendor_macros __P((ENVELOPE *));
extern SIGFUNC_DECL	intsig __P((int));
extern bool	isloopback __P((SOCKADDR sa));
extern void	load_if_names __P((void));
extern bool	lockfile __P((int, char *, char *, int));
extern void	log_sendmail_pid __P((ENVELOPE *));
extern char	lower __P((int));
extern void	makelower __P((char *));
extern int	makeconnection_ds __P((char *, MCI *));
extern int	makeconnection __P((char *, volatile u_int, MCI *, ENVELOPE *));
extern char *	munchstring __P((char *, char **, int));
extern struct hostent	*myhostname __P((char *, int));
extern char	*nisplus_default_domain __P((void));	/* extern for Sun */
extern bool	path_is_dir __P((char *, bool));
extern char	*pintvl __P((time_t, bool));
extern void	printav __P((char **));
extern void	printmailer __P((MAILER *));
extern void	printopenfds __P((bool));
extern void	printqueue __P((void));
extern void	printrules __P((void));
extern pid_t	prog_open __P((char **, int *, ENVELOPE *));
extern void	putline __P((char *, MCI *));
extern void	putxline __P((char *, size_t, MCI *, int));
extern void	queueup_macros __P((int, FILE *, ENVELOPE *));
extern void	readcf __P((char *, bool, ENVELOPE *));
extern SIGFUNC_DECL	reapchild __P((int));
extern int	releasesignal __P((int));
extern void	resetlimits __P((void));
extern bool	rfc822_string __P((char *));
extern void	savemail __P((ENVELOPE *, bool));
extern void	seed_random __P((void));
extern void	sendtoargv __P((char **, ENVELOPE *));
extern void	setclientoptions __P((char *));
extern bool	setdaemonoptions __P((char *));
extern void	setdefaults __P((ENVELOPE *));
extern void	setdefuser __P((void));
extern bool	setvendor __P((char *));
extern void	setoption __P((int, char *, bool, bool, ENVELOPE *));
extern sigfunc_t	setsignal __P((int, sigfunc_t));
extern void	setuserenv __P((const char *, const char *));
extern void	settime __P((ENVELOPE *));
extern char	*sfgets __P((char *, int, FILE *, time_t, char *));
extern char	*shortenstring __P((const char *, int));
extern char	*shorten_hostname __P((char []));
extern bool	shorten_rfc822_string __P((char *, size_t));
extern void	shutdown_daemon __P((void));
extern void	sm_dopr __P((char *, const char *, va_list));
extern void	sm_free __P((void *));
extern struct hostent	*sm_gethostbyname __P((char *, int));
extern struct hostent	*sm_gethostbyaddr __P((char *, int, int));
extern int	sm_getla __P((ENVELOPE *));
extern struct passwd	*sm_getpwnam __P((char *));
extern struct passwd	*sm_getpwuid __P((UID_T));
extern void	sm_setproctitle __P((bool, ENVELOPE *, const char *, ...));
extern int	sm_strcasecmp __P((const char *, const char *));
extern void	stop_sendmail __P((void));
extern bool	strcontainedin __P((char *, char *));
extern void	stripquotes __P((char *));
extern int	switch_map_find __P((char *, char *[], short []));
extern bool	transienterror __P((int));
extern void	tTflag __P((char *));
extern void	tTsetup __P((u_char *, int, char *));
extern char	*ttypath __P((void));
extern void	unlockqueue __P((ENVELOPE *));
#if !HASUNSETENV
extern void	unsetenv __P((char *));
#endif /* !HASUNSETENV */
extern char	*username __P((void));
extern bool	usershellok __P((char *, char *));
extern void	vendor_post_defaults __P((ENVELOPE *));
extern void	vendor_pre_defaults __P((ENVELOPE *));
extern int	waitfor __P((pid_t));
extern bool	writable __P((char *, ADDRESS *, long));
extern char	*xalloc __P((int));
extern char	*xcalloc __P((size_t, size_t));
extern char	*xrealloc __P((void *, size_t));
extern void	xputs __P((const char *));
extern char	*xtextify __P((char *, char *));
extern bool	xtextok __P((char *));
extern void	xunlink __P((char *));
extern char	*xuntextify __P((char *));
#endif /* _SENDMAIL_H */
