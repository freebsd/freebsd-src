/* $FreeBSD$ */
/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

/* Define if the closedir function returns void instead of int.  */
/* #undef CLOSEDIR_VOID */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef gid_t */

/* Define if your system has a working fnmatch function.  */
#define HAVE_FNMATCH 1

/* Define if you support file names longer than 14 characters.  */
#define HAVE_LONG_FILE_NAMES 1

/* Define if your struct stat has st_blksize.  */
#define HAVE_ST_BLKSIZE 1

/* Define if your struct stat has st_rdev.  */
#define HAVE_ST_RDEV 1

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#define HAVE_SYS_WAIT_H 1

/* Define if utime(file, NULL) sets file's timestamp to the present.  */
#define HAVE_UTIME_NULL 1

/* Define if you have <vfork.h>.  */
/* #undef HAVE_VFORK_H */

/* Define if on MINIX.  */
/* #undef _MINIX */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef mode_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef pid_t */

/* Define if the system does not provide POSIX.1 features except
   with this defined.  */
/* #undef _POSIX_1_SOURCE */

/* Define if you need to in order for stat and other things to work.  */
/* #undef _POSIX_SOURCE */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if the `S_IS*' macros in <sys/stat.h> do not work properly.  */
/* #undef STAT_MACROS_BROKEN */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef uid_t */

/* Define vfork as fork if vfork does not work.  */
/* #undef vfork */

/* Define if you have MIT Kerberos version 4 available.  */
/* #undef HAVE_KERBEROS */

/* Define if you have GSSAPI with MIT Kerberos version 5 available.  */
/* #undef HAVE_GSSAPI */

/* Define if GSS_C_NT_HOSTBASED_SERVICE is defined in the gssapi.h
   header file.  Only relevant when using GSSAPI.  */
/* #undef HAVE_GSS_C_NT_HOSTBASED_SERVICE */

/* Define if you want CVS to be able to be a remote repository client.  */
#define CLIENT_SUPPORT 1

/* Define if you want CVS to be able to serve repositories to remote
   clients.  */
#define SERVER_SUPPORT 1

/* Define if you want to use the password authenticated server.  */
#define AUTH_SERVER_SUPPORT 1

/* Define if you want encryption support.  */
/* #undef ENCRYPTION */

/* Define if you have the connect function.  */
#define HAVE_CONNECT 1

/* Define if you have memchr (always for CVS).  */
#define HAVE_MEMCHR 1

/* Define if you have strchr (always for CVS).  */
#define HAVE_STRCHR 1

/* Define if utime requires write access to the file (true on Windows,
   but not Unix).  */
/* #undef UTIME_EXPECTS_WRITABLE */

/* Define if setmode is required when writing binary data to stdout.  */
/* #undef USE_SETMODE_STDOUT */

/* Define if the diff library should use setmode for binary files.
   FIXME: Why two different macros for setmode?  */
/* #undef HAVE_SETMODE */

/* Define if you have the crypt function.  */
#define HAVE_CRYPT 1

/* Define if you have the getspnam function.  */
/* #undef HAVE_GETSPNAM */

/* Define to force lib/regex.c to use malloc instead of alloca.  */
#define REGEX_MALLOC 1

/* Define to force lib/regex.c to define re_comp et al.  */
#define _REGEX_RE_COMP 1

/* Define if you have the dup2 function.  */
#define HAVE_DUP2 1

/* Define if you have the fchdir function.  */
#define HAVE_FCHDIR 1

/* Define if you have the fchmod function.  */
#define HAVE_FCHMOD 1

/* Define if you have the fsync function.  */
#define HAVE_FSYNC 1

/* Define if you have the ftime function.  */
/* #undef HAVE_FTIME */

/* Define if you have the ftruncate function.  */
#define HAVE_FTRUNCATE 1

/* Define if you have the getpagesize function.  */
#define HAVE_GETPAGESIZE 1

/* Define if you have the getpassphrase function.  */
/* #undef HAVE_GETPASSPHRASE */

/* Define if you have the initgroups function.  */
#define HAVE_INITGROUPS 1

/* Define if you have the krb_get_err_text function.  */
/* #undef HAVE_KRB_GET_ERR_TEXT */

/* Define if you have the memmove function.  */
#define HAVE_MEMMOVE 1

/* Define if you have the mkdir function.  */
#define HAVE_MKDIR 1

/* Define if you have the mknod function.  */
#define HAVE_MKNOD 1

/* Define if you have the mktemp function.  */
#define HAVE_MKTEMP 1

/* Define if you have the putenv function.  */
#define HAVE_PUTENV 1

/* Define if you have the readlink function.  */
#define HAVE_READLINK 1

/* Define if you have the rename function.  */
#define HAVE_RENAME 1

/* Define if you have the sigaction function.  */
#define HAVE_SIGACTION 1

/* Define if you have the sigblock function.  */
#define HAVE_SIGBLOCK 1

/* Define if you have the sigprocmask function.  */
#define HAVE_SIGPROCMASK 1

/* Define if you have the sigsetmask function.  */
#define HAVE_SIGSETMASK 1

/* Define if you have the sigvec function.  */
#define HAVE_SIGVEC 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strstr function.  */
#define HAVE_STRSTR 1

/* Define if you have the strtoul function.  */
#define HAVE_STRTOUL 1

/* Define if you have the tempnam function.  */
#define HAVE_TEMPNAM 1

/* Define if you have the timezone function.  */
#define HAVE_TIMEZONE 1

/* Define if you have the tzset function.  */
#define HAVE_TZSET 1

/* Define if you have the valloc function.  */
#define HAVE_VALLOC 1

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define if you have the wait3 function.  */
#define HAVE_WAIT3 1

/* Define if you have the waitpid function.  */
#define HAVE_WAITPID 1

/* Define if you have the <direct.h> header file.  */
/* #undef HAVE_DIRECT_H */

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <gssapi.h> header file.  */
/* #undef HAVE_GSSAPI_H */

/* Define if you have the <gssapi/gssapi.h> header file.  */
/* #undef HAVE_GSSAPI_GSSAPI_H */

/* Define if you have the <gssapi/gssapi_generic.h> header file.  */
/* #undef HAVE_GSSAPI_GSSAPI_GENERIC_H */

/* Define if you have the <io.h> header file.  */
/* #undef HAVE_IO_H */

/* Define if you have the <krb5.h> header file.  */
/* #undef HAVE_KRB5_H */

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <ndbm.h> header file.  */
#define HAVE_NDBM_H 1

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <sys/bsdtypes.h> header file.  */
/* #undef HAVE_SYS_BSDTYPES_H */

/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/file.h> header file.  */
#define HAVE_SYS_FILE_H 1

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/resource.h> header file.  */
#define HAVE_SYS_RESOURCE_H 1

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/timeb.h> header file.  */
#define HAVE_SYS_TIMEB_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <utime.h> header file.  */
#define HAVE_UTIME_H 1

/* Define if you have the gen library (-lgen).  */
/* #undef HAVE_LIBGEN */
