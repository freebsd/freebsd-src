/*
 * Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *
 *	$Id: sendmail.h,v 8.34.4.8 2001/06/01 05:06:51 gshapiro Exp $
 */

/*
**  SENDMAIL.H -- Global definitions for sendmail.
*/

#if SFIO
# include <sfio/stdio.h>
#else /* SFIO */
# include <stdio.h>
#endif /* SFIO */
#include <string.h>
#include "conf.h"
#include "sendmail/errstring.h"
#include "sendmail/useful.h"


/**********************************************************************
**  Table sizes, etc....
**	There shouldn't be much need to change these....
**********************************************************************/
#ifndef MAXMAILERS
# define MAXMAILERS	25	/* maximum mailers known to system */
#endif /* ! MAXMAILERS */

/*
**  Data structure for bit maps.
**
**	Each bit in this map can be referenced by an ascii character.
**	This is 256 possible bits, or 32 8-bit bytes.
*/

#define BITMAPBITS	256	/* number of bits in a bit map */
#define BYTEBITS	8	/* number of bits in a byte */
#define BITMAPBYTES	(BITMAPBITS / BYTEBITS)	/* number of bytes in bit map */

/* internal macros */
#define _BITWORD(bit)	((bit) / (BYTEBITS * sizeof (int)))
#define _BITBIT(bit)	((unsigned int)1 << ((bit) % (BYTEBITS * sizeof (int))))

typedef unsigned int	BITMAP256[BITMAPBYTES / sizeof (int)];

/* properly case and truncate bit */
#define bitidx(bit)		((unsigned int) (bit) & 0xff)

/* test bit number N */
#define bitnset(bit, map)	((map)[_BITWORD(bit)] & _BITBIT(bit))

/* set bit number N */
#define setbitn(bit, map)	(map)[_BITWORD(bit)] |= _BITBIT(bit)

/* clear bit number N */
#define clrbitn(bit, map)	(map)[_BITWORD(bit)] &= ~_BITBIT(bit)

/* clear an entire bit map */
#define clrbitmap(map)		memset((char *) map, '\0', BITMAPBYTES)


/*
**  Utility macros
*/

/* return number of bytes left in a buffer */
#define SPACELEFT(buf, ptr)	(sizeof buf - ((ptr) - buf))
/*
**  Flags passed to safefile/safedirpath.
*/

#define SFF_ANYFILE	0L		/* no special restrictions */
#define SFF_MUSTOWN	0x00000001L	/* user must own this file */
#define SFF_NOSLINK	0x00000002L	/* file cannot be a symbolic link */
#define SFF_ROOTOK	0x00000004L	/* ok for root to own this file */
#define SFF_RUNASREALUID 0x00000008L	/* if no ctladdr, run as real uid */
#define SFF_NOPATHCHECK	0x00000010L	/* don't bother checking dir path */
#define SFF_SETUIDOK	0x00000020L	/* setuid files are ok */
#define SFF_CREAT	0x00000040L	/* ok to create file if necessary */
#define SFF_REGONLY	0x00000080L	/* regular files only */
#define SFF_SAFEDIRPATH	0x00000100L	/* no writable directories allowed */
#define SFF_NOHLINK	0x00000200L	/* file cannot have hard links */
#define SFF_NOWLINK	0x00000400L	/* links only in non-writable dirs */
#define SFF_NOGWFILES	0x00000800L	/* disallow world writable files */
#define SFF_NOWWFILES	0x00001000L	/* disallow group writable files */
#define SFF_OPENASROOT	0x00002000L	/* open as root instead of real user */
#define SFF_NOLOCK	0x00004000L	/* don't lock the file */
#define SFF_NOGRFILES	0x00008000L	/* disallow g readable files */
#define SFF_NOWRFILES	0x00010000L	/* disallow o readable files */
#define SFF_NOTEXCL	0x00020000L	/* creates don't need to be exclusive */
#define SFF_EXECOK	0x00040000L	/* executable files are ok (E_SM_ISEXEC) */
#define SFF_NORFILES	(SFF_NOGRFILES|SFF_NOWRFILES)

/* pseudo-flags */
#define SFF_NOLINK	(SFF_NOHLINK|SFF_NOSLINK)

/* functions */
extern int	safefile __P((char *, UID_T, GID_T, char *, long, int, struct stat *));
extern int	safedirpath __P((char *, UID_T, GID_T, char *, long, int, int));
extern int	safeopen __P((char *, int, int, long));
extern int	dfopen __P((char *, int, int, long));
extern bool	filechanged __P((char *, int, struct stat *));

/*
**  DontBlameSendmail options
**
**	Hopefully nobody uses these.
*/
#define DBS_SAFE					0
#define DBS_ASSUMESAFECHOWN				1
#define DBS_GROUPWRITABLEDIRPATHSAFE			2
#define DBS_GROUPWRITABLEFORWARDFILESAFE		3
#define DBS_GROUPWRITABLEINCLUDEFILESAFE		4
#define DBS_GROUPWRITABLEALIASFILE			5
#define DBS_WORLDWRITABLEALIASFILE			6
#define DBS_FORWARDFILEINUNSAFEDIRPATH			7
#define DBS_MAPINUNSAFEDIRPATH				8
#define DBS_LINKEDALIASFILEINWRITABLEDIR		9
#define DBS_LINKEDCLASSFILEINWRITABLEDIR		10
#define DBS_LINKEDFORWARDFILEINWRITABLEDIR		11
#define DBS_LINKEDINCLUDEFILEINWRITABLEDIR		12
#define DBS_LINKEDMAPINWRITABLEDIR			13
#define DBS_LINKEDSERVICESWITCHFILEINWRITABLEDIR	14
#define DBS_FILEDELIVERYTOHARDLINK			15
#define DBS_FILEDELIVERYTOSYMLINK			16
#define DBS_WRITEMAPTOHARDLINK				17
#define DBS_WRITEMAPTOSYMLINK				18
#define DBS_WRITESTATSTOHARDLINK			19
#define DBS_WRITESTATSTOSYMLINK				20
#define DBS_FORWARDFILEINGROUPWRITABLEDIRPATH		21
#define DBS_INCLUDEFILEINGROUPWRITABLEDIRPATH		22
#define DBS_CLASSFILEINUNSAFEDIRPATH			23
#define DBS_ERRORHEADERINUNSAFEDIRPATH			24
#define DBS_HELPFILEINUNSAFEDIRPATH			25
#define DBS_FORWARDFILEINUNSAFEDIRPATHSAFE		26
#define DBS_INCLUDEFILEINUNSAFEDIRPATHSAFE		27
#define DBS_RUNPROGRAMINUNSAFEDIRPATH			28 /* Not used yet */
#define DBS_RUNWRITABLEPROGRAM				29
#define DBS_INCLUDEFILEINUNSAFEDIRPATH			30
#define DBS_NONROOTSAFEADDR				31
#define DBS_TRUSTSTICKYBIT				32
#define DBS_DONTWARNFORWARDFILEINUNSAFEDIRPATH		33
#define DBS_INSUFFICIENTENTROPY				34
#if _FFR_UNSAFE_SASL
# define DBS_GROUPREADABLESASLFILE			35
#endif /* _FFR_UNSAFE_SASL */
#if _FFR_UNSAFE_WRITABLE_INCLUDE
# define DBS_GROUPWRITABLEFORWARDFILE			36
# define DBS_GROUPWRITABLEINCLUDEFILE			37
# define DBS_WORLDWRITABLEFORWARDFILE			38
# define DBS_WORLDWRITABLEINCLUDEFILE			39
#endif /* _FFR_UNSAFE_WRITABLE_INCLUDE */

/* struct defining such things */
struct dbsval
{
	char	*dbs_name;	/* name of DontBlameSendmail flag */
	u_char	dbs_flag;	/* numeric level */
};

#if _FFR_DPRINTF
extern void	dprintf __P((const char *, ...));
extern int	dflush __P((void));
#else /* _FFR_DPRINTF */
#define dprintf		printf
#define dflush()	fflush(stdout)
#endif /* _FFR_DPRINTF */

extern int	sm_snprintf __P((char *, size_t, const char *, ...));
extern int	sm_vsnprintf __P((char *, size_t, const char *, va_list));
extern char	*quad_to_string __P((QUAD_T));

extern size_t	strlcpy __P((char *, const char *, size_t));
extern size_t	strlcat __P((char *, const char *, size_t));

