/* ntp_stdlib.h,v 3.1 1993/07/06 01:06:58 jbj Exp
 * ntp_stdlib.h - Prototypes for XNTP lib.
 */
#include <sys/types.h>

#include "ntp_types.h"
#include "ntp_string.h"
#include "l_stdlib.h"

#ifndef	P
#if defined(__STDC__) || defined(USE_PROTOTYPES)
#define P(x)	x
#else
#define	P(x)	()
#if	!defined(const)
#define	const
#endif
#endif
#endif

#if defined(__STDC__)
extern	void	msyslog		P((int, char *, ...));
#else
extern	void	msyslog		P(());
#endif

extern	void	auth_des	P((U_LONG *, u_char *));
extern	void	auth_delkeys	P((void));
extern	int	auth_havekey	P((U_LONG));
extern	int	auth_parity	P((U_LONG *));
extern	void	auth_setkey	P((U_LONG, U_LONG *));
extern	void	auth_subkeys	P((U_LONG *, u_char *, u_char *));
extern	int	authistrusted	P((U_LONG));
extern	int	authusekey	P((U_LONG, int, const char *));

extern	void	auth_delkeys	P((void));

extern	void	auth1crypt	P((U_LONG, U_LONG *, int));
extern	int	auth2crypt	P((U_LONG, U_LONG *, int));
extern	int	authdecrypt	P((U_LONG, U_LONG *, int));
extern	int	authencrypt	P((U_LONG, U_LONG *, int));
extern	int	authhavekey	P((U_LONG));
extern	int	authreadkeys	P((const char *));
extern	void	authtrust	P((U_LONG, int));
extern	void	calleapwhen	P((U_LONG, U_LONG *, U_LONG *));
extern	U_LONG	calyearstart	P((U_LONG));
extern	const char *clockname	P((int));
extern	int	clocktime	P((int, int, int, int, int, U_LONG, U_LONG *, U_LONG *));
extern	char *	emalloc		P((u_int));
extern	int	getopt_l	P((int, char **, char *));
extern	void	init_auth	P((void));
extern	void	init_lib	P((void));
extern	void	init_random	P((void));

#ifdef	DES
extern	void	DESauth1crypt	P((U_LONG, U_LONG *, int));
extern	int	DESauth2crypt	P((U_LONG, U_LONG *, int));
extern	int	DESauthdecrypt	P((U_LONG, const U_LONG *, int));
extern	int	DESauthencrypt	P((U_LONG, U_LONG *, int));
extern	void	DESauth_setkey	P((U_LONG, const U_LONG *));
extern	void	DESauth_subkeys	P((const U_LONG *, u_char *, u_char *));
extern	void	DESauth_des	P((U_LONG *, u_char *));
extern	int	DESauth_parity	P((U_LONG *));
#endif	/* DES */

#ifdef	MD5
extern	void	MD5auth1crypt	P((U_LONG, U_LONG *, int));
extern	int	MD5auth2crypt	P((U_LONG, U_LONG *, int));
extern	int	MD5authdecrypt	P((U_LONG, const U_LONG *, int));
extern	int	MD5authencrypt	P((U_LONG, U_LONG *, int));
extern	void	MD5auth_setkey	P((U_LONG, const U_LONG *));
#endif	/* MD5 */

extern	int	atoint		P((const char *, LONG *));
extern	int	atouint		P((const char *, U_LONG *));
extern	int	hextoint	P((const char *, U_LONG *));
extern	char *	humandate	P((U_LONG));
extern	char *	inttoa		P((LONG));
extern	char *	mfptoa		P((U_LONG, U_LONG, int));
extern	char *	mfptoms		P((U_LONG, U_LONG, int));
extern	char *	modetoa		P((int));
extern	char *	numtoa		P((U_LONG));
extern	char *	numtohost	P((U_LONG));
extern	int	octtoint	P((const char *, U_LONG *));
extern	U_LONG	ranp2		P((int));
extern	char *	refnumtoa	P((U_LONG));
extern	int	tsftomsu	P((U_LONG, int));
extern	char *	uinttoa		P((U_LONG));

extern	int	decodenetnum	P((const char *, U_LONG *));

extern RETSIGTYPE signal_no_reset P((int, RETSIGTYPE (*func)()));
