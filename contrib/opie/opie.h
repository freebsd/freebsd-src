/* opie.h: Data structures and values for the OPIE authentication
	system that a program might need.

%%% portions-copyright-cmetz
Portions of this software are Copyright 1996 by Craig Metz, All Rights
Reserved. The Inner Net License Version 2 applies to these portions of
the software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

Portions of this software are Copyright 1995 by Randall Atkinson and Dan
McDonald, All Rights Reserved. All Rights under this copyright are assigned
to the U.S. Naval Research Laboratory (NRL). The NRL Copyright Notice and
License Agreement applies to this software.

	History:

	Modified by cmetz for OPIE 2.3. Renamed PTR to VOIDPTR. Added
		re-init key and extension file fields to struct opie. Added
		opie_ prefix on struct opie members. Added opie_flags field
		and definitions. Added more prototypes. Changed opiehash()
		prototype.
	Modified by cmetz for OPIE 2.22. Define __P correctly if this file
		is included in a third-party program.
	Modified by cmetz for OPIE 2.2. Re-did prototypes. Added FUNCTION
                definition et al. Multiple-include protection. Added struct
		utsname fake. Got rid of gethostname() cruft. Moved UINT4
                here. Provide for *seek whence values. Move MDx context here
                and unify. Re-did prototypes.
	Modified at NRL for OPIE 2.0.
	Written at Bellcore for the S/Key Version 1 software distribution
		(skey.h).
*/
#ifndef _OPIE_H
#define _OPIE_H

#if _OPIE

#if HAVE_VOIDPTR
#define VOIDPTR void *
#else /* HAVE_VOIDPTR */
#define VOIDPTR char *
#endif /* HAVE_VOIDPTR */

#if HAVE_VOIDRET
#define VOIDRET void
#else /* HAVE_VOIDRET */
#define VOIDRET
#endif /* HAVE_VOIDRET */

#if HAVE_VOIDARG
#define NOARGS void
#else /* HAVE_VOIDARG */
#define NOARGS
#endif /* HAVE_VOIDARG */

#if HAVE_ANSIDECL
#define FUNCTION(arglist, args) (args)
#define AND ,
#else /* HAVE_ANSIDECL */
#define FUNCTION(arglist, args) arglist args;
#define AND ;
#endif /* HAVE_ANSIDECL */

#define FUNCTION_NOARGS ()

#ifndef __P
#if HAVE_ANSIPROTO
#define __P(x) x
#else /* HAVE_ANSIPROTO */
#define __P(x) ()
#endif /* HAVE_ANSIPROTO */
#endif /* __P */

#ifndef HAVE_SYS_UTSNAME_H
struct utsname {
	char nodename[65];
	};
#endif /* HAVE_SYS_UTSNAME_H */

#ifndef _SC_OPEN_MAX
#define _SC_OPEN_MAX 1
#endif /* _SC_OPEN_MAX */

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 1024
#endif /* MAXHOSTNAMELEN */

#else /* _OPIE */
#ifdef __STDC__
#define VOIDRET void
#define VOIDPTR void *
#else /* __STDC__ */
#define VOIDRET
#define VOIDPTR char *
#endif /* __STDC__ */
#endif /* _OPIE */

#ifndef __P
#ifdef __ARGS
#define __P __ARGS
#else /* __ARGS */
#ifdef __STDC__
#define __P(x) x
#else /* __STDC__ */
#define __P(x) ()
#endif /* __STDC__ */
#endif /* __ARGS */
#endif /* __P */

struct opie {
  int opie_flags;
  char opie_buf[256];
  char *opie_principal;
  int opie_n;
  char *opie_seed;
  char *opie_val;
  long opie_recstart;
  char opie_extbuf[129]; /* > OPIE_PRINCIPAL_MAX + 1 + 16 + 2 + 1 */
  long opie_extrecstart;
  char *opie_reinitkey;
};

#define __OPIE_FLAGS_RW 1
#define __OPIE_FLAGS_READ 2

/* Minimum length of a secret password */
#ifndef OPIE_SECRET_MIN
#define OPIE_SECRET_MIN 10
#endif	/* OPIE_SECRET_MIN */

/* Maximum length of a secret password */
#ifndef OPIE_SECRET_MAX
#define OPIE_SECRET_MAX 127
#endif	/* OPIE_SECRET_MAX */

/* Minimum length of a seed */
#ifndef OPIE_SEED_MIN
#define OPIE_SEED_MIN 5
#endif	/* OPIE_SEED_MIN */

/* Maximum length of a seed */
#ifndef OPIE_SEED_MAX
#define OPIE_SEED_MAX 16
#endif	/* OPIE_SEED_MAX */

/* Maximum length of a challenge (otp-md? 9999 seed) */
#ifndef OPIE_CHALLENGE_MAX
#define OPIE_CHALLENGE_MAX (7+1+4+1+OPIE_SEED_MAX)
#endif	/* OPIE_CHALLENGE_MAX */

/* Maximum length of a response that we allow */
#ifndef OPIE_RESPONSE_MAX
#define OPIE_RESPONSE_MAX (9+1+19+1+9+OPIE_SEED_MAX+1+19+1+19+1+19)
#endif	/* OPIE_RESPONSE_MAX */

/* Maximum length of a principal (read: user name) */
#ifndef OPIE_PRINCIPAL_MAX
#define OPIE_PRINCIPAL_MAX 32
#endif	/* OPIE_PRINCIPAL_MAX */

#ifndef __alpha
#define UINT4 unsigned long
#else   /* __alpha */
#define UINT4 unsigned int 
#endif  /* __alpha */

struct opiemdx_ctx {
	UINT4 state[4];
	UINT4 count[2];
	unsigned char buffer[64];
};

#ifndef SEEK_SET
#define SEEK_SET 0
#endif /* SEEK_SET */

#ifndef SEEK_END
#define SEEK_END 2
#endif /* SEEK_END */

int  opieaccessfile __P((char *));
int  rdnets __P((long));
int  isaddr __P((register char *));
int  opiealways __P((char *));
char *opieatob8 __P((char *,char *));
VOIDRET  opiebackspace __P((char *));
char *opiebtoa8 __P((char *,char *));
char *opiebtoe __P((char *,char *));
char *opiebtoh __P((char *,char *));
int  opieetob __P((char *,char *));
int  opiechallenge __P((struct opie *,char *,char *));
int  opiegenerator __P((char *,char *,char *));
int  opiegetsequence __P((struct opie *));
VOIDRET  opiehash __P((VOIDPTR, unsigned));
int  opiehtoi __P((register char));
int  opiekeycrunch __P((int, char *, char *, char *));
int  opielock __P((char *));
int  opielookup __P((struct opie *,char *));
VOIDRET  opiemd4init __P((struct opiemdx_ctx *));
VOIDRET  opiemd4update __P((struct opiemdx_ctx *,unsigned char *,unsigned int));
VOIDRET  opiemd4final __P((unsigned char *,struct opiemdx_ctx *));
VOIDRET  opiemd5init __P((struct opiemdx_ctx *));
VOIDRET  opiemd5update __P((struct opiemdx_ctx *,unsigned char *,unsigned int));
VOIDRET  opiemd5final __P((unsigned char *,struct opiemdx_ctx *));
int  opiepasscheck __P((char *));
VOIDRET  opierandomchallenge __P((char *));
char * opieskipspace __P((register char *));
VOIDRET  opiestripcrlf __P((char *));
int  opieverify __P((struct opie *,char *));
int opiepasswd __P((struct opie *, int, char *, int, char *, char *));
char *opiereadpass __P((char *, int, int));
int opielogin __P((char *line, char *name, char *host));

#if _OPIE
struct utmp;
int __opiegetutmpentry __P((char *, struct utmp *));
#ifdef EOF
FILE *__opieopen __P((char *, int, int));
#endif /* EOF */
int __opiereadrec __P((struct opie *));
int __opiewriterec __P((struct opie *));
#endif /* _OPIE */
#endif /* _OPIE_H */
