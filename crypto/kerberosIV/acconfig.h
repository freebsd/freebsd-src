/* $Id: acconfig.h,v 1.71 1997/06/01 22:32:24 assar Exp $ */

/*  Define this if RETSIGTYPE == void  */
#undef VOID_RETSIGTYPE

/*  Define this if struct utmp have ut_user  */
#undef HAVE_UT_USER

/*  Define this if struct utmp have ut_host  */
#undef HAVE_UT_HOST

/*  Define this if struct utmp have ut_addr  */
#undef HAVE_UT_ADDR

/*  Define this if struct utmp have ut_type  */
#undef HAVE_UT_TYPE

/*  Define this if struct utmp have ut_pid  */
#undef HAVE_UT_PID

/*  Define this if struct utmp have ut_id  */
#undef HAVE_UT_ID

/*  Define this if struct utmpx have ut_syslen  */
#undef HAVE_UT_SYSLEN

/*  Define this if struct winsize is declared in sys/termios.h */
#undef HAVE_STRUCT_WINSIZE

/*  Define this if struct winsize have ws_xpixel */
#undef HAVE_WS_XPIXEL

/*  Define this if struct winsize have ws_ypixel */
#undef HAVE_WS_YPIXEL

/*  Define this to be the directory where the dictionary for cracklib */
/*  resides */
#undef DICTPATH

/* Define this if you want to use SOCKS v5 */
#undef SOCKS

/* Define this to the path of the mail spool directory */
#undef KRB4_MAILDIR

/* Define this if `struct sockaddr' includes sa_len */
#undef SOCKADDR_HAS_SA_LEN

/* Define this if `struct siaentity' includes ouid */
#undef SIAENTITY_HAS_OUID

/* Define if getlogin has POSIX flavour, as opposed to BSD */
#undef POSIX_GETLOGIN

/* Define if getpwnam_r has POSIX flavour */
#undef POSIX_GETPWNAM_R

/* define if getcwd() is broken (such as in SunOS) */
#undef BROKEN_GETCWD

/* define if the system is missing a prototype for crypt() */
#undef NEED_CRYPT_PROTO

/* define if the system is missing a prototype for strtok_r() */
#undef NEED_STRTOK_R_PROTO

/* define if /bin/ls takes -A */
#undef HAVE_LS_A

/* define if you have h_errno */
#undef HAVE_H_ERRNO

/* define if you have h_errlist but not hstrerror */
#undef HAVE_H_ERRLIST

/* define if you have h_nerr but not hstrerror */
#undef HAVE_H_NERR

/* define if your system doesn't declare h_errlist */
#undef HAVE_H_ERRLIST_DECLARATION

/* define if your system doesn't declare h_nerr */
#undef HAVE_H_NERR_DECLARATION

/* define this if you need a declaration for h_errno */
#undef HAVE_H_ERRNO_DECLARATION

/* define if you need a declaration for optarg */
#undef HAVE_OPTARG_DECLARATION

/* define if you need a declaration for optind */
#undef HAVE_OPTIND_DECLARATION

/* define if you need a declaration for opterr */
#undef HAVE_OPTERR_DECLARATION

/* define if you need a declaration for optopt */
#undef HAVE_OPTOPT_DECLARATION

/* define if you need a declaration for __progname */
#undef HAVE___PROGNAME_DECLARATION

@BOTTOM@

#undef HAVE_INT8_T
#undef HAVE_INT16_T
#undef HAVE_INT32_T
#undef HAVE_INT64_T
#undef HAVE_U_INT8_T
#undef HAVE_U_INT16_T
#undef HAVE_U_INT32_T
#undef HAVE_U_INT64_T

#define RCSID(msg) \
static /**/const char *const rcsid[] = { (char *)rcsid, "\100(#)" msg }

/*
 * Set ORGANIZATION to be the desired organization string printed
 * by the 'kinit' program.  It may have spaces.
 */
#define ORGANIZATION "eBones International"

#if 0
#undef BINDIR 
#undef LIBDIR
#undef LIBEXECDIR
#undef SBINDIR
#endif

#if 0
#define KRB_CNF_FILES	{ "/etc/krb.conf",   "/etc/kerberosIV/krb.conf", 0}
#define KRB_RLM_FILES	{ "/etc/krb.realms", "/etc/kerberosIV/krb.realms", 0}
#define KRB_EQUIV	"/etc/krb.equiv"

#define KEYFILE		"/etc/srvtab"

#define KRBDIR		"/var/kerberos"
#define DBM_FILE	KRBDIR "/principal"
#define DEFAULT_ACL_DIR	KRBDIR

#define KRBLOG		"/var/log/kerberos.log"	/* master server  */
#define KRBSLAVELOG	"/var/log/kerberos_slave.log" /* slave server  */
#define KADM_SYSLOG	"/var/log/admin_server.syslog"
#define K_LOGFIL	"/var/log/kpropd.log"
#endif

/* Maximum values on all known systems */
#define MaxHostNameLen (64+4)
#define MaxPathLen (1024+4)

/*
 * Define NDBM if you are using the 4.3 ndbm library (which is part of
 * libc).  If not defined, 4.2 dbm will be assumed.
 */
#if defined(HAVE_DBM_FIRSTKEY)
#define NDBM
#endif

/* ftp stuff -------------------------------------------------- */

#define KERBEROS

/* telnet stuff ----------------------------------------------- */

/* define this if you have kerberos 4 */
#undef KRB4

/* define this if you want encryption */
#undef ENCRYPTION

/* define this if you want authentication */
#undef AUTHENTICATION

#if defined(ENCRYPTION) && !defined(AUTHENTICATION)
#define AUTHENTICATION 1
#endif

/* Set this if you want des encryption */
#undef DES_ENCRYPTION

/* Set this to the default system lead string for telnetd 
 * can contain %-escapes: %s=sysname, %m=machine, %r=os-release
 * %v=os-version, %t=tty, %h=hostname, %d=date and time
 */
#undef USE_IM

/* define this if you want diagnostics in telnetd */
#undef DIAGNOSTICS

/* define this if you want support for broken ENV_{VALUE,VAR} systems  */
#undef ENV_HACK

/*  */
#undef OLD_ENVIRON

/* Used with login -p */
#undef LOGIN_ARGS

/* Define if there are working stream ptys */
#undef STREAMSPTY

/* set this to a sensible login */
#ifndef LOGIN_PATH
#define LOGIN_PATH BINDIR "/login"
#endif


/* ------------------------------------------------------------ */

/*
 * Define this if your ndbm-library really is berkeley db and creates
 * files that ends in .db.
 */
#undef HAVE_NEW_DB

/* Define this if you have a working getmsg */
#undef HAVE_GETMSG

/* Define to enable new master key code */
#undef RANDOM_MKEY

/* Location of the master key file, default value lives in <kdc.h> */
#undef MKEYFILE

/* Define if you don't want support for afs, might be a good idea on
   AIX if you don't have afs */
#undef NO_AFS

/* Define if you have a readline compatible library */
#undef HAVE_READLINE

#ifdef VOID_RETSIGTYPE
#define SIGRETURN(x) return
#else
#define SIGRETURN(x) return (RETSIGTYPE)(x)
#endif

/* Define this if your compiler supports '#pragma weak' */
#undef HAVE_PRAGMA_WEAK

/* Temporary fixes for krb_{rd,mk}_safe */
#define DES_QUAD_GUESS 0
#define DES_QUAD_NEW 1
#define DES_QUAD_OLD 2

/* Set this to one of the constants above to specify default checksum
   type to emit */
#undef DES_QUAD_DEFAULT

/*
 * AIX braindamage!
 */
#if _AIX
#define _ALL_SOURCE
#define _POSIX_SOURCE
/* this is left for hysteric reasons :-) */
#define unix /* well, ok... */
#endif

/*
 * SunOS braindamage! (Sun include files are generally braindead)
 */
#if (defined(sun) || defined(__sun))
#if defined(__svr4__) || defined(__SVR4)
#define SunOS 5
#else
#define SunOS 4
#endif
#endif

#if defined(__sgi) || defined(sgi)
#if defined(__SYSTYPE_SVR4) || defined(_SYSTYPE_SVR4)
#define IRIX 5
#else
#define IRIX 4
#endif
#endif

/* IRIX 4 braindamage */
#if IRIX == 4 && !defined(__STDC__)
#define __STDC__ 0
#endif
