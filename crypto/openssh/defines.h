#ifndef _DEFINES_H
#define _DEFINES_H

/* $Id: defines.h,v 1.92 2002/06/24 16:26:49 stevesk Exp $ */


/* Constants */

#ifndef SHUT_RDWR
enum
{
  SHUT_RD = 0,		/* No more receptions.  */
  SHUT_WR,			/* No more transmissions.  */
  SHUT_RDWR			/* No more receptions or transmissions.  */
};
# define SHUT_RD   SHUT_RD
# define SHUT_WR   SHUT_WR
# define SHUT_RDWR SHUT_RDWR
#endif

#ifndef IPTOS_LOWDELAY
# define IPTOS_LOWDELAY          0x10
# define IPTOS_THROUGHPUT        0x08
# define IPTOS_RELIABILITY       0x04
# define IPTOS_LOWCOST           0x02
# define IPTOS_MINCOST           IPTOS_LOWCOST
#endif /* IPTOS_LOWDELAY */

#ifndef MAXPATHLEN
# ifdef PATH_MAX
#  define MAXPATHLEN PATH_MAX
# else /* PATH_MAX */
#  define MAXPATHLEN 64 /* Should be safe */
# endif /* PATH_MAX */
#endif /* MAXPATHLEN */

#ifndef STDIN_FILENO
# define STDIN_FILENO    0
#endif
#ifndef STDOUT_FILENO
# define STDOUT_FILENO   1
#endif
#ifndef STDERR_FILENO
# define STDERR_FILENO   2
#endif

#ifndef NGROUPS_MAX	/* Disable groupaccess if NGROUP_MAX is not set */
#ifdef NGROUPS
#define NGROUPS_MAX NGROUPS
#else
#define NGROUPS_MAX 0
#endif
#endif

#ifndef O_NONBLOCK	/* Non Blocking Open */
# define O_NONBLOCK      00004
#endif

#ifndef S_ISDIR
# define S_ISDIR(mode)	(((mode) & (_S_IFMT)) == (_S_IFDIR))
#endif /* S_ISDIR */

#ifndef S_ISREG 
# define S_ISREG(mode)	(((mode) & (_S_IFMT)) == (_S_IFREG))
#endif /* S_ISREG */

#ifndef S_ISLNK
# define S_ISLNK(mode)	(((mode) & S_IFMT) == S_IFLNK)
#endif /* S_ISLNK */

#ifndef S_IXUSR
# define S_IXUSR			0000100	/* execute/search permission, */
# define S_IXGRP			0000010	/* execute/search permission, */
# define S_IXOTH			0000001	/* execute/search permission, */
# define _S_IWUSR			0000200	/* write permission, */
# define S_IWUSR			_S_IWUSR	/* write permission, owner */
# define S_IWGRP			0000020	/* write permission, group */
# define S_IWOTH			0000002	/* write permission, other */
# define S_IRUSR			0000400	/* read permission, owner */
# define S_IRGRP			0000040	/* read permission, group */
# define S_IROTH			0000004	/* read permission, other */
# define S_IRWXU			0000700	/* read, write, execute */
# define S_IRWXG			0000070	/* read, write, execute */
# define S_IRWXO			0000007	/* read, write, execute */
#endif /* S_IXUSR */

#if !defined(MAP_ANON) && defined(MAP_ANONYMOUS)
#define MAP_ANON MAP_ANONYMOUS
#endif

#ifndef MAP_FAILED
# define MAP_FAILED ((void *)-1)
#endif

/* *-*-nto-qnx doesn't define this constant in the system headers */
#ifdef MISSING_NFDBITS
# define	NFDBITS (8 * sizeof(unsigned long))
#endif

/*
SCO Open Server 3 has INADDR_LOOPBACK defined in rpc/rpc.h but
including rpc/rpc.h breaks Solaris 6
*/
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK ((ulong)0x7f000001)
#endif

/* Types */

/* If sys/types.h does not supply intXX_t, supply them ourselves */
/* (or die trying) */


#ifndef HAVE_U_INT
typedef unsigned int u_int;
#endif

#ifndef HAVE_INTXX_T
# if (SIZEOF_CHAR == 1)
typedef char int8_t;
# else
#  error "8 bit int type not found."
# endif
# if (SIZEOF_SHORT_INT == 2)
typedef short int int16_t;
# else
#  ifdef _CRAY
#   if (SIZEOF_SHORT_INT == 4)
typedef short int16_t;
#   else
typedef long  int16_t;
#   endif
#  else
#   error "16 bit int type not found."
#  endif /* _CRAY */
# endif
# if (SIZEOF_INT == 4)
typedef int int32_t;
# else
#  ifdef _CRAY
typedef long  int32_t;
#  else
#   error "32 bit int type not found."
#  endif /* _CRAY */
# endif
#endif

/* If sys/types.h does not supply u_intXX_t, supply them ourselves */
#ifndef HAVE_U_INTXX_T
# ifdef HAVE_UINTXX_T
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
# define HAVE_U_INTXX_T 1
# else
#  if (SIZEOF_CHAR == 1)
typedef unsigned char u_int8_t;
#  else
#   error "8 bit int type not found."
#  endif
#  if (SIZEOF_SHORT_INT == 2)
typedef unsigned short int u_int16_t;
#  else
#   ifdef _CRAY
#    if (SIZEOF_SHORT_INT == 4)
typedef unsigned short u_int16_t;
#    else
typedef unsigned long  u_int16_t;
#    endif
#   else
#    error "16 bit int type not found."
#   endif
#  endif
#  if (SIZEOF_INT == 4)
typedef unsigned int u_int32_t;
#  else
#   ifdef _CRAY
typedef unsigned long  u_int32_t;
#   else
#    error "32 bit int type not found."
#   endif
#  endif
# endif
#define __BIT_TYPES_DEFINED__
#endif

/* 64-bit types */
#ifndef HAVE_INT64_T
# if (SIZEOF_LONG_INT == 8)
typedef long int int64_t;
#   define HAVE_INT64_T 1
# else
#  if (SIZEOF_LONG_LONG_INT == 8)
typedef long long int int64_t;
#   define HAVE_INT64_T 1
#  endif
# endif
#endif
#ifndef HAVE_U_INT64_T
# if (SIZEOF_LONG_INT == 8)
typedef unsigned long int u_int64_t;
#   define HAVE_U_INT64_T 1
# else
#  if (SIZEOF_LONG_LONG_INT == 8)
typedef unsigned long long int u_int64_t;
#   define HAVE_U_INT64_T 1
#  endif
# endif
#endif
#if !defined(HAVE_LONG_LONG_INT) && (SIZEOF_LONG_LONG_INT == 8)
# define HAVE_LONG_LONG_INT 1
#endif

#ifndef HAVE_U_CHAR
typedef unsigned char u_char;
# define HAVE_U_CHAR
#endif /* HAVE_U_CHAR */

#ifndef HAVE_SIZE_T
typedef unsigned int size_t;
# define HAVE_SIZE_T
#endif /* HAVE_SIZE_T */

#ifndef HAVE_SSIZE_T
typedef int ssize_t;
# define HAVE_SSIZE_T
#endif /* HAVE_SSIZE_T */

#ifndef HAVE_CLOCK_T
typedef long clock_t;
# define HAVE_CLOCK_T
#endif /* HAVE_CLOCK_T */

#ifndef HAVE_SA_FAMILY_T
typedef int sa_family_t;
# define HAVE_SA_FAMILY_T
#endif /* HAVE_SA_FAMILY_T */

#ifndef HAVE_PID_T
typedef int pid_t;
# define HAVE_PID_T
#endif /* HAVE_PID_T */

#ifndef HAVE_SIG_ATOMIC_T
typedef int sig_atomic_t;
# define HAVE_SIG_ATOMIC_T
#endif /* HAVE_SIG_ATOMIC_T */

#ifndef HAVE_MODE_T
typedef int mode_t;
# define HAVE_MODE_T
#endif /* HAVE_MODE_T */

#if !defined(HAVE_SS_FAMILY_IN_SS) && defined(HAVE___SS_FAMILY_IN_SS)
# define ss_family __ss_family
#endif /* !defined(HAVE_SS_FAMILY_IN_SS) && defined(HAVE_SA_FAMILY_IN_SS) */

#ifndef HAVE_SYS_UN_H
struct	sockaddr_un {
	short	sun_family;		/* AF_UNIX */
	char	sun_path[108];		/* path name (gag) */
};
#endif /* HAVE_SYS_UN_H */

#if defined(BROKEN_SYS_TERMIO_H) && !defined(_STRUCT_WINSIZE)
#define _STRUCT_WINSIZE
struct winsize {
      unsigned short ws_row;          /* rows, in characters */
      unsigned short ws_col;          /* columns, in character */
      unsigned short ws_xpixel;       /* horizontal size, pixels */
      unsigned short ws_ypixel;       /* vertical size, pixels */
};
#endif

/* *-*-nto-qnx does not define this type in the system headers */
#ifdef MISSING_FD_MASK
 typedef unsigned long int	fd_mask;
#endif

/* Paths */

#ifndef _PATH_BSHELL
# define _PATH_BSHELL "/bin/sh"
#endif
#ifndef _PATH_CSHELL
# define _PATH_CSHELL "/bin/csh"
#endif
#ifndef _PATH_SHELLS
# define _PATH_SHELLS "/etc/shells"
#endif

#ifdef USER_PATH
# ifdef _PATH_STDPATH
#  undef _PATH_STDPATH
# endif
# define _PATH_STDPATH USER_PATH
#endif

#ifndef _PATH_STDPATH
# define _PATH_STDPATH "/usr/bin:/bin:/usr/sbin:/sbin"
#endif

#ifndef _PATH_DEVNULL
# define _PATH_DEVNULL "/dev/null"
#endif

#ifndef MAIL_DIRECTORY
# define MAIL_DIRECTORY "/var/spool/mail"
#endif

#ifndef MAILDIR
# define MAILDIR MAIL_DIRECTORY
#endif

#if !defined(_PATH_MAILDIR) && defined(MAILDIR)
# define _PATH_MAILDIR MAILDIR
#endif /* !defined(_PATH_MAILDIR) && defined(MAILDIR) */

#ifndef _PATH_NOLOGIN
# define _PATH_NOLOGIN "/etc/nologin"
#endif

/* Define this to be the path of the xauth program. */
#ifdef XAUTH_PATH
#define _PATH_XAUTH XAUTH_PATH
#endif /* XAUTH_PATH */

/* derived from XF4/xc/lib/dps/Xlibnet.h */
#ifndef X_UNIX_PATH
#  ifdef __hpux
#    define X_UNIX_PATH "/var/spool/sockets/X11/%u"
#  else
#    define X_UNIX_PATH "/tmp/.X11-unix/X%u"
#  endif
#endif /* X_UNIX_PATH */
#define _PATH_UNIX_X X_UNIX_PATH

#ifndef _PATH_TTY
# define _PATH_TTY "/dev/tty"
#endif

/* Macros */

#if defined(HAVE_LOGIN_GETCAPBOOL) && defined(HAVE_LOGIN_CAP_H)
# define HAVE_LOGIN_CAP
#endif

#ifndef MAX
# define MAX(a,b) (((a)>(b))?(a):(b))
# define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef roundup
# define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))
#endif

#ifndef timersub
#define timersub(a, b, result)					\
   do {								\
      (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;		\
      (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;		\
      if ((result)->tv_usec < 0) {				\
	 --(result)->tv_sec;					\
	 (result)->tv_usec += 1000000;				\
      }								\
   } while (0)
#endif

#ifndef __P
# define __P(x) x
#endif

#if !defined(IN6_IS_ADDR_V4MAPPED)
# define IN6_IS_ADDR_V4MAPPED(a) \
	((((u_int32_t *) (a))[0] == 0) && (((u_int32_t *) (a))[1] == 0) && \
	 (((u_int32_t *) (a))[2] == htonl (0xffff)))
#endif /* !defined(IN6_IS_ADDR_V4MAPPED) */

#if !defined(__GNUC__) || (__GNUC__ < 2)
# define __attribute__(x)
#endif /* !defined(__GNUC__) || (__GNUC__ < 2) */

/* *-*-nto-qnx doesn't define this macro in the system headers */
#ifdef MISSING_HOWMANY
# define howmany(x,y)	(((x)+((y)-1))/(y))
#endif

#ifndef OSSH_ALIGNBYTES
#define OSSH_ALIGNBYTES	(sizeof(int) - 1)
#endif
#ifndef __CMSG_ALIGN
#define	__CMSG_ALIGN(p) (((u_int)(p) + OSSH_ALIGNBYTES) &~ OSSH_ALIGNBYTES)
#endif

/* Length of the contents of a control message of length len */
#ifndef CMSG_LEN
#define	CMSG_LEN(len)	(__CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))
#endif

/* Length of the space taken up by a padded control message of length len */
#ifndef CMSG_SPACE
#define	CMSG_SPACE(len)	(__CMSG_ALIGN(sizeof(struct cmsghdr)) + __CMSG_ALIGN(len))
#endif

/* Function replacement / compatibility hacks */

#if !defined(HAVE_GETADDRINFO) && (defined(HAVE_OGETADDRINFO) || defined(HAVE_NGETADDRINFO))
# define HAVE_GETADDRINFO
#endif

#ifndef HAVE_GETOPT_OPTRESET
# undef getopt
# undef opterr
# undef optind
# undef optopt
# undef optreset
# undef optarg
# define getopt(ac, av, o)  BSDgetopt(ac, av, o)
# define opterr             BSDopterr
# define optind             BSDoptind
# define optopt             BSDoptopt
# define optreset           BSDoptreset
# define optarg             BSDoptarg
#endif

/* In older versions of libpam, pam_strerror takes a single argument */
#ifdef HAVE_OLD_PAM
# define PAM_STRERROR(a,b) pam_strerror((b))
#else
# define PAM_STRERROR(a,b) pam_strerror((a),(b))
#endif

#ifdef PAM_SUN_CODEBASE
# define PAM_MSG_MEMBER(msg, n, member) ((*(msg))[(n)].member)
#else
# define PAM_MSG_MEMBER(msg, n, member) ((msg)[(n)]->member)
#endif

#if defined(BROKEN_GETADDRINFO) && defined(HAVE_GETADDRINFO)
# undef HAVE_GETADDRINFO
#endif
#if defined(BROKEN_GETADDRINFO) && defined(HAVE_FREEADDRINFO)
# undef HAVE_FREEADDRINFO
#endif
#if defined(BROKEN_GETADDRINFO) && defined(HAVE_GAI_STRERROR)
# undef HAVE_GAI_STRERROR
#endif

#if !defined(HAVE_MEMMOVE) && defined(HAVE_BCOPY)
# define memmove(s1, s2, n) bcopy((s2), (s1), (n))
#endif /* !defined(HAVE_MEMMOVE) && defined(HAVE_BCOPY) */

#if defined(HAVE_VHANGUP) && !defined(HAVE_DEV_PTMX)
#  define USE_VHANGUP
#endif /* defined(HAVE_VHANGUP) && !defined(HAVE_DEV_PTMX) */

#ifndef GETPGRP_VOID
# define getpgrp() getpgrp(0)
#endif

/* OPENSSL_free() is Free() in versions before OpenSSL 0.9.6 */
#if !defined(OPENSSL_VERSION_NUMBER) || (OPENSSL_VERSION_NUMBER < 0x0090600f)
# define OPENSSL_free(x) Free(x)
#endif

#if !defined(HAVE___func__) && defined(HAVE___FUNCTION__)
#  define __func__ __FUNCTION__
#elif !defined(HAVE___func__)
#  define __func__ ""
#endif

/*
 * Define this to use pipes instead of socketpairs for communicating with the
 * client program.  Socketpairs do not seem to work on all systems.
 *
 * configure.ac sets this for a few OS's which are known to have problems
 * but you may need to set it yourself
 */
/* #define USE_PIPES 1 */

/**
 ** login recorder definitions
 **/

/* FIXME: put default paths back in */
#ifndef UTMP_FILE
#  ifdef _PATH_UTMP
#    define UTMP_FILE _PATH_UTMP
#  else
#    ifdef CONF_UTMP_FILE
#      define UTMP_FILE CONF_UTMP_FILE
#    endif
#  endif
#endif
#ifndef WTMP_FILE
#  ifdef _PATH_WTMP
#    define WTMP_FILE _PATH_WTMP
#  else
#    ifdef CONF_WTMP_FILE
#      define WTMP_FILE CONF_WTMP_FILE
#    endif
#  endif
#endif
/* pick up the user's location for lastlog if given */
#ifndef LASTLOG_FILE
#  ifdef _PATH_LASTLOG
#    define LASTLOG_FILE _PATH_LASTLOG
#  else
#    ifdef CONF_LASTLOG_FILE
#      define LASTLOG_FILE CONF_LASTLOG_FILE
#    endif
#  endif
#endif


/* The login() library function in libutil is first choice */
#if defined(HAVE_LOGIN) && !defined(DISABLE_LOGIN)
#  define USE_LOGIN

#else
/* Simply select your favourite login types. */
/* Can't do if-else because some systems use several... <sigh> */
#  if defined(UTMPX_FILE) && !defined(DISABLE_UTMPX)
#    define USE_UTMPX
#  endif
#  if defined(UTMP_FILE) && !defined(DISABLE_UTMP)
#    define USE_UTMP
#  endif
#  if defined(WTMPX_FILE) && !defined(DISABLE_WTMPX)
#    define USE_WTMPX
#  endif
#  if defined(WTMP_FILE) && !defined(DISABLE_WTMP)
#    define USE_WTMP
#  endif

#endif

/* I hope that the presence of LASTLOG_FILE is enough to detect this */
#if defined(LASTLOG_FILE) && !defined(DISABLE_LASTLOG)
#  define USE_LASTLOG
#endif

/** end of login recorder definitions */

#endif /* _DEFINES_H */
