/*
 * ntp_stdlib.h - Prototypes for NTP lib.
 */
#include <sys/types.h>

#include "ntp_types.h"
#include "ntp_string.h"
#include "l_stdlib.h"

/*
 * Handle gcc __attribute__ if available.
 */
#ifndef __attribute__
/* This feature is available in gcc versions 2.5 and later.  */
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 5) || (defined(__STRICT_ANSI__))
#  define __attribute__(Spec) /* empty */
# endif
/* The __-protected variants of `format' and `printf' attributes
   are accepted by gcc versions 2.6.4 (effectively 2.7) and later.  */
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
#  define __format__ format
#  define __printf__ printf
# endif
#endif

#if defined(__STDC__) || defined(HAVE_STDARG_H)
# include <stdarg.h>
extern	void	msyslog		P((int, const char *, ...))
				__attribute__((__format__(__printf__, 2, 3)));
#else
# include <varargs.h>
extern	void msyslog		P(());
#endif

#if 0				/* HMS: These seem to be unused now */
extern	void	auth_des	P((u_long *, u_char *));
extern	void	auth_delkeys	P((void));
extern	int	auth_parity	P((u_long *));
extern	void	auth_setkey	P((u_long, u_long *));
extern	void	auth_subkeys	P((u_long *, u_char *, u_char *));
#endif

extern	void	auth1crypt	P((u_long, u_int32 *, int));
extern	int	auth2crypt	P((u_long, u_int32 *, int));
extern	void	auth_delkeys	P((void));
extern	int	auth_havekey	P((u_long));
extern	int	authdecrypt	P((u_long, u_int32 *, int, int));
extern	int	authencrypt	P((u_long, u_int32 *, int));
extern	int	authhavekey	P((u_long));
extern	int	authistrusted	P((u_long));
extern	int	authreadkeys	P((const char *));
extern	void	authtrust	P((u_long, int));
extern	int	authusekey	P((u_long, int, const u_char *));

extern	u_long	calleapwhen	P((u_long));
extern	u_long	calyearstart	P((u_long));
extern	const char *clockname	P((int));
extern	int	clocktime	P((int, int, int, int, int, u_long, u_long *, u_int32 *));
#if defined SYS_WINNT && defined DEBUG
# define emalloc(_c) debug_emalloc(_c, __FILE__, __LINE__)
extern	void *	debug_emalloc		P((u_int, char *, int));
#else
extern	void *	emalloc		P((u_int));
#endif
extern	int	ntp_getopt	P((int, char **, const char *));
extern	void	init_auth	P((void));
extern	void	init_lib	P((void));
extern	void	init_random	P((void));
extern	struct savekey *auth_findkey P((u_long));
extern	int	auth_moremem	P((void));
extern	int	ymd2yd		P((int, int, int));

#ifdef	DES
extern	int	DESauthdecrypt	P((u_char *, u_int32 *, int, int));
extern	int	DESauthencrypt	P((u_char *, u_int32 *, int));
extern	void	DESauth_setkey	P((u_long, const u_int32 *));
extern	void	DESauth_subkeys	P((const u_int32 *, u_char *, u_char *));
extern	void	DESauth_des	P((u_int32 *, u_char *));
extern	int	DESauth_parity	P((u_int32 *));
#endif	/* DES */

#ifdef	MD5
extern	int	MD5authdecrypt	P((u_char *, u_int32 *, int, int));
extern	int	MD5authencrypt	P((u_char *, u_int32 *, int));
extern	void	MD5auth_setkey	P((u_long, const u_char *, const int));
extern	u_long	session_key	P((u_int32, u_int32, u_long, u_long));
#endif	/* MD5 */

extern	int	atoint		P((const char *, long *));
extern	int	atouint		P((const char *, u_long *));
extern	int	hextoint	P((const char *, u_long *));
extern	char *	humandate	P((u_long));
extern	char *	humanlogtime	P((void));
extern	char *	inttoa		P((long));
extern	char *	mfptoa		P((u_long, u_long, int));
extern	char *	mfptoms		P((u_long, u_long, int));
extern	const char * modetoa	P((int));
extern  const char * eventstr   P((int));
extern  const char * ceventstr  P((int));
extern	char *	statustoa	P((int, int));
extern  const char * sysstatstr P((int));
extern  const char * peerstatstr P((int));
extern  const char * clockstatstr P((int));
extern	u_int32	netof		P((u_int32));
extern	char *	numtoa		P((u_int32));
extern	char *	numtohost	P((u_int32));
extern	int	octtoint	P((const char *, u_long *));
extern	u_long	ranp2		P((int));
extern	char *	refnumtoa	P((u_int32));
extern	int	tsftomsu	P((u_long, int));
extern	char *	uinttoa		P((u_long));

extern	int	decodenetnum	P((const char *, u_int32 *));

extern	const char *	FindConfig	P((const char *));

extern	void	signal_no_reset P((int, RETSIGTYPE (*func)(int)));

extern	void	getauthkeys 	P((char *));
extern	void	auth_agekeys	P((void));
extern	void	rereadkeys	P((void));

/*
 * Variable declarations for libntp.
 */

/*
 * Defined by any program.
 */
extern volatile int debug;		/* debugging flag */

/* authkeys.c */
extern u_long	authkeynotfound;	/* keys not found */
extern u_long	authkeylookups;		/* calls to lookup keys */
extern u_long	authnumkeys;		/* number of active keys */
extern u_long	authkeyexpired;		/* key lifetime expirations */
extern u_long	authkeyuncached;	/* cache misses */
extern u_long	authencryptions;	/* calls to encrypt */
extern u_long	authdecryptions;	/* calls to decrypt */

extern int	authnumfreekeys;

/*
 * The key cache. We cache the last key we looked at here.
 */
extern u_long	cache_keyid;		/* key identifier */
extern u_char *	cache_key;		/* key pointer */
extern u_int	cache_keylen;		/* key length */

/* clocktypes.c */
struct clktype;
extern struct clktype clktypes[];

/* getopt.c */
extern char *	ntp_optarg;		/* global argument pointer */
extern int	ntp_optind;		/* global argv index */

/* machines.c */
extern const char *set_tod_using;

/* mexit.c */
#if defined SYS_WINNT || defined SYS_CYGWIN32
extern HANDLE	hServDoneEvent;
#endif

/* systime.c */
extern int	systime_10ms_ticks;	/* adj sysclock in 10ms increments */

extern double	sys_maxfreq;		/* max frequency correction */

/* version.c */
extern const char *Version;		/* version declaration */
