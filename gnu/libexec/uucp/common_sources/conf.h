/* conf.h.  Generated automatically by configure.  */
/* Configuration header file for Taylor UUCP.  -*- C -*-  */

/* Set MAIL_PROGRAM to a program which takes a mail address as an
   argument and accepts a mail message to send to that address on
   stdin (e.g. "/bin/mail").  */
#define MAIL_PROGRAM "/usr/bin/mail"

/* Set ECHO_PROGRAM to a program which echoes its arguments; if echo
   is a shell builtin you can just use "echo".  */
#define ECHO_PROGRAM "echo"

/* The following macros indicate what header files you have.  Set the
   macro to 1 if you have the corresponding header file, or 0 if you
   do not.  */
#define HAVE_STDDEF_H 1 /* <stddef.h> */
#define HAVE_STRING_H 1 /* <string.h> */
#define HAVE_STRINGS_H 1 /* <strings.h> */
#define HAVE_UNISTD_H 1 /* <unistd.h> */
#define HAVE_STDLIB_H 1 /* <stdlib.h> */
#define HAVE_LIMITS_H 1 /* <limits.h> */
#define HAVE_TIME_H 1 /* <time.h> */
#define HAVE_SYS_WAIT_H 1 /* <sys/wait.h> */
#define HAVE_SYS_IOCTL_H 1 /* <sys/ioctl.h> */
#define HAVE_DIRENT_H 1 /* <dirent.h> */
#define HAVE_MEMORY_H 1 /* <memory.h> */
#define HAVE_SYS_PARAM_H 1 /* <sys/param.h> */
#define HAVE_UTIME_H 1 /* <utime.h> */
#define HAVE_FCNTL_H 1 /* <fcntl.h> */
#define HAVE_SYS_FILE_H 1 /* <sys/file.h> */
#define HAVE_SYS_TIMES_H 1 /* <sys/times.h> */
#define HAVE_LIBC_H 0 /* <libc.h> */
#define HAVE_SYSEXITS_H 1 /* <sysexits.h> */
#define HAVE_POLL_H 0 /* <poll.h> */
#define HAVE_TIUSER_H 0 /* <tiuser.h> */
#define HAVE_XTI_H 0 /* <xti.h> */
#define HAVE_SYS_TLI_H 0 /* <sys/tli.h> */
#define HAVE_STROPTS_H 0 /* <stropts.h> */
#define HAVE_FTW_H 0 /* <ftw.h> */
#define HAVE_GLOB_H 1 /* <glob.h> */
#define HAVE_SYS_SELECT_H 0 /* <sys/select.h> */
#define HAVE_SYS_TYPES_TCP_H 0 /* <sys/types.tcp.h> */

/* If major and minor are not defined in <sys/types.h>, but are in
   <sys/mkdev.h>, set MAJOR_IN_MKDEV to 1.  If they are in
   <sys/sysmacros.h>, set MAJOR_IN_SYSMACROS to 1.  */
#define MAJOR_IN_MKDEV 0
#define MAJOR_IN_SYSMACROS 0

/* If the macro offsetof is not defined in <stddef.h>, you may give it
   a definition here.  If you do not, the code will use a definition
   (in uucp.h) that should be fairly portable.  */
/* #define offsetof */

/* Set RETSIGTYPE to the return type of a signal handler.  On newer
   systems this will be void; some older systems use int.  */
#define RETSIGTYPE void

/* Set HAVE_SYS_TIME_AND_TIME_H to 1 if <time.h> and <sys/time.h> can both
   be included in a single source file; if you don't have either or both of
   them, it doesn't matter what you set this to.  */
#define HAVE_SYS_TIME_AND_TIME_H 1

/* Set HAVE_TERMIOS_AND_SYS_IOCTL_H to 1 if <termios.h> and <sys/ioctl.h>
   can both be included in a single source file; if you don't have either
   or both of them, it doesn't matter what you set this to.  */
#define HAVE_TERMIOS_AND_SYS_IOCTL_H 1

/* If you are configuring by hand, you should set one of the terminal
   driver options in policy.h.  If you are autoconfiguring, the script
   will check whether your system defines CBREAK, which is a terminal
   setting; if your system supports CBREAK, and you don't set a terminal
   driver in policy.h, the code will assume that you have a BSD style
   terminal driver.  */
#define HAVE_CBREAK 1

/* The package needs several standard types.  If you are using the
   configure script, it will look in standard places for these types,
   and give default definitions for them here if it doesn't find them.
   The default definitions should work on most systems, but you may
   want to check them.  If you are configuring by hand, you will have
   to figure out whether the types are defined on your system, and
   what they should be defined to.

   Any type that is not defined on your system should get a macro
   definition.  The definition should be of the name of the type in
   all capital letters.  For example, #define PID_T int.  If the type
   is defined in a standard header file, the macro name should not be
   defined.  */

/* The type pid_t is used to hold a process ID number.  It is normally
   defined in <sys/types.h>.  This is the type returned by the
   functions fork or getpid.  Usually int will work fine.  */
#undef PID_T

/* The type uid_t is used to hold a user ID number.  It is normally
   defined in <sys/types.h>.  This is the type returned by the getuid
   function.  Usually int will work fine.  */
#undef UID_T

/* The type gid_t is used to hold a group ID number.  It is sometimes
   defined in <sys/types.h>.  This is the type returned by the getgid
   function.  Usually int will work fine.  */
#undef GID_T

/* The type off_t is used to hold an offset in a file.  It is sometimes
   defined in <sys/types.h>.  This is the type of the second argument to
   the lseek function.  Usually long will work fine.  */
#undef OFF_T

/* Set HAVE_SIG_ATOMIC_T_IN_SIGNAL_H if the type sig_atomic_t is defined
   in <signal.h> as required by ANSI C.  */
#define HAVE_SIG_ATOMIC_T_IN_SIGNAL_H 0

/* Set HAVE_SIG_ATOMIC_T_IN_TYPES_H if the type sig_atomic_t is defined
   in <sys/types.h>.  This is ignored if HAVE_SIG_ATOMIC_T_IN_SIGNAL_H is
   set to 1.  */
#define HAVE_SIG_ATOMIC_T_IN_TYPES_H 0

/* The type sig_atomic_t is used to hold a value which may be
   referenced in a single atomic operation.  If it is not defined in
   either <signal.h> or <sys/types.h>, you may want to give it a
   definition here.  If you don't, the code will use char.  If your
   compiler does not support sig_atomic_t, there is no type which is
   really correct; fortunately, for this package it does not really
   matter very much.  */
#undef SIG_ATOMIC_T

/* Set HAVE_SIZE_T_IN_STDDEF_H to 1 if the type size_t is defined in
   <stddef.h> as required by ANSI C.  */
#define HAVE_SIZE_T_IN_STDDEF_H 1

/* Set HAVE_SIZE_T_IN_TYPES_H to 1 if the type size_t is defined in
   <sys/types.h>.  This is ignored if HAVE_SIZE_T_IN_STDDEF_H is set
   to 1.  */
#define HAVE_SIZE_T_IN_TYPES_H 1

/* The type size_t is used to hold the size of an object.  In
   particular, an argument of this type is passed as the size argument
   to the malloc and realloc functions.  If size_t is not defined in
   either <stddef.h> or <sys/types.h>, you may want to give it a
   definition here.  If you don't, the code will use unsigned.  */
#undef SIZE_T

/* Set HAVE_TIME_T_IN_TIME_H to 1 if the type time_t is defined in
   <time.h>, as required by the ANSI C standard.  */
#define HAVE_TIME_T_IN_TIME_H 1

/* Set HAVE_TIME_T_IN_TYPES_H to 1 if the type time_t is defined in
   <sys/types.h>.  This is ignored if HAVE_TIME_T_IN_TIME_H is set to
   1.  */
#define HAVE_TIME_T_IN_TYPES_H 1

/* When Taylor UUCP is talking to another instance of itself, it will
   tell the other side the size of a file before it is transferred.
   If the package can determine how much disk space is available, it
   will use this information to avoid filling up the disk.  Define one
   of the following macros to tell the code how to determine the
   amount of available disk space.  It is possible that none of these
   are appropriate; it will do no harm to use none of them, but, of
   course, nothing will then prevent the package from filling up the
   disk.  Note that this space check is only useful when talking to
   another instance of Taylor UUCP.

   STAT_STATVFS          statvfs function
   STAT_STATFS2_BSIZE    two argument statfs function with f_bsize field
   STAT_STATFS2_FSIZE    two argument statfs function with f_fsize field
   STAT_STATFS2_FS_DATA  two argument statfs function with fd_req field
   STAT_STATFS4          four argument statfs function
   STAT_USTAT            the ustat function with 512 byte blocks.  */
#define STAT_STATVFS 0
#define STAT_STATFS2_BSIZE 0
#define STAT_STATFS2_FSIZE 1
#define STAT_STATFS2_FS_DATA 0
#define STAT_STATFS4 0
#define STAT_USTAT 0

/* Set HAVE_VOID to 1 if the compiler supports declaring functions with
   a return type of void and casting values to void.  */
#define HAVE_VOID 1

/* Set HAVE_UNSIGNED_CHAR to 1 if the compiler supports the type unsigned
   char.  */
#define HAVE_UNSIGNED_CHAR 1

/* Set HAVE_ERRNO_DECLARATION to 1 if errno is declared in <errno.h>.  */
#define HAVE_ERRNO_DECLARATION 1

/* There are now a number of functions to check for.  For each of
   these, the macro HAVE_FUNC should be set to 1 if your system has
   FUNC.  For example, HAVE_VFPRINTF should be set to 1 if your system
   has vfprintf, 0 otherwise.  */

/* Taylor UUCP will take advantage of the following functions if they
   are available, but knows how to deal with their absence.  */
#define HAVE_VFPRINTF 1
#define HAVE_FTRUNCATE 1
#define HAVE_LTRUNC 0
#define HAVE_WAITPID 1
#define HAVE_WAIT4 1
#define HAVE_GLOB 1
#define HAVE_SETREUID 1

/* There are several functions which are replaced in the subdirectory
   lib.  If they are missing, the configure script will automatically
   add them to lib/Makefile to force them to be recompiled.  If you
   are configuring by hand, you will have to do this yourself.  The
   string @LIBOBJS@ in lib/Makefile.in should be replaced by a list of
   object files in lib/Makefile.  The following comments tell you
   which object file names to add (they are generally fairly obvious,
   given that the file names have no more than six characters before
   the period).  */

/* For each of these functions, if it does not exist, the indicated
   object file should be added to lib/Makefile.  */
#define HAVE_BSEARCH 1 /* bsrch.o */
#define HAVE_GETLINE 0 /* getlin.o */
#define HAVE_MEMCHR 1 /* memchr.o */
#define HAVE_STRDUP 1 /* strdup.o */
#define HAVE_STRSTR 1 /* strstr.o */
#define HAVE_STRTOL 1 /* strtol.o */

/* If neither of these functions exists, you should add bzero.o to
   lib/Makefile.  */
#define HAVE_BZERO 1
#define HAVE_MEMSET 1

/* If neither of these functions exists, you should add memcmp.o to
   lib/Makefile.  */
#define HAVE_MEMCMP 1
#define HAVE_BCMP 1

/* If neither of these functions exists, you should add memcpy.o to
   lib/Makefile.  */
#define HAVE_MEMCPY 1
#define HAVE_BCOPY 1

/* If neither of these functions exists, you should add strcas.o to
   lib/Makefile.  */
#define HAVE_STRCASECMP 1
#define HAVE_STRICMP 0

/* If neither of these functions exists, you should add strncs.o to
   lib/Makefile.  */
#define HAVE_STRNCASECMP 1
#define HAVE_STRNICMP 0

/* If neither of these functions exists, you should add strchr.o to
   lib/Makefile.  */
#define HAVE_STRCHR 1
#define HAVE_INDEX 1

/* If neither of these functions exists, you should add strrch.o to
   lib/Makefile.  */
#define HAVE_STRRCHR 1
#define HAVE_RINDEX 1

/* There are also Unix specific functions which are replaced in the
   subdirectory unix.  If they are missing, the configure script will
   automatically add them to unix/Makefile to force them to be
   recompiled.  If you are configuring by hand, you will have to do
   this yourself.  The string @UNIXOBJS@ in unix/Makefile.in should be
   replaced by a list of object files in unix/Makefile.  The following
   comments tell you which object file names to add.  */

/* For each of these functions, if it does not exist, the indicated
   object file should be added to unix/Makefile.  */
#define HAVE_OPENDIR 1 /* dirent.o */
#define HAVE_DUP2 1 /* dup2.o */
#define HAVE_FTW 0 /* ftw.o */
#define HAVE_REMOVE 1 /* remove.o */
#define HAVE_RENAME 1 /* rename.o */
#define HAVE_STRERROR 1 /* strerr.o */

/* The code needs to know how to create directories.  If you have the
   mkdir function, set HAVE_MKDIR to 1 and replace @UUDIR@ in
   Makefile.in with '# ' (the configure script will set @UUDIR@
   according to the variable UUDIR).  Otherwise, set HAVE_MKDIR to 0,
   remove @UUDIR@ from Makefile.in, set MKDIR_PROGRAM to the name of
   the program which will create a directory named on the command line
   (e.g., "/bin/mkdir"), and add mkdir.o to the @UNIXOBJS@ string in
   unix/Makefile.in.  */
#define HAVE_MKDIR 1
#define MKDIR_PROGRAM unused

/* The code also needs to know how to remove directories.  If you have
   the rmdir function, set HAVE_RMDIR to 1.  Otherwise, set
   RMDIR_PROGRAM to the name of the program which will remove a
   directory named on the command line (e.g., "/bin/rmdir") and add
   rmdir.o to the @UNIXOBJS@ string in unix/Makefile.in.  */
#define HAVE_RMDIR 1
#define RMDIR_PROGRAM unused

/* The code needs to know to how to get the name of the current
   directory.  If getcwd is available it will be used, otherwise if
   getwd is available it will be used.  Otherwise, set PWD_PROGRAM to
   the name of the program which will print the name of the current
   working directory (e.g., "/bin/pwd") and add getcwd.o to the
   @UNIXOBJS@ string in unix/Makefile.in.  */
#define HAVE_GETCWD 1
#define HAVE_GETWD 1
#define PWD_PROGRAM unused

/* If you have either sigsetjmp or setret, it will be used instead of
   setjmp.  These functions will only be used if your system restarts
   system calls after interrupts (see HAVE_RESTARTABLE_SYSCALLS,
   below).  */
#define HAVE_SIGSETJMP 0
#define HAVE_SETRET 0

/* The code needs to know what function to use to set a signal
   handler.  If will try to use each of the following functions in
   turn.  If none are available, it will use signal, which is assumed
   to always exist.  */
#define HAVE_SIGACTION 1
#define HAVE_SIGVEC 1
#define HAVE_SIGSET 0

/* If the code is going to use sigvec (HAVE_SIGACTION is 0 and
   HAVE_SIGVEC is 1), then HAVE_SIGVEC_SV_FLAGS must be set to 1 if
   the sigvec structure contains the sv_flags field, or 0 if the
   sigvec structure contains the sv_onstack field.  If the code is not
   going to use sigvec, it doesn't matter what this is set to.  */
#define HAVE_SIGVEC_SV_FLAGS 1

/* The code will try to use each of the following functions in turn
   when blocking signals from delivery.  If none are available, a
   relatively unimportant race condition will exist.  */
#define HAVE_SIGPROCMASK 1
#define HAVE_SIGBLOCK 1
#define HAVE_SIGHOLD 0

/* If you have either of the following functions, it will be used to
   determine the number of file descriptors which may be open.
   Otherwise, the code will use OPEN_MAX if defined, then NOFILE if
   defined, then 20.  */
#define HAVE_GETDTABLESIZE 1
#define HAVE_SYSCONF 0

/* The code will use one of the following functions when detaching
   from a terminal.  One of these must exist.  */
#define HAVE_SETPGRP 1
#define HAVE_SETSID 1

/* If you do not specify the local node name in the main configuration
   file, Taylor UUCP will try to use each of the following functions
   in turn.  If neither is available, you must specify the local node
   name in the configuration file.  */
#define HAVE_GETHOSTNAME 1
#define HAVE_UNAME 0

/* The code will try to use each of the following functions in turn to
   determine the current time.  If none are available, it will use
   time, which is assumed to always exist.  */
#define HAVE_GETTIMEOFDAY 1
#define HAVE_FTIME 0

/* If neither gettimeofday nor ftime is available, the code will use
   times (if available) to measure a span of time.  See also the
   discussion of TIMES_TICK in policy.h.  */
#define HAVE_TIMES 1

/* When a chat script requests a pause of less than a second with \p,
   Taylor UUCP will try to use each of the following functions in
   turn.  If none are available, it will sleep for a full second.
   Also, the (non-portable) tstuu program requires either select or
   poll.  */
#define HAVE_NAPMS 0
#define HAVE_NAP 0
#define HAVE_USLEEP 1
#define HAVE_POLL 0
#define HAVE_SELECT 1

/* If the getgrent function is available, it will be used to determine
   all the groups a user belongs to when checking file access
   permissions.  */
#define HAVE_GETGRENT 1

/* If the socket function is available, TCP support code will be
   compiled in.  */
#define HAVE_SOCKET 1

/* If the t_open function is available, TLI support code will be
   compiled in.  This may require adding a library, such as -lnsl or
   -lxti, to the Makefile variables LIBS.  */
#define HAVE_T_OPEN 0

/* That's the end of the list of the functions.  Now there are a few
   last miscellaneous items.  */

/* On some systems the following functions are declared in such a way
   that the code cannot make a simple extern.  On other systems, these
   functions are not declared at all, and the extern is required.  If
   a declaration of the function, as shown, compiles on your system,
   set the value to 1.  Not all functions declared externally are
   listed here, only the ones with which I have had trouble.  */
/* extern long times (); */
#define TIMES_DECLARATION_OK 0
/* extern struct passwd *getpwnam (); */
#define GETPWNAM_DECLARATION_OK 1
/* extern struct passwd *getpwuid (); */
#define GETPWUID_DECLARATION_OK 0
/* extern struct group *getgrent (); */
#define GETGRENT_DECLARATION_OK 1

/* Set HAVE_BSD_PGRP to 1 if your getpgrp call takes 1 argument and
   your setpgrp calls takes 2 arguments (on System V they generally
   take no arguments).  You can safely set this to 1 on System V,
   provided the call will compile without any errors.  */
#define HAVE_BSD_PGRP 0

/* Set HAVE_UNION_WAIT to 1 if union wait is defined in the header
   file <sys/wait.h>.  */
#define HAVE_UNION_WAIT 1

/* Set HAVE_LONG_FILE_NAMES to 1 if the system supports file names
   longer than 14 characters.  */
#define HAVE_LONG_FILE_NAMES 1

/* If slow system calls are restarted after interrupts, set
   HAVE_RESTARTABLE_SYSCALLS to 1.  This is ignored if HAVE_SIGACTION
   is 1 or if HAVE_SIGVEC is 1 and HAVE_SIGVEC_SV_FLAGS is 1 and
   SV_INTERRUPT is defined in <signal.h>.  In both of these cases
   system calls can be prevented from restarting.  */
#define HAVE_RESTARTABLE_SYSCALLS 1

/* Some systems supposedly need the following macros to be defined.
   These are handled by the configure script (it will turn #undef into
   #define when appropriate, which is why the peculiar #ifndef #undef
   construction is used).  If you are configuring by hand, you may add
   appropriate definitions here, or just add them to CFLAGS when
   running make.  */
#ifndef _ALL_SOURCE
#undef _ALL_SOURCE
#endif
#ifndef _POSIX_SOURCE
#undef _POSIX_SOURCE
#endif
#ifndef _MINIX
#undef _MINIX
#endif
#ifndef _POSIX_1_SOURCE
#undef _POSIX_1_SOURCE
#endif
