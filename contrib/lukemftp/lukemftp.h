/* $Id: lukemftp.h,v 1.43 2002/06/10 08:13:01 lukem Exp $ */

#define	FTP_PRODUCT	"lukemftp"
#define	FTP_VERSION	"1.6-beta2"

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <arpa/ftp.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#if HAVE_POLL
# if HAVE_POLL_H
#  include <poll.h>
# elif HAVE_SYS_POLL_H
#  include <sys/poll.h>
# endif
#elif HAVE_SELECT
# define USE_SELECT
#else
# error "no poll() or select() found"
#endif

#if HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if HAVE_ERR_H
# include <err.h>
#endif

#if USE_GLOB_H		/* not set by configure; used by other build systems */
# include <glob.h>
#else
# include "ftpglob.h"
#endif

#if HAVE_PATHS_H
# include <paths.h>
#endif
#ifndef _PATH_BSHELL
#define _PATH_BSHELL	"/bin/sh"
#endif
#ifndef _PATH_TMP
#define _PATH_TMP	"/tmp/"
#endif

typedef struct _stringlist {
	char	**sl_str;
	size_t	  sl_max;
	size_t	  sl_cur;
} StringList;

StringList *sl_init(void);
int	 sl_add(StringList *, char *);
void	 sl_free(StringList *, int);
char	*sl_find(StringList *, char *);

#if HAVE_TERMCAP_H
# include <termcap.h>
#else
int	 tgetent(char *, const char *);
char	*tgetstr(const char *, char **);
int	 tgetflag(const char *);
int	 tgetnum(const char *);
char	*tgoto(const char *, int, int);
void	 tputs(const char *, int, int (*)(int));
#endif

#if HAVE_UTIL_H
# include <util.h>
#endif

#if HAVE_LIBUTIL_H
# include <libutil.h>
#endif

#if HAVE_VIS_H
# include <vis.h>
#else
# include "ftpvis.h"
#endif

#if ! HAVE_IN_PORT_T
typedef unsigned short in_port_t;
#endif

#if ! HAVE_SA_FAMILY_T
typedef unsigned short sa_family_t;
#endif

#if ! HAVE_SOCKLEN_T
typedef unsigned int socklen_t;
#endif

#if HAVE_AF_INET6 && HAVE_SOCKADDR_IN6
# define INET6
#endif


#if ! HAVE_RFC2553_NETDB

				/* RFC 2553 */
#undef	EAI_ADDRFAMILY
#define	EAI_ADDRFAMILY	 1	/* address family for hostname not supported */
#undef	EAI_AGAIN
#define	EAI_AGAIN	 2	/* temporary failure in name resolution */
#undef	EAI_BADFLAGS
#define	EAI_BADFLAGS	 3	/* invalid value for ai_flags */
#undef	EAI_FAIL
#define	EAI_FAIL	 4	/* non-recoverable failure in name resolution */
#undef	EAI_FAMILY
#define	EAI_FAMILY	 5	/* ai_family not supported */
#undef	EAI_MEMORY
#define	EAI_MEMORY	 6	/* memory allocation failure */
#undef	EAI_NODATA
#define	EAI_NODATA	 7	/* no address associated with hostname */
#undef	EAI_NONAME
#define	EAI_NONAME	 8	/* hostname nor servname provided, or not known */
#undef	EAI_SERVICE
#define	EAI_SERVICE	 9	/* servname not supported for ai_socktype */
#undef	EAI_SOCKTYPE
#define	EAI_SOCKTYPE	10	/* ai_socktype not supported */
#undef	EAI_SYSTEM
#define	EAI_SYSTEM	11	/* system error returned in errno */

				/* KAME extensions? */
#undef	EAI_BADHINTS
#define	EAI_BADHINTS	12
#undef	EAI_PROTOCOL
#define	EAI_PROTOCOL	13
#undef	EAI_MAX
#define	EAI_MAX		14

				/* RFC 2553 */
#undef	NI_MAXHOST
#define	NI_MAXHOST	1025
#undef	NI_MAXSERV
#define	NI_MAXSERV	32

#undef	NI_NOFQDN
#define	NI_NOFQDN	0x00000001
#undef	NI_NUMERICHOST
#define	NI_NUMERICHOST	0x00000002
#undef	NI_NAMEREQD
#define	NI_NAMEREQD	0x00000004
#undef	NI_NUMERICSERV
#define	NI_NUMERICSERV	0x00000008
#undef	NI_DGRAM
#define	NI_DGRAM	0x00000010

				/* RFC 2553 */
#undef	AI_PASSIVE
#define	AI_PASSIVE	0x00000001 /* get address to use bind() */
#undef	AI_CANONNAME
#define	AI_CANONNAME	0x00000002 /* fill ai_canonname */

				/* KAME extensions ? */
#undef	AI_NUMERICHOST
#define	AI_NUMERICHOST	0x00000004 /* prevent name resolution */
#undef	AI_MASK
#define	AI_MASK		(AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST)

				/* RFC 2553 */
#undef	AI_ALL
#define	AI_ALL		0x00000100 /* IPv6 and IPv4-mapped (with AI_V4MAPPED) */
#undef	AI_V4MAPPED_CFG
#define	AI_V4MAPPED_CFG	0x00000200 /* accept IPv4-mapped if kernel supports */
#undef	AI_ADDRCONFIG
#define	AI_ADDRCONFIG	0x00000400 /* only if any address is assigned */
#undef	AI_V4MAPPED
#define	AI_V4MAPPED	0x00000800 /* accept IPv4-mapped IPv6 address */

#endif /* ! HAVE_RFC2553_NETDB */


#if ! HAVE_RFC2553_NETDB && ! HAVE_ADDRINFO

struct addrinfo {
	int	ai_flags;	/* AI_PASSIVE, AI_CANONNAME, AI_NUMERICHOST */
	int	ai_family;	/* PF_xxx */
	int	ai_socktype;	/* SOCK_xxx */
	int	ai_protocol;	/* 0 or IPPROTO_xxx for IPv4 and IPv6 */
	size_t	ai_addrlen;	/* length of ai_addr */
	char	*ai_canonname;	/* canonical name for hostname */
	struct sockaddr *ai_addr;	/* binary address */
	struct addrinfo *ai_next;	/* next structure in linked list */
};

int	getaddrinfo(const char *, const char *,
	    const struct addrinfo *, struct addrinfo **);
int	getnameinfo(const struct sockaddr *, socklen_t, char *,
	    size_t, char *, size_t, int);
void	freeaddrinfo(struct addrinfo *);
char   *gai_strerror(int);

#endif /* ! HAVE_RFC2553_NETDB && ! HAVE_ADDRINFO */


#if ! HAVE_D_NAMLEN
# define DIRENT_MISSING_D_NAMLEN
#endif

#if ! HAVE_H_ERRNO_D
extern int	h_errno;
#endif
#define HAVE_H_ERRNO	1		/* XXX: an assumption for now... */

#if ! HAVE_FCLOSE_D
int	fclose(FILE *);
#endif

#if ! HAVE_GETPASS_D
char	*getpass(const char *);
#endif

#if ! HAVE_OPTARG_D
extern char    *optarg;
#endif

#if ! HAVE_OPTIND_D
extern int	optind;
#endif

#if ! HAVE_PCLOSE_D
int	pclose(FILE *);
#endif

#if ! HAVE_ERR
void	err(int, const char *, ...);
void	errx(int, const char *, ...);
void	warn(const char *, ...);
void	warnx(const char *, ...);
#endif

#if ! HAVE_FGETLN
char   *fgetln(FILE *, size_t *);
#endif

#if ! HAVE_FSEEKO
int	fseeko(FILE *, off_t, int);
#endif

#if ! HAVE_FPARSELN
# define FPARSELN_UNESCESC	0x01
# define FPARSELN_UNESCCONT	0x02
# define FPARSELN_UNESCCOMM	0x04
# define FPARSELN_UNESCREST	0x08
# define FPARSELN_UNESCALL	0x0f
char   *fparseln(FILE *, size_t *, size_t *, const char[3], int);
#endif

#if ! HAVE_INET_NTOP
const char *inet_ntop(int, const void *, char *, size_t);
#endif

#if ! HAVE_INET_PTON
int inet_pton(int, const char *, void *);
#endif

#if ! HAVE_MKSTEMP
int	mkstemp(char *);
#endif

#if ! HAVE_SETPROGNAME
const char *getprogname(void);
void	setprogname(const char *);
#endif

#if ! HAVE_SNPRINTF
int	snprintf(char *, size_t, const char *, ...);
#endif

#if ! HAVE_STRDUP
char   *strdup(const char *);
#endif

#if ! HAVE_STRERROR
char   *strerror(int);
#endif

#if ! HAVE_STRPTIME || ! HAVE_STRPTIME_D
char   *strptime(const char *, const char *, struct tm *);
#endif

#if HAVE_QUAD_SUPPORT
# if ! HAVE_STRTOLL && HAVE_LONG_LONG
long long strtoll(const char *, char **, int);
#  if ! defined(QUAD_MIN)
#   define QUAD_MIN	(-0x7fffffffffffffffL-1)
#  endif
#  if ! defined(QUAD_MAX)
#   define QUAD_MAX	(0x7fffffffffffffffL)
#  endif
# endif
#else	/* ! HAVE_QUAD_SUPPORT */
# define NO_LONG_LONG	1
#endif	/* ! HAVE_QUAD_SUPPORT */

#if ! HAVE_TIMEGM
time_t	timegm(struct tm *);
#endif

#if ! HAVE_HSTRERROR
char   *strerror(int);
#endif

#if ! HAVE_STRLCAT
size_t	strlcat(char *, const char *, size_t);
#endif

#if ! HAVE_STRLCPY
size_t	strlcpy(char *, const char *, size_t);
#endif

#if ! HAVE_STRSEP
char   *strsep(char **stringp, const char *delim);
#endif

#if ! HAVE_MEMMOVE
# define memmove(a,b,c)	bcopy((b),(a),(c))
	/* XXX: add others #defines for borken systems? */
#endif

#if HAVE_GETPASSPHRASE
# define getpass getpassphrase
#endif

#if ! defined(MIN)
# define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif
#if ! defined(MAX)
# define MAX(a, b)	((a) < (b) ? (b) : (a))
#endif

#if ! defined(timersub)
# define timersub(tvp, uvp, vvp)					\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (0)
#endif

#if ! defined(S_ISLNK)
# define S_ISLNK(m)	((m & S_IFMT) == S_IFLNK)
#endif

#define	EPOCH_YEAR	1970
#define	SECSPERHOUR	3600
#define	SECSPERDAY	86400
#define	TM_YEAR_BASE	1900
