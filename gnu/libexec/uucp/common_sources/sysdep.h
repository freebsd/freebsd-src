/* sysh.unx -*- C -*-
   The header file for the UNIX system dependent routines.

   Copyright (C) 1991, 1992, 1993 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#ifndef SYSH_UNX_H

#define SYSH_UNX_H

#if ANSI_C
/* These structures are used in prototypes but are not defined in this
   header file.  */
struct uuconf_system;
struct sconnection;
#endif

/* SCO, SVR4 and Sequent lockfiles are basically just like HDB
   lockfiles.  */
#if HAVE_SCO_LOCKFILES || HAVE_SVR4_LOCKFILES || HAVE_SEQUENT_LOCKFILES
#undef HAVE_HDB_LOCKFILES
#define HAVE_HDB_LOCKFILES 1
#endif

#if HAVE_BSD_TTY + HAVE_SYSV_TERMIO + HAVE_POSIX_TERMIOS != 1
 #error Terminal driver define not set or duplicated
#endif

#if SPOOLDIR_V2 + SPOOLDIR_BSD42 + SPOOLDIR_BSD43 + SPOOLDIR_HDB + SPOOLDIR_ULTRIX + SPOOLDIR_SVR4 + SPOOLDIR_TAYLOR != 1
 #error Spool directory define not set or duplicated
#endif

/* If setreuid is broken, don't use it.  */
#if HAVE_BROKEN_SETREUID
#undef HAVE_SETREUID
#define HAVE_SETREUID 0
#endif

/* Get some standard types from the configuration header file.  */
#ifdef PID_T
typedef PID_T pid_t;
#endif

#ifdef UID_T
typedef UID_T uid_t;
#endif

#ifdef GID_T
typedef GID_T gid_t;
#endif

#ifdef OFF_T
typedef OFF_T off_t;
#endif

/* On Unix, binary files are the same as text files.  */
#define BINREAD "r"
#define BINWRITE "w"

/* If we have sigaction, we can force system calls to not be
   restarted.  */
#if HAVE_SIGACTION
#undef HAVE_RESTARTABLE_SYSCALLS
#define HAVE_RESTARTABLE_SYSCALLS 0
#endif

/* If we have sigvec, and we have HAVE_SIGVEC_SV_FLAGS, and
   SV_INTERRUPT is defined, we can force system calls to not be
   restarted (signal.h is included by uucp.h before this point, so
   SV_INTERRUPT will be defined by now if it it ever is).  */
#if HAVE_SIGVEC && HAVE_SIGVEC_SV_FLAGS
#ifdef SV_INTERRUPT
#undef HAVE_RESTARTABLE_SYSCALLS
#define HAVE_RESTARTABLE_SYSCALLS 0
#endif
#endif

/* If we were cross-configured, we will have a value of -1 for
   HAVE_RESTARTABLE_SYSCALLS.  In this case, we try to guess what the
   correct value should be.  Yuck.  If we have sigvec, but neither of
   the above cases applied (which we know because they would have
   changed HAVE_RESTARTABLE_SYSCALLS) then we are probably on 4.2BSD
   and system calls are automatically restarted.  Otherwise, assume
   that they are not.  */
#if HAVE_RESTARTABLE_SYSCALLS == -1
#undef HAVE_RESTARTABLE_SYSCALLS
#if HAVE_SIGVEC
#define HAVE_RESTARTABLE_SYSCALLS 1
#else
#define HAVE_RESTARTABLE_SYSCALLS 0
#endif
#endif /* HAVE_RESTARTABLE_SYSCALLS == -1 */

/* We don't handle sigset in combination with restartable system
   calls, so we check for it although this combination will never
   happen.  */
#if ! HAVE_SIGACTION && ! HAVE_SIGVEC && HAVE_SIGSET
#if HAVE_RESTARTABLE_SYSCALLS
#undef HAVE_SIGSET
#define HAVE_SIGSET 0
#endif
#endif

/* If we don't have restartable system calls, we can ignore
   fsysdep_catch, usysdep_start_catch and usysdep_end_catch.
   Otherwise fsysdep_catch has to do a setjmp.  */

#if ! HAVE_RESTARTABLE_SYSCALLS

#define fsysdep_catch() (TRUE)
#define usysdep_start_catch()
#define usysdep_end_catch()
#define CATCH_PROTECT

#else /* HAVE_RESTARTABLE_SYSCALLS */

#if HAVE_SETRET && ! HAVE_SIGSETJMP
#include <setret.h>
#define setjmp setret
#define longjmp longret
#define jmp_buf ret_buf
#else /* ! HAVE_SETRET || HAVE_SIGSETJMP */
#include <setjmp.h>
#if HAVE_SIGSETJMP
#undef setjmp
#undef longjmp
#undef jmp_buf
#define setjmp(s) sigsetjmp ((s), TRUE)
#define longjmp siglongjmp
#define jmp_buf sigjmp_buf
#endif /* HAVE_SIGSETJMP */
#endif /* ! HAVE_SETRET || HAVE_SIGSETJMP */

extern volatile sig_atomic_t fSjmp;
extern volatile jmp_buf sSjmp_buf;

#define fsysdep_catch() (setjmp (sSjmp_buf) == 0)

#define usysdep_start_catch() (fSjmp = TRUE)

#define usysdep_end_catch() (fSjmp = FALSE)

#define CATCH_PROTECT volatile

#endif /* HAVE_RESTARTABLE_SYSCALLS */

/* Get definitions for the terminal driver.  */

#if HAVE_BSD_TTY
#include <sgtty.h>
struct sbsd_terminal
{
  struct sgttyb stty;
  struct tchars stchars;
  struct ltchars sltchars;
};
typedef struct sbsd_terminal sterminal;
#define fgetterminfo(o, q) \
  (ioctl ((o), TIOCGETP, &(q)->stty) == 0 \
   && ioctl ((o), TIOCGETC, &(q)->stchars) == 0 \
   && ioctl ((o), TIOCGLTC, &(q)->sltchars) == 0)
#define fsetterminfo(o, q) \
  (ioctl ((o), TIOCSETN, &(q)->stty) == 0 \
   && ioctl ((o), TIOCSETC, &(q)->stchars) == 0 \
   && ioctl ((o), TIOCSLTC, &(q)->sltchars) == 0)
#define fsetterminfodrain(o, q) \
  (ioctl ((o), TIOCSETP, &(q)->stty) == 0 \
   && ioctl ((o), TIOCSETC, &(q)->stchars) == 0 \
   && ioctl ((o), TIOCSLTC, &(q)->sltchars) == 0)
#endif /* HAVE_BSD_TTY */

#if HAVE_SYSV_TERMIO
#include <termio.h>
typedef struct termio sterminal;
#define fgetterminfo(o, q) (ioctl ((o), TCGETA, (q)) == 0)
#define fsetterminfo(o, q) (ioctl ((o), TCSETA, (q)) == 0)
#define fsetterminfodrain(o, q) (ioctl ((o), TCSETAW, (q)) == 0)
#endif /* HAVE_SYSV_TERMIO */

#if HAVE_POSIX_TERMIOS
#include <termios.h>
typedef struct termios sterminal;
#define fgetterminfo(o, q) (tcgetattr ((o), (q)) == 0)
#define fsetterminfo(o, q) (tcsetattr ((o), TCSANOW, (q)) == 0)
#define fsetterminfodrain(o, q) (tcsetattr ((o), TCSADRAIN, (q)) == 0)

/* On some systems it is not possible to include both <sys/ioctl.h>
   and <termios.h> in the same source files; I don't really know why.
   On such systems, we pretend that we don't have <sys/ioctl.h>.  */
#if ! HAVE_TERMIOS_AND_SYS_IOCTL_H
#undef HAVE_SYS_IOCTL_H
#define HAVE_SYS_IOCTL_H 0
#endif

#endif /* HAVE_POSIX_TERMIOS */

/* The root directory (this is needed by the system independent stuff
   as the default for local-send).  */
#define ZROOTDIR "/"

/* The name of the execution directory within the spool directory
   (this is need by the system independent uuxqt.c).  */
#define XQTDIR ".Xqtdir"

/* The name of the directory in which we preserve file transfers that
   failed.  */
#define PRESERVEDIR ".Preserve"

/* The name of the directory to which we move corrupt files.  */
#define CORRUPTDIR ".Corrupt"

/* The length of the sequence number used in a file name.  */
#define CSEQLEN (4)

/* Get some standard definitions.  Avoid including the files more than
   once--some might have been included by uucp.h.  */
#if USE_STDIO && HAVE_UNISTD_H
#include <unistd.h>
#endif
#if ! USE_TYPES_H
#include <sys/types.h>
#endif
#include <sys/stat.h>

/* Get definitions for the file permission bits.  */

#ifndef S_IRWXU
#define S_IRWXU 0700
#endif
#ifndef S_IRUSR
#define S_IRUSR 0400
#endif
#ifndef S_IWUSR
#define S_IWUSR 0200
#endif
#ifndef S_IXUSR
#define S_IXUSR 0100
#endif

#ifndef S_IRWXG
#define S_IRWXG 0070
#endif
#ifndef S_IRGRP
#define S_IRGRP 0040
#endif
#ifndef S_IWGRP
#define S_IWGRP 0020
#endif
#ifndef S_IXGRP
#define S_IXGRP 0010
#endif

#ifndef S_IRWXO
#define S_IRWXO 0007
#endif
#ifndef S_IROTH
#define S_IROTH 0004
#endif
#ifndef S_IWOTH
#define S_IWOTH 0002
#endif
#ifndef S_IXOTH
#define S_IXOTH 0001
#endif

#if STAT_MACROS_BROKEN
#undef S_ISDIR
#endif

#ifndef S_ISDIR
#ifdef S_IFDIR
#define S_ISDIR(i) (((i) & S_IFMT) == S_IFDIR)
#else /* ! defined (S_IFDIR) */
#define S_ISDIR(i) (((i) & 0170000) == 040000)
#endif /* ! defined (S_IFDIR) */
#endif /* ! defined (S_ISDIR) */

/* We need the access macros.  */
#ifndef R_OK
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0
#endif /* ! defined (R_OK) */

/* We create files with these modes (should this be configurable?).  */
#define IPRIVATE_FILE_MODE (S_IRUSR | S_IWUSR)
#define IPUBLIC_FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

/* We create directories with this mode (should this be configurable?).  */
#define IDIRECTORY_MODE (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)
#define IPUBLIC_DIRECTORY_MODE (S_IRWXU | S_IRWXG | S_IRWXO)

#if ! HAVE_OPENDIR

/* Define some structures to use if we don't have opendir, etc.  These
   will only work if we have the old Unix filesystem, with a 2 byte
   inode and a 14 byte filename.  */

#include <sys/dir.h>

struct dirent
{
  char d_name[DIRSIZ + 1];
};

typedef struct
{
  int o;
  struct dirent s;
} DIR;

extern DIR *opendir P((const char *zdir));
extern struct dirent *readdir P((DIR *));
extern int closedir P((DIR *));

#endif /* ! HAVE_OPENDIR */

#if ! HAVE_FTW_H

/* If there is no <ftw.h>, define the ftw constants.  */

#define FTW_F (0)
#define FTW_D (1)
#define FTW_DNR (2)
#define FTW_NS (3)

#endif /* ! HAVE_FTW_H */

/* This structure holds the system dependent information we keep for a
   connection.  This is used by the TCP and TLI code.  */

struct ssysdep_conn
{
  /* File descriptor.  */
  int o;
  /* File descriptor to read from (used by stdin and pipe port types).  */
  int ord;
  /* File descriptor to write to (used by stdin and pipe port types).  */
  int owr;
  /* Device name.  */
  char *zdevice;
  /* File status flags.  */
  int iflags;
  /* File status flags for write descriptor (-1 if not used).  */
  int iwr_flags;
  /* Hold the real descriptor when using a dialer device.  */
  int ohold;
  /* TRUE if this is a terminal and the remaining fields are valid.  */
  boolean fterminal;
  /* TRUE if this is a TLI descriptor.  */
  boolean ftli;
  /* Baud rate.  */
  long ibaud;
  /* Original terminal settings.  */
  sterminal sorig;
  /* Current terminal settings.  */
  sterminal snew;
  /* Process ID of currently executing pipe command, or parent process
     of forked TCP or TLI server, or -1.  */
  pid_t ipid;
#if HAVE_COHERENT_LOCKFILES
  /* On Coherent we need to hold on to the real port name which will
     be used to enable the port.  Ick.  */
  char *zenable;
#endif
};

/* These functions do I/O and chat scripts to a port.  They are called
   by the TCP and TLI routines.  */
extern boolean fsysdep_conn_read P((struct sconnection *qconn,
				    char *zbuf, size_t *pclen,
				    size_t cmin, int ctimeout,
				    boolean freport));
extern boolean fsysdep_conn_write P((struct sconnection *qconn,
				     const char *zbuf, size_t clen));
extern boolean fsysdep_conn_io P((struct sconnection *qconn,
				  const char *zwrite, size_t *pcwrite,
				  char *zread, size_t *pcread));
extern boolean fsysdep_conn_chat P((struct sconnection *qconn,
				    char **pzprog));

/* Set a signal handler.  */
extern void usset_signal P((int isig, RETSIGTYPE (*pfn) P((int)),
			    boolean fforce, boolean *pfignored));

/* Default signal handler.  This sets the appropriate element of the
   afSignal array.  If system calls are automatically restarted, it
   may do a longjmp to an fsysdep_catch.  */
extern RETSIGTYPE ussignal P((int isig));

/* Try to fork, repeating several times.  */
extern pid_t ixsfork P((void));

/* Spawn a job.  Returns the process ID of the spawned job or -1 on
   error.  The following macros may be passed in aidescs.  */

/* Set descriptor to /dev/null.  */
#define SPAWN_NULL (-1)
/* Set element of aidescs to a pipe for caller to read from.  */
#define SPAWN_READ_PIPE (-2)
/* Set element of aidescs to a pipe for caller to write to.  */
#define SPAWN_WRITE_PIPE (-3)

extern pid_t ixsspawn P((const char **pazargs, int *aidescs,
			 boolean fkeepuid, boolean fkeepenv,
			 const char *zchdir, boolean fnosigs,
			 boolean fshell, const char *zpath,
			 const char *zuu_machine,
			 const char *zuu_user));

/* Do a form of popen using ixsspawn.  */
extern FILE *espopen P((const char **pazargs, boolean frd,
			pid_t *pipid));

/* Wait for a particular process to finish, returning the exit status.
   The process ID should be pid_t, but we can't put that in a
   prototype.  */
extern int ixswait P((unsigned long ipid, const char *zreport));

/* Read from a connection using two file descriptors.  */
extern boolean fsdouble_read P((struct sconnection *qconn, char *zbuf,
				size_t *pclen, size_t cmin, int ctimeout,
				boolean freport));

/* Write to a connection using two file descriptors.  */
extern boolean fsdouble_write P((struct sconnection *qconn,
				 const char *zbuf, size_t clen));

/* Run a chat program on a connection using two file descriptors.  */
extern boolean fsdouble_chat P((struct sconnection *qconn,
				char **pzprog));

/* Find a spool file in the spool directory.  For a local file, the
   bgrade argument is the grade of the file.  This is needed for
   SPOOLDIR_SVR4.  */
extern char *zsfind_file P((const char *zsimple, const char *zsystem,
			    int bgrade));

/* Return the grade given a sequence number.  */
extern int bsgrade P((pointer pseq));

/* Lock a string.  */
extern boolean fsdo_lock P((const char *, boolean fspooldir,
			    boolean *pferr));

/* Unlock a string.  */
extern boolean fsdo_unlock P((const char *, boolean fspooldir));

/* Check access for a particular user name, or NULL to check access
   for any user.  */
extern boolean fsuser_access P((const struct stat *, int imode,
				const char *zuser));

/* Stick two directories and a file name together.  */
extern char *zsappend3 P((const char *zdir1, const char *zdir2,
			  const char *zfile));

/* Stick three directories and a file name together.  */
extern char *zsappend4 P((const char *zdir1, const char *zdir2,
			  const char *zdir3, const char *zfile));

/* Get a temporary file name.  */
extern char *zstemp_file P((const struct uuconf_system *qsys));

/* Get a command file name.  */
extern char *zscmd_file P((const struct uuconf_system *qsys, int bgrade));

/* Get a jobid from a system, a file name, and a grade.  */
extern char *zsfile_to_jobid P((const struct uuconf_system *qsys,
				const char *zfile,
				int bgrade));

/* Get a file name from a jobid.  This also returns the associated system
   in *pzsystem and the grade in *pbgrade.  */
extern char *zsjobid_to_file P((const char *zid, char **pzsystem,
				char *pbgrade));

/* See whether there is a spool directory for a system when using
   SPOOLDIR_ULTRIX.  */
extern boolean fsultrix_has_spool P((const char *zsystem));

#if HAVE_COHERENT_LOCKFILES
/* Lock a coherent tty.  */
extern boolean lockttyexist P((const char *z));
extern boolean fscoherent_disable_tty P((const char *zdevice,
					 char **pzenable));
#endif

/* Some replacements for standard Unix functions.  */

#if ! HAVE_DUP2
extern int dup2 P((int oold, int onew));
#endif

#if ! HAVE_FTW
extern int ftw P((const char *zdir,
		  int (*pfn) P((const char *zfile,
				struct stat *qstat,
				int iflag)),
		  int cdescriptors));
#endif

#if ! HAVE_GETCWD && ! HAVE_GETWD
extern char *getcwd P((char *zbuf, size_t cbuf));
#endif

#if ! HAVE_MKDIR
extern int mkdir P((const char *zdir, int imode));
#endif

#if ! HAVE_RENAME
extern int rename P((const char *zold, const char *znew));
#endif

#if ! HAVE_RMDIR
extern int rmdir P((const char *zdir));
#endif

/* The working directory from which the program was run (this is set
   by usysdep_initialize if called with INIT_GETCWD).  */
extern char *zScwd;

/* The spool directory name.  */
extern const char *zSspooldir;

/* The lock directory name.  */
extern const char *zSlockdir;

/* The local UUCP name (needed for some spool directory stuff).  */
extern const char *zSlocalname;

#endif /* ! defined (SYSH_UNX_H) */
