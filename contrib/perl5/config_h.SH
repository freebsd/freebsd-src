case $CONFIG in
'')
	if test -f config.sh; then TOP=.;
	elif test -f ../config.sh; then TOP=..;
	elif test -f ../../config.sh; then TOP=../..;
	elif test -f ../../../config.sh; then TOP=../../..;
	elif test -f ../../../../config.sh; then TOP=../../../..;
	else
		echo "Can't find config.sh."; exit 1
	fi
	. $TOP/config.sh
	;;
esac
case "$0" in
*/*) cd `expr X$0 : 'X\(.*\)/'` ;;
esac
echo "Extracting config.h (with variable substitutions)"
sed <<!GROK!THIS! >config.h -e 's!^#undef\(.*/\)\*!/\*#define\1 \*!' -e 's!^#un-def!#undef!'
/*
 * This file was produced by running the config_h.SH script, which
 * gets its values from config.sh, which is generally produced by
 * running Configure.
 *
 * Feel free to modify any of this as the need arises.  Note, however,
 * that running config_h.SH again will wipe out any changes you've made.
 * For a more permanent change edit config.sh and rerun config_h.SH.
 *
 * \$Id: Config_h.U,v 3.0.1.5 1997/02/28 14:57:43 ram Exp $
 */

/*
 * Package name      : $package
 * Source directory  : $src
 * Configuration time: $cf_time
 * Configured by     : $cf_by
 * Target system     : $myuname
 */

#ifndef _config_h_
#define _config_h_

/* LOC_SED:
 *	This symbol holds the complete pathname to the sed program.
 */
#define LOC_SED 	"$full_sed"	/**/

/* HAS_ALARM:
 *	This symbol, if defined, indicates that the alarm routine is
 *	available.
 */
#$d_alarm HAS_ALARM		/**/

/* HASATTRIBUTE:
 *	This symbol indicates the C compiler can check for function attributes,
 *	such as printf formats. This is normally only supported by GNU cc.
 */
#$d_attribut HASATTRIBUTE 	/**/
#ifndef HASATTRIBUTE
#define __attribute__(_arg_)
#endif

/* HAS_BCMP:
 *	This symbol is defined if the bcmp() routine is available to
 *	compare blocks of memory.
 */
#$d_bcmp HAS_BCMP	/**/

/* HAS_BCOPY:
 *	This symbol is defined if the bcopy() routine is available to
 *	copy blocks of memory.
 */
#$d_bcopy HAS_BCOPY	/**/

/* HAS_BZERO:
 *	This symbol is defined if the bzero() routine is available to
 *	set a memory block to 0.
 */
#$d_bzero HAS_BZERO	/**/

/* HAS_CHOWN:
 *	This symbol, if defined, indicates that the chown routine is
 *	available.
 */
#$d_chown HAS_CHOWN		/**/

/* HAS_CHROOT:
 *	This symbol, if defined, indicates that the chroot routine is
 *	available.
 */
#$d_chroot HAS_CHROOT		/**/

/* HAS_CHSIZE:
 *	This symbol, if defined, indicates that the chsize routine is available
 *	to truncate files.  You might need a -lx to get this routine.
 */
#$d_chsize	HAS_CHSIZE		/**/

/* HASCONST:
 *	This symbol, if defined, indicates that this C compiler knows about
 *	the const type. There is no need to actually test for that symbol
 *	within your programs. The mere use of the "const" keyword will
 *	trigger the necessary tests.
 */
#$d_const HASCONST	/**/
#ifndef HASCONST
#define const
#endif

/* HAS_CRYPT:
 *	This symbol, if defined, indicates that the crypt routine is available
 *	to encrypt passwords and the like.
 */
#$d_crypt HAS_CRYPT		/**/

/* HAS_CUSERID:
 *	This symbol, if defined, indicates that the cuserid routine is
 *	available to get character login names.
 */
#$d_cuserid HAS_CUSERID		/**/

/* HAS_DBL_DIG:
 *	This symbol, if defined, indicates that this system's <float.h>
 *	or <limits.h> defines the symbol DBL_DIG, which is the number
 *	of significant digits in a double precision number.  If this
 *	symbol is not defined, a guess of 15 is usually pretty good.
 */
#$d_dbl_dig HAS_DBL_DIG 	/* */

/* HAS_DIFFTIME:
 *	This symbol, if defined, indicates that the difftime routine is
 *	available.
 */
#$d_difftime HAS_DIFFTIME		/**/

/* HAS_DLERROR:
 *	This symbol, if defined, indicates that the dlerror routine is
 *	available to return a string describing the last error that
 *	occurred from a call to dlopen(), dlclose() or dlsym().
 */
#$d_dlerror HAS_DLERROR	/**/

/* SETUID_SCRIPTS_ARE_SECURE_NOW:
 *	This symbol, if defined, indicates that the bug that prevents
 *	setuid scripts from being secure is not present in this kernel.
 */
/* DOSUID:
 *	This symbol, if defined, indicates that the C program should
 *	check the script that it is executing for setuid/setgid bits, and
 *	attempt to emulate setuid/setgid on systems that have disabled
 *	setuid #! scripts because the kernel can't do it securely.
 *	It is up to the package designer to make sure that this emulation
 *	is done securely.  Among other things, it should do an fstat on
 *	the script it just opened to make sure it really is a setuid/setgid
 *	script, it should make sure the arguments passed correspond exactly
 *	to the argument on the #! line, and it should not trust any
 *	subprocesses to which it must pass the filename rather than the
 *	file descriptor of the script to be executed.
 */
#$d_suidsafe SETUID_SCRIPTS_ARE_SECURE_NOW	/**/
#$d_dosuid DOSUID		/**/

/* HAS_DUP2:
 *	This symbol, if defined, indicates that the dup2 routine is
 *	available to duplicate file descriptors.
 */
#$d_dup2 HAS_DUP2	/**/

/* HAS_FCHMOD:
 *	This symbol, if defined, indicates that the fchmod routine is available
 *	to change mode of opened files.  If unavailable, use chmod().
 */
#$d_fchmod HAS_FCHMOD		/**/

/* HAS_FCHOWN:
 *	This symbol, if defined, indicates that the fchown routine is available
 *	to change ownership of opened files.  If unavailable, use chown().
 */
#$d_fchown HAS_FCHOWN		/**/

/* HAS_FCNTL:
 *	This symbol, if defined, indicates to the C program that
 *	the fcntl() function exists.
 */
#$d_fcntl HAS_FCNTL		/**/

/* HAS_FGETPOS:
 *	This symbol, if defined, indicates that the fgetpos routine is
 *	available to get the file position indicator, similar to ftell().
 */
#$d_fgetpos HAS_FGETPOS	/**/

/* HAS_FLOCK:
 *	This symbol, if defined, indicates that the flock routine is
 *	available to do file locking.
 */
#$d_flock HAS_FLOCK		/**/

/* HAS_FORK:
 *	This symbol, if defined, indicates that the fork routine is
 *	available.
 */
#$d_fork HAS_FORK		/**/

/* HAS_FSETPOS:
 *	This symbol, if defined, indicates that the fsetpos routine is
 *	available to set the file position indicator, similar to fseek().
 */
#$d_fsetpos HAS_FSETPOS	/**/

/* HAS_GETTIMEOFDAY:
 *	This symbol, if defined, indicates that the gettimeofday() system
 *	call is available for a sub-second accuracy clock. Usually, the file
 *	<sys/resource.h> needs to be included (see I_SYS_RESOURCE).
 *	The type "Timeval" should be used to refer to "struct timeval".
 */
#$d_gettimeod HAS_GETTIMEOFDAY	/**/
#ifdef HAS_GETTIMEOFDAY
#define Timeval struct timeval	/* Structure used by gettimeofday() */
#endif

/* HAS_GETGROUPS:
 *	This symbol, if defined, indicates that the getgroups() routine is
 *	available to get the list of process groups.  If unavailable, multiple
 *	groups are probably not supported.
 */
#$d_getgrps HAS_GETGROUPS		/**/

/* HAS_GETLOGIN:
 *	This symbol, if defined, indicates that the getlogin routine is
 *	available to get the login name.
 */
#$d_getlogin HAS_GETLOGIN		/**/

/* HAS_GETPGID:
 *	This symbol, if defined, indicates to the C program that 
 *	the getpgid(pid) function is available to get the
 *	process group id.
 */
#$d_getpgid HAS_GETPGID		/**/

/* HAS_GETPGRP:
 *	This symbol, if defined, indicates that the getpgrp routine is
 *	available to get the current process group.
 */
/* USE_BSD_GETPGRP:
 *	This symbol, if defined, indicates that getpgrp needs one
 *	arguments whereas USG one needs none.
 */
#$d_getpgrp HAS_GETPGRP		/**/
#$d_bsdgetpgrp USE_BSD_GETPGRP	/**/

/* HAS_GETPGRP2:
 *	This symbol, if defined, indicates that the getpgrp2() (as in DG/UX)
 *	routine is available to get the current process group.
 */
#$d_getpgrp2 HAS_GETPGRP2		/**/

/* HAS_GETPPID:
 *	This symbol, if defined, indicates that the getppid routine is
 *	available to get the parent process ID.
 */
#$d_getppid HAS_GETPPID		/**/

/* HAS_GETPRIORITY:
 *	This symbol, if defined, indicates that the getpriority routine is
 *	available to get a process's priority.
 */
#$d_getprior HAS_GETPRIORITY		/**/

/* HAS_INET_ATON:
 *	This symbol, if defined, indicates to the C program that the
 *	inet_aton() function is available to parse IP address "dotted-quad"
 *	strings.
 */
#$d_inetaton HAS_INET_ATON		/**/

/* HAS_KILLPG:
 *	This symbol, if defined, indicates that the killpg routine is available
 *	to kill process groups.  If unavailable, you probably should use kill
 *	with a negative process number.
 */
#$d_killpg HAS_KILLPG	/**/

/* HAS_LINK:
 *	This symbol, if defined, indicates that the link routine is
 *	available to create hard links.
 */
#$d_link HAS_LINK	/**/

/* HAS_LOCALECONV:
 *	This symbol, if defined, indicates that the localeconv routine is
 *	available for numeric and monetary formatting conventions.
 */
#$d_locconv HAS_LOCALECONV	/**/

/* HAS_LOCKF:
 *	This symbol, if defined, indicates that the lockf routine is
 *	available to do file locking.
 */
#$d_lockf HAS_LOCKF		/**/

/* HAS_LSTAT:
 *	This symbol, if defined, indicates that the lstat routine is
 *	available to do file stats on symbolic links.
 */
#$d_lstat HAS_LSTAT		/**/

/* HAS_MBLEN:
 *	This symbol, if defined, indicates that the mblen routine is available
 *	to find the number of bytes in a multibye character.
 */
#$d_mblen HAS_MBLEN		/**/

/* HAS_MBSTOWCS:
 *	This symbol, if defined, indicates that the mbstowcs routine is
 *	available to covert a multibyte string into a wide character string.
 */
#$d_mbstowcs	HAS_MBSTOWCS		/**/

/* HAS_MBTOWC:
 *	This symbol, if defined, indicates that the mbtowc routine is available
 *	to covert a multibyte to a wide character.
 */
#$d_mbtowc HAS_MBTOWC		/**/

/* HAS_MEMCMP:
 *	This symbol, if defined, indicates that the memcmp routine is available
 *	to compare blocks of memory.
 */
#$d_memcmp HAS_MEMCMP	/**/

/* HAS_MEMCPY:
 *	This symbol, if defined, indicates that the memcpy routine is available
 *	to copy blocks of memory.
 */
#$d_memcpy HAS_MEMCPY	/**/

/* HAS_MEMMOVE:
 *	This symbol, if defined, indicates that the memmove routine is available
 *	to copy potentially overlapping blocks of memory. This should be used
 *	only when HAS_SAFE_BCOPY is not defined. If neither is there, roll your
 *	own version.
 */
#$d_memmove HAS_MEMMOVE	/**/

/* HAS_MEMSET:
 *	This symbol, if defined, indicates that the memset routine is available
 *	to set blocks of memory.
 */
#$d_memset HAS_MEMSET	/**/

/* HAS_MKDIR:
 *	This symbol, if defined, indicates that the mkdir routine is available
 *	to create directories.  Otherwise you should fork off a new process to
 *	exec /bin/mkdir.
 */
#$d_mkdir HAS_MKDIR		/**/

/* HAS_MKFIFO:
 *	This symbol, if defined, indicates that the mkfifo routine is
 *	available to create FIFOs. Otherwise, mknod should be able to
 *	do it for you. However, if mkfifo is there, mknod might require
 *	super-user privileges which mkfifo will not.
 */
#$d_mkfifo HAS_MKFIFO		/**/

/* HAS_MKTIME:
 *	This symbol, if defined, indicates that the mktime routine is
 *	available.
 */
#$d_mktime HAS_MKTIME		/**/

/* HAS_MSYNC:
 *	This symbol, if defined, indicates that the msync system call is
 *	available to synchronize a mapped file.
 */
#$d_msync HAS_MSYNC		/**/

/* HAS_MUNMAP:
 *	This symbol, if defined, indicates that the munmap system call is
 *	available to unmap a region, usually mapped by mmap().
 */
#$d_munmap HAS_MUNMAP		/**/

/* HAS_NICE:
 *	This symbol, if defined, indicates that the nice routine is
 *	available.
 */
#$d_nice HAS_NICE		/**/

/* HAS_PATHCONF:
 *	This symbol, if defined, indicates that pathconf() is available
 *	to determine file-system related limits and options associated
 *	with a given filename.
 */
/* HAS_FPATHCONF:
 *	This symbol, if defined, indicates that pathconf() is available
 *	to determine file-system related limits and options associated
 *	with a given open file descriptor.
 */
#$d_pathconf HAS_PATHCONF		/**/
#$d_fpathconf HAS_FPATHCONF		/**/

/* HAS_PAUSE:
 *	This symbol, if defined, indicates that the pause routine is
 *	available to suspend a process until a signal is received.
 */
#$d_pause HAS_PAUSE		/**/

/* HAS_PIPE:
 *	This symbol, if defined, indicates that the pipe routine is
 *	available to create an inter-process channel.
 */
#$d_pipe HAS_PIPE		/**/

/* HAS_POLL:
 *	This symbol, if defined, indicates that the poll routine is
 *	available to poll active file descriptors. You may safely
 *	include <poll.h> when this symbol is defined.
 */
#$d_poll HAS_POLL		/**/

/* HAS_READDIR:
 *	This symbol, if defined, indicates that the readdir routine is
 *	available to read directory entries. You may have to include
 *	<dirent.h>. See I_DIRENT.
 */
#$d_readdir HAS_READDIR		/**/

/* HAS_SEEKDIR:
 *	This symbol, if defined, indicates that the seekdir routine is
 *	available. You may have to include <dirent.h>. See I_DIRENT.
 */
#$d_seekdir HAS_SEEKDIR		/**/

/* HAS_TELLDIR:
 *	This symbol, if defined, indicates that the telldir routine is
 *	available. You may have to include <dirent.h>. See I_DIRENT.
 */
#$d_telldir HAS_TELLDIR		/**/

/* HAS_REWINDDIR:
 *	This symbol, if defined, indicates that the rewinddir routine is
 *	available. You may have to include <dirent.h>. See I_DIRENT.
 */
#$d_rewinddir HAS_REWINDDIR		/**/

/* HAS_READLINK:
 *	This symbol, if defined, indicates that the readlink routine is
 *	available to read the value of a symbolic link.
 */
#$d_readlink HAS_READLINK		/**/

/* HAS_RENAME:
 *	This symbol, if defined, indicates that the rename routine is available
 *	to rename files.  Otherwise you should do the unlink(), link(), unlink()
 *	trick.
 */
#$d_rename HAS_RENAME	/**/

/* HAS_RMDIR:
 *	This symbol, if defined, indicates that the rmdir routine is
 *	available to remove directories. Otherwise you should fork off a
 *	new process to exec /bin/rmdir.
 */
#$d_rmdir HAS_RMDIR		/**/

/* HAS_SELECT:
 *	This symbol, if defined, indicates that the select routine is
 *	available to select active file descriptors. If the timeout field
 *	is used, <sys/time.h> may need to be included.
 */
#$d_select HAS_SELECT	/**/

/* HAS_SETEGID:
 *	This symbol, if defined, indicates that the setegid routine is available
 *	to change the effective gid of the current program.
 */
#$d_setegid HAS_SETEGID		/**/

/* HAS_SETEUID:
 *	This symbol, if defined, indicates that the seteuid routine is available
 *	to change the effective uid of the current program.
 */
#$d_seteuid HAS_SETEUID		/**/

/* HAS_SETLINEBUF:
 *	This symbol, if defined, indicates that the setlinebuf routine is
 *	available to change stderr or stdout from block-buffered or unbuffered
 *	to a line-buffered mode.
 */
#$d_setlinebuf HAS_SETLINEBUF		/**/

/* HAS_SETLOCALE:
 *	This symbol, if defined, indicates that the setlocale routine is
 *	available to handle locale-specific ctype implementations.
 */
#$d_setlocale HAS_SETLOCALE	/**/

/* HAS_SETPGID:
 *	This symbol, if defined, indicates that the setpgid(pid, gpid)
 *	routine is available to set process group ID.
 */
#$d_setpgid HAS_SETPGID	/**/

/* HAS_SETPGRP:
 *	This symbol, if defined, indicates that the setpgrp routine is
 *	available to set the current process group.
 */
/* USE_BSD_SETPGRP:
 *	This symbol, if defined, indicates that setpgrp needs two
 *	arguments whereas USG one needs none.  See also HAS_SETPGID
 *	for a POSIX interface.
 */
#$d_setpgrp HAS_SETPGRP		/**/
#$d_bsdsetpgrp USE_BSD_SETPGRP	/**/

/* HAS_SETPGRP2:
 *	This symbol, if defined, indicates that the setpgrp2() (as in DG/UX)
 *	routine is available to set the current process group.
 */
#$d_setpgrp2 HAS_SETPGRP2		/**/

/* HAS_SETPRIORITY:
 *	This symbol, if defined, indicates that the setpriority routine is
 *	available to set a process's priority.
 */
#$d_setprior HAS_SETPRIORITY		/**/

/* HAS_SETREGID:
 *	This symbol, if defined, indicates that the setregid routine is
 *	available to change the real and effective gid of the current
 *	process.
 */
/* HAS_SETRESGID:
 *	This symbol, if defined, indicates that the setresgid routine is
 *	available to change the real, effective and saved gid of the current
 *	process.
 */
#$d_setregid HAS_SETREGID		/**/
#$d_setresgid HAS_SETRESGID		/**/

/* HAS_SETREUID:
 *	This symbol, if defined, indicates that the setreuid routine is
 *	available to change the real and effective uid of the current
 *	process.
 */
/* HAS_SETRESUID:
 *	This symbol, if defined, indicates that the setresuid routine is
 *	available to change the real, effective and saved uid of the current
 *	process.
 */
#$d_setreuid HAS_SETREUID		/**/
#$d_setresuid HAS_SETRESUID		/**/

/* HAS_SETRGID:
 *	This symbol, if defined, indicates that the setrgid routine is available
 *	to change the real gid of the current program.
 */
#$d_setrgid HAS_SETRGID		/**/

/* HAS_SETRUID:
 *	This symbol, if defined, indicates that the setruid routine is available
 *	to change the real uid of the current program.
 */
#$d_setruid HAS_SETRUID		/**/

/* HAS_SETSID:
 *	This symbol, if defined, indicates that the setsid routine is
 *	available to set the process group ID.
 */
#$d_setsid HAS_SETSID	/**/

/* Shmat_t:
 *	This symbol holds the return type of the shmat() system call.
 *	Usually set to 'void *' or 'char *'.
 */
/* HAS_SHMAT_PROTOTYPE:
 *	This symbol, if defined, indicates that the sys/shm.h includes
 *	a prototype for shmat().  Otherwise, it is up to the program to
 *	guess one.  Shmat_t shmat _((int, Shmat_t, int)) is a good guess,
 *	but not always right so it should be emitted by the program only
 *	when HAS_SHMAT_PROTOTYPE is not defined to avoid conflicting defs.
 */
#define Shmat_t $shmattype	/**/
#$d_shmatprototype HAS_SHMAT_PROTOTYPE	/**/

/* HAS_STRCHR:
 *	This symbol is defined to indicate that the strchr()/strrchr()
 *	functions are available for string searching. If not, try the
 *	index()/rindex() pair.
 */
/* HAS_INDEX:
 *	This symbol is defined to indicate that the index()/rindex()
 *	functions are available for string searching.
 */
#$d_strchr HAS_STRCHR	/**/
#$d_index HAS_INDEX	/**/

/* HAS_STRCOLL:
 *	This symbol, if defined, indicates that the strcoll routine is
 *	available to compare strings using collating information.
 */
#$d_strcoll HAS_STRCOLL	/**/

/* USE_STRUCT_COPY:
 *	This symbol, if defined, indicates that this C compiler knows how
 *	to copy structures.  If undefined, you'll need to use a block copy
 *	routine of some sort instead.
 */
#$d_strctcpy	USE_STRUCT_COPY	/**/

/* HAS_STRTOD:
 *	This symbol, if defined, indicates that the strtod routine is
 *	available to provide better numeric string conversion than atof().
 */
#$d_strtod HAS_STRTOD	/**/

/* HAS_STRTOL:
 *	This symbol, if defined, indicates that the strtol routine is available
 *	to provide better numeric string conversion than atoi() and friends.
 */
#$d_strtol HAS_STRTOL	/**/

/* HAS_STRTOUL:
 *	This symbol, if defined, indicates that the strtoul routine is
 *	available to provide conversion of strings to unsigned long.
 */
#$d_strtoul HAS_STRTOUL	/**/

/* HAS_STRXFRM:
 *	This symbol, if defined, indicates that the strxfrm() routine is
 *	available to transform strings.
 */
#$d_strxfrm HAS_STRXFRM	/**/

/* HAS_SYMLINK:
 *	This symbol, if defined, indicates that the symlink routine is available
 *	to create symbolic links.
 */
#$d_symlink HAS_SYMLINK	/**/

/* HAS_SYSCALL:
 *	This symbol, if defined, indicates that the syscall routine is
 *	available to call arbitrary system calls. If undefined, that's tough.
 */
#$d_syscall HAS_SYSCALL	/**/

/* HAS_SYSCONF:
 *	This symbol, if defined, indicates that sysconf() is available
 *	to determine system related limits and options.
 */
#$d_sysconf HAS_SYSCONF	/**/

/* HAS_SYSTEM:
 *	This symbol, if defined, indicates that the system routine is
 *	available to issue a shell command.
 */
#$d_system HAS_SYSTEM	/**/

/* HAS_TCGETPGRP:
 *	This symbol, if defined, indicates that the tcgetpgrp routine is
 *	available to get foreground process group ID.
 */
#$d_tcgetpgrp HAS_TCGETPGRP		/**/

/* HAS_TCSETPGRP:
 *	This symbol, if defined, indicates that the tcsetpgrp routine is
 *	available to set foreground process group ID.
 */
#$d_tcsetpgrp HAS_TCSETPGRP		/**/

/* HAS_TRUNCATE:
 *	This symbol, if defined, indicates that the truncate routine is
 *	available to truncate files.
 */
#$d_truncate HAS_TRUNCATE	/**/

/* HAS_TZNAME:
 *	This symbol, if defined, indicates that the tzname[] array is
 *	available to access timezone names.
 */
#$d_tzname HAS_TZNAME		/**/

/* HAS_UMASK:
 *	This symbol, if defined, indicates that the umask routine is
 *	available to set and get the value of the file creation mask.
 */
#$d_umask HAS_UMASK		/**/

/* HASVOLATILE:
 *	This symbol, if defined, indicates that this C compiler knows about
 *	the volatile declaration.
 */
#$d_volatile	HASVOLATILE	/**/
#ifndef HASVOLATILE
#define volatile
#endif

/* HAS_WAIT4:
 *	This symbol, if defined, indicates that wait4() exists.
 */
#$d_wait4 HAS_WAIT4	/**/

/* HAS_WAITPID:
 *	This symbol, if defined, indicates that the waitpid routine is
 *	available to wait for child process.
 */
#$d_waitpid HAS_WAITPID	/**/

/* HAS_WCSTOMBS:
 *	This symbol, if defined, indicates that the wcstombs routine is
 *	available to convert wide character strings to multibyte strings.
 */
#$d_wcstombs HAS_WCSTOMBS	/**/

/* HAS_WCTOMB:
 *	This symbol, if defined, indicates that the wctomb routine is available
 *	to covert a wide character to a multibyte.
 */
#$d_wctomb HAS_WCTOMB		/**/

/* I_ARPA_INET:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <arpa/inet.h> to get inet_addr and friends declarations.
 */
#$i_arpainet	I_ARPA_INET		/**/

/* I_DBM:
 *	This symbol, if defined, indicates that <dbm.h> exists and should
 *	be included.
 */
/* I_RPCSVC_DBM:
 *	This symbol, if defined, indicates that <rpcsvc/dbm.h> exists and
 *	should be included.
 */
#$i_dbm I_DBM	/**/
#$i_rpcsvcdbm I_RPCSVC_DBM	/**/

/* I_DIRENT:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <dirent.h>. Using this symbol also triggers the definition
 *	of the Direntry_t define which ends up being 'struct dirent' or
 *	'struct direct' depending on the availability of <dirent.h>.
 */
/* DIRNAMLEN:
 *	This symbol, if defined, indicates to the C program that the length
 *	of directory entry names is provided by a d_namlen field.  Otherwise
 *	you need to do strlen() on the d_name field.
 */
/* Direntry_t:
 *	This symbol is set to 'struct direct' or 'struct dirent' depending on
 *	whether dirent is available or not. You should use this pseudo type to
 *	portably declare your directory entries.
 */
#$i_dirent I_DIRENT		/**/
#$d_dirnamlen DIRNAMLEN	/**/
#define Direntry_t $direntrytype

/* I_DLFCN:
 *	This symbol, if defined, indicates that <dlfcn.h> exists and should
 *	be included.
 */
#$i_dlfcn I_DLFCN		/**/

/* I_FCNTL:
 *	This manifest constant tells the C program to include <fcntl.h>.
 */
#$i_fcntl I_FCNTL	/**/

/* I_FLOAT:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <float.h> to get definition of symbols like DBL_MAX or
 *	DBL_MIN, i.e. machine dependent floating point values.
 */
#$i_float I_FLOAT		/**/

/* I_LIMITS:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <limits.h> to get definition of symbols like WORD_BIT or
 *	LONG_MAX, i.e. machine dependant limitations.
 */
#$i_limits I_LIMITS		/**/

/* I_LOCALE:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <locale.h>.
 */
#$i_locale	I_LOCALE		/**/

/* I_MATH:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <math.h>.
 */
#$i_math I_MATH		/**/

/* I_MEMORY:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <memory.h>.
 */
#$i_memory I_MEMORY		/**/

/* I_NDBM:
 *	This symbol, if defined, indicates that <ndbm.h> exists and should
 *	be included.
 */
#$i_ndbm I_NDBM	/**/

/* I_NET_ERRNO:
 *	This symbol, if defined, indicates that <net/errno.h> exists and 
 *	should be included.
 */
#$i_neterrno I_NET_ERRNO		/**/

/* I_NETINET_IN:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <netinet/in.h>. Otherwise, you may try <sys/in.h>.
 */
#$i_niin I_NETINET_IN	/**/

/* I_SFIO:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sfio.h>.
 */
#$i_sfio	I_SFIO		/**/

/* I_STDDEF:
 *	This symbol, if defined, indicates that <stddef.h> exists and should
 *	be included.
 */
#$i_stddef I_STDDEF	/**/

/* I_STDLIB:
 *	This symbol, if defined, indicates that <stdlib.h> exists and should
 *	be included.
 */
#$i_stdlib I_STDLIB		/**/

/* I_STRING:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <string.h> (USG systems) instead of <strings.h> (BSD systems).
 */
#$i_string I_STRING		/**/

/* I_SYS_DIR:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/dir.h>.
 */
#$i_sysdir I_SYS_DIR		/**/

/* I_SYS_FILE:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/file.h> to get definition of R_OK and friends.
 */
#$i_sysfile I_SYS_FILE		/**/

/* I_SYS_IOCTL:
 *	This symbol, if defined, indicates that <sys/ioctl.h> exists and should
 *	be included. Otherwise, include <sgtty.h> or <termio.h>.
 */
#$i_sysioctl	I_SYS_IOCTL		/**/

/* I_SYS_NDIR:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/ndir.h>.
 */
#$i_sysndir I_SYS_NDIR	/**/

/* I_SYS_PARAM:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/param.h>.
 */
#$i_sysparam I_SYS_PARAM		/**/

/* I_SYS_RESOURCE:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/resource.h>.
 */
#$i_sysresrc I_SYS_RESOURCE		/**/

/* I_SYS_SELECT:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/select.h> in order to get definition of struct timeval.
 */
#$i_sysselct I_SYS_SELECT	/**/

/* I_SYS_STAT:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/stat.h>.
 */
#$i_sysstat	I_SYS_STAT		/**/

/* I_SYS_TIMES:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/times.h>.
 */
#$i_systimes	I_SYS_TIMES		/**/

/* I_SYS_TYPES:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/types.h>.
 */
#$i_systypes	I_SYS_TYPES		/**/

/* I_SYS_UN:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/un.h> to get UNIX domain socket definitions.
 */
#$i_sysun I_SYS_UN		/**/

/* I_SYS_WAIT:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/wait.h>.
 */
#$i_syswait I_SYS_WAIT	/**/

/* I_TERMIO:
 *	This symbol, if defined, indicates that the program should include
 *	<termio.h> rather than <sgtty.h>.  There are also differences in
 *	the ioctl() calls that depend on the value of this symbol.
 */
/* I_TERMIOS:
 *	This symbol, if defined, indicates that the program should include
 *	the POSIX termios.h rather than sgtty.h or termio.h.
 *	There are also differences in the ioctl() calls that depend on the
 *	value of this symbol.
 */
/* I_SGTTY:
 *	This symbol, if defined, indicates that the program should include
 *	<sgtty.h> rather than <termio.h>.  There are also differences in
 *	the ioctl() calls that depend on the value of this symbol.
 */
#$i_termio I_TERMIO		/**/
#$i_termios I_TERMIOS		/**/
#$i_sgtty I_SGTTY		/**/

/* I_UNISTD:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <unistd.h>.
 */
#$i_unistd I_UNISTD		/**/

/* I_UTIME:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <utime.h>.
 */
#$i_utime I_UTIME		/**/

/* I_VALUES:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <values.h> to get definition of symbols like MINFLOAT or
 *	MAXLONG, i.e. machine dependant limitations.  Probably, you
 *	should use <limits.h> instead, if it is available.
 */
#$i_values I_VALUES		/**/

/* I_STDARG:
 *	This symbol, if defined, indicates that <stdarg.h> exists and should
 *	be included.
 */
/* I_VARARGS:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <varargs.h>.
 */
#$i_stdarg I_STDARG		/**/
#$i_varargs I_VARARGS	/**/

/* I_VFORK:
 *	This symbol, if defined, indicates to the C program that it should
 *	include vfork.h.
 */
#$i_vfork I_VFORK	/**/

/* CAN_PROTOTYPE:
 *	If defined, this macro indicates that the C compiler can handle
 *	function prototypes.
 */
/* _:
 *	This macro is used to declare function parameters for folks who want
 *	to make declarations with prototypes using a different style than
 *	the above macros.  Use double parentheses.  For example:
 *
 *		int main _((int argc, char *argv[]));
 */
#$prototype	CAN_PROTOTYPE	/**/
#ifdef CAN_PROTOTYPE
#define	_(args) args
#else
#define	_(args) ()
#endif

/* SH_PATH:
 *	This symbol contains the full pathname to the shell used on this
 *	on this system to execute Bourne shell scripts.  Usually, this will be
 *	/bin/sh, though it's possible that some systems will have /bin/ksh,
 *	/bin/pdksh, /bin/ash, /bin/bash, or even something such as
 *	D:/bin/sh.exe.
 */
#define SH_PATH "$sh"  /**/

/* STDCHAR:
 *	This symbol is defined to be the type of char used in stdio.h.
 *	It has the values "unsigned char" or "char".
 */
#define STDCHAR $stdchar	/**/

/* CROSSCOMPILE:
 *	This symbol, if defined, signifies that we our
 *	build process is a cross-compilation.
 */
#$crosscompile CROSSCOMPILE		/**/

/* INTSIZE:
 *	This symbol contains the value of sizeof(int) so that the C
 *	preprocessor can make decisions based on it.
 */
/* LONGSIZE:
 *	This symbol contains the value of sizeof(long) so that the C
 *	preprocessor can make decisions based on it.
 */
/* SHORTSIZE:
 *	This symbol contains the value of sizeof(short) so that the C
 *	preprocessor can make decisions based on it.
 */
#define INTSIZE $intsize		/**/
#define LONGSIZE $longsize		/**/
#define SHORTSIZE $shortsize		/**/

/* MULTIARCH:
 *	This symbol, if defined, signifies that the build
 *	process will produce some binary files that are going to be
 *	used in a cross-platform environment.  This is the case for
 *	example with the NeXT "fat" binaries that contain executables
 *	for several CPUs.
 */
#$multiarch MULTIARCH		/**/

/* HAS_QUAD:
 *	This symbol, if defined, tells that there's a 64-bit integer type,
 *	Quad_t, and its unsigned counterpar, Uquad_t. QUADKIND will be one
 *	of QUAD_IS_INT, QUAD_IS_LONG, QUAD_IS_LONG_LONG, or QUAD_IS_INT64_T.
 */
#$d_quad HAS_QUAD	/**/
#ifdef HAS_QUAD
#   define Quad_t $quadtype	/**/
#   define Uquad_t $uquadtype	/**/
#   define QUADKIND $quadkind	/**/
#   define QUAD_IS_INT	1
#   define QUAD_IS_LONG	2
#   define QUAD_IS_LONG_LONG	3
#   define QUAD_IS_INT64_T	4
#endif

/* HAS_ACCESSX:
 *	This symbol, if defined, indicates that the accessx routine is
 *	available to do extended access checks.
 */
#$d_accessx HAS_ACCESSX		/**/

/* HAS_EACCESS:
 *	This symbol, if defined, indicates that the eaccess routine is
 *	available to do extended access checks.
 */
#$d_eaccess HAS_EACCESS		/**/

/* I_SYS_ACCESS:
 *     This symbol, if defined, indicates to the C program that it should
 *     include <sys/access.h>.
 */
#$i_sysaccess   I_SYS_ACCESS                /**/

/* I_SYS_SECURITY:
 *     This symbol, if defined, indicates to the C program that it should
 *     include <sys/security.h>.
 */
#$i_syssecrt   I_SYS_SECURITY	/**/

/* OSNAME:
 *	This symbol contains the name of the operating system, as determined
 *	by Configure.  You shouldn't rely on it too much; the specific
 *	feature tests from Configure are generally more reliable.
 */
#define OSNAME "$osname"		/**/

/* MEM_ALIGNBYTES:
 *	This symbol contains the number of bytes required to align a
 *	double, or a long double when applicable. Usual values are 2,
 *	4 and 8. The default is eight, for safety.
 */
#if defined(CROSSCOMPILE) || defined(MULTIARCH)
#  define MEM_ALIGNBYTES 8
#else
#define MEM_ALIGNBYTES $alignbytes
#endif

/* ARCHLIB:
 *	This variable, if defined, holds the name of the directory in
 *	which the user wants to put architecture-dependent public
 *	library files for $package.  It is most often a local directory
 *	such as /usr/local/lib.  Programs using this variable must be
 *	prepared to deal with filename expansion.  If ARCHLIB is the
 *	same as PRIVLIB, it is not defined, since presumably the
 *	program already searches PRIVLIB.
 */
/* ARCHLIB_EXP:
 *	This symbol contains the ~name expanded version of ARCHLIB, to be used
 *	in programs that are not prepared to deal with ~ expansion at run-time.
 */
#$d_archlib ARCHLIB "$archlib"		/**/
#$d_archlib ARCHLIB_EXP "$archlibexp"		/**/

/* ARCHNAME:
 *	This symbol holds a string representing the architecture name.
 *	It may be used to construct an architecture-dependant pathname
 *	where library files may be held under a private library, for
 *	instance.
 */
#define ARCHNAME "$archname"		/**/

/* HAS_ATOLF:
 *	This symbol, if defined, indicates that the atolf routine is
 *	available to convert strings into long doubles.
 */
#$d_atolf HAS_ATOLF		/**/

/* HAS_ATOLL:
 *	This symbol, if defined, indicates that the atoll routine is
 *	available to convert strings into long longs.
 */
#$d_atoll HAS_ATOLL		/**/

/* BIN:
 *	This symbol holds the path of the bin directory where the package will
 *	be installed. Program must be prepared to deal with ~name substitution.
 */
/* BIN_EXP:
 *	This symbol is the filename expanded version of the BIN symbol, for
 *	programs that do not want to deal with that at run-time.
 */
#define BIN "$bin"	/**/
#define BIN_EXP "$binexp"	/**/

/* PERL_BINCOMPAT_5005:
 *	This symbol, if defined, indicates that this version of Perl should be
 *	binary-compatible with Perl 5.005.  This is impossible for builds
 *	that use features like threads and multiplicity it is always $undef
 *	for those versions.
 */
#$d_bincompat5005 PERL_BINCOMPAT_5005			/**/

/* BYTEORDER:
 *	This symbol holds the hexadecimal constant defined in byteorder,
 *	i.e. 0x1234 or 0x4321, etc...
 *	If the compiler supports cross-compiling or multiple-architecture
 *	binaries (eg. on NeXT systems), use compiler-defined macros to
 *	determine the byte order.
 *	On NeXT 3.2 (and greater), you can build "Fat" Multiple Architecture
 *	Binaries (MAB) on either big endian or little endian machines.
 *	The endian-ness is available at compile-time.  This only matters
 *	for perl, where the config.h can be generated and installed on 
 *	one system, and used by a different architecture to build an
 *	extension.  Older versions of NeXT that might not have
 *	defined either *_ENDIAN__ were all on Motorola 680x0 series,
 *	so the default case (for NeXT) is big endian to catch them. 
 *	This might matter for NeXT 3.0.
 */
#if defined(CROSSCOMPILE) || defined(MULTIARCH)
#  ifdef __LITTLE_ENDIAN__
#    if LONGSIZE == 4
#      define BYTEORDER 0x1234
#    else
#      if LONGSIZE == 8
#        define BYTEORDER 0x12345678
#      endif
#    endif
#  else
#    ifdef __BIG_ENDIAN__
#      if LONGSIZE == 4
#        define BYTEORDER 0x4321
#      else
#        if LONGSIZE == 8
#          define BYTEORDER 0x87654321
#        endif
#      endif
#    endif
#  endif
#  if !defined(BYTEORDER) && (defined(NeXT) || defined(__NeXT__))
#    define BYTEORDER 0x4321
#  endif
#else
#define BYTEORDER 0x$byteorder	/* large digits for MSB */
#endif /* NeXT */

/* CAT2:
 *	This macro catenates 2 tokens together.
 */
/* STRINGIFY:
 *	This macro surrounds its token with double quotes.
 */
#if $cpp_stuff == 1
#  define CAT2(a,b)	a/**/b
#  define STRINGIFY(a)	"a"
		/* If you can get stringification with catify, tell me how! */
#endif
#if $cpp_stuff == 42
#  define PeRl_CaTiFy(a, b)	a ## b	
#  define PeRl_StGiFy(a)	#a
/* the additional level of indirection enables these macros to be
 * used as arguments to other macros.  See K&R 2nd ed., page 231. */
#  define CAT2(a,b)	PeRl_CaTiFy(a,b)
#  define StGiFy(a)	PeRl_StGiFy(a)
#  define STRINGIFY(a)	PeRl_StGiFy(a)
#endif
#if $cpp_stuff != 1 && $cpp_stuff != 42
#include "Bletch: How does this C preprocessor catenate tokens?"
#endif

/* CPPSTDIN:
 *	This symbol contains the first part of the string which will invoke
 *	the C preprocessor on the standard input and produce to standard
 *	output.	 Typical value of "cc -E" or "/lib/cpp", but it can also
 *	call a wrapper. See CPPRUN.
 */
/* CPPMINUS:
 *	This symbol contains the second part of the string which will invoke
 *	the C preprocessor on the standard input and produce to standard
 *	output.  This symbol will have the value "-" if CPPSTDIN needs a minus
 *	to specify standard input, otherwise the value is "".
 */
/* CPPRUN:
 *	This symbol contains the string which will invoke a C preprocessor on
 *	the standard input and produce to standard output. It needs to end
 *	with CPPLAST, after all other preprocessor flags have been specified.
 *	The main difference with CPPSTDIN is that this program will never be a
 *	pointer to a shell wrapper, i.e. it will be empty if no preprocessor is
 *	available directly to the user. Note that it may well be different from
 *	the preprocessor used to compile the C program.
 */
/* CPPLAST:
 *	This symbol is intended to be used along with CPPRUN in the same manner
 *	symbol CPPMINUS is used with CPPSTDIN. It contains either "-" or "".
 */
#define CPPSTDIN "$cppstdin"
#define CPPMINUS "$cppminus"
#define CPPRUN "$cpprun"
#define CPPLAST "$cpplast"

/* HAS_ACCESS:
 *	This manifest constant lets the C program know that the access()
 *	system call is available to check for accessibility using real UID/GID.
 *	(always present on UNIX.)
 */
#$d_access HAS_ACCESS		/**/

/* CASTI32:
 *	This symbol is defined if the C compiler can cast negative
 *	or large floating point numbers to 32-bit ints.
 */
#$d_casti32	CASTI32		/**/

/* CASTNEGFLOAT:
 *	This symbol is defined if the C compiler can cast negative
 *	numbers to unsigned longs, ints and shorts.
 */
/* CASTFLAGS:
 *	This symbol contains flags that say what difficulties the compiler
 *	has casting odd floating values to unsigned long:
 *		0 = ok
 *		1 = couldn't cast < 0
 *		2 = couldn't cast >= 0x80000000
 *		4 = couldn't cast in argument expression list
 */
#$d_castneg	CASTNEGFLOAT		/**/
#define CASTFLAGS $castflags		/**/

/* VOID_CLOSEDIR:
 *	This symbol, if defined, indicates that the closedir() routine
 *	does not return a value.
 */
#$d_void_closedir VOID_CLOSEDIR		/**/

/* HAS_CSH:
 *	This symbol, if defined, indicates that the C-shell exists.
 */
/* CSH:
 *	This symbol, if defined, contains the full pathname of csh.
 */
#$d_csh HAS_CSH		/**/
#ifdef HAS_CSH
#define CSH "$full_csh"	/**/
#endif

/* DLSYM_NEEDS_UNDERSCORE:
 *	This symbol, if defined, indicates that we need to prepend an
 *	underscore to the symbol name before calling dlsym().  This only
 *	makes sense if you *have* dlsym, which we will presume is the
 *	case if you're using dl_dlopen.xs.
 */
#$d_dlsymun 	DLSYM_NEEDS_UNDERSCORE 	/**/

/* HAS_DRAND48_PROTO:
 *	This symbol, if defined, indicates that the system provides
 *	a prototype for the drand48() function.  Otherwise, it is up
 *	to the program to supply one.  A good guess is
 *		extern double drand48 _((void));
 */
#$d_drand48proto	HAS_DRAND48_PROTO	/**/

/* HAS_ENDGRENT:
 *	This symbol, if defined, indicates that the getgrent routine is
 *	available for finalizing sequential access of the group database.
 */
#$d_endgrent HAS_ENDGRENT		/**/

/* HAS_ENDHOSTENT:
 *	This symbol, if defined, indicates that the endhostent() routine is
 *	available to close whatever was being used for host queries.
 */
#$d_endhent HAS_ENDHOSTENT		/**/

/* HAS_ENDNETENT:
 *	This symbol, if defined, indicates that the endnetent() routine is
 *	available to close whatever was being used for network queries.
 */
#$d_endnent HAS_ENDNETENT		/**/

/* HAS_ENDPROTOENT:
 *	This symbol, if defined, indicates that the endprotoent() routine is
 *	available to close whatever was being used for protocol queries.
 */
#$d_endpent HAS_ENDPROTOENT		/**/

/* HAS_ENDPWENT:
 *	This symbol, if defined, indicates that the getgrent routine is
 *	available for finalizing sequential access of the passwd database.
 */
#$d_endpwent HAS_ENDPWENT		/**/

/* HAS_ENDSERVENT:
 *	This symbol, if defined, indicates that the endservent() routine is
 *	available to close whatever was being used for service queries.
 */
#$d_endsent HAS_ENDSERVENT		/**/

/* HAS_ENDSPENT:
 *	This symbol, if defined, indicates that the endspent system call is
 *	available to finalize the scan of SysV shadow password entries.
 */
#$d_endspent HAS_ENDSPENT		/**/

/* HAS_FD_SET:
 *	This symbol, when defined, indicates presence of the fd_set typedef
 *	in <sys/types.h>
 */
#$d_fd_set HAS_FD_SET	/**/

/* HAS_FPOS64_T:
 *	This symbol will be defined if the C compiler supports fpos64_t.
 */
#$d_fpos64_t	HAS_FPOS64_T    	/**/

/* HAS_STRUCT_FS_DATA:
 *	This symbol, if defined, indicates that the struct fs_data
 *	to do statfs() is supported.
 */
#$d_fs_data_s HAS_STRUCT_FS_DATA	/**/

/* HAS_FSEEKO:
 *	This symbol, if defined, indicates that the fseeko routine is
 *	available to fseek beyond 32 bits (useful for ILP32 hosts).
 */
#$d_fseeko HAS_FSEEKO		/**/

/* HAS_FSTATFS:
 *	This symbol, if defined, indicates that the fstatfs routine is
 *	available to stat filesystems by file descriptors.
 */
#$d_fstatfs HAS_FSTATFS		/**/

/* HAS_FTELLO:
 *	This symbol, if defined, indicates that the ftello routine is
 *	available to ftell beyond 32 bits (useful for ILP32 hosts).
 */
#$d_ftello HAS_FTELLO		/**/

/* Gconvert:
 *	This preprocessor macro is defined to convert a floating point
 *	number to a string without a trailing decimal point.  This
 *	emulates the behavior of sprintf("%g"), but is sometimes much more
 *	efficient.  If gconvert() is not available, but gcvt() drops the
 *	trailing decimal point, then gcvt() is used.  If all else fails,
 *	a macro using sprintf("%g") is used. Arguments for the Gconvert
 *	macro are: value, number of digits, whether trailing zeros should
 *	be retained, and the output buffer.
 *	Possible values are:
 *		d_Gconvert='gconvert((x),(n),(t),(b))'
 *		d_Gconvert='gcvt((x),(n),(b))'
 *		d_Gconvert='sprintf((b),"%.*g",(n),(x))'
 *	The last two assume trailing zeros should not be kept.
 */
#define Gconvert(x,n,t,b) $d_Gconvert

/* HAS_GETCWD:
 *	This symbol, if defined, indicates that the getcwd routine is
 *	available to get the current working directory.
 */
#$d_getcwd HAS_GETCWD		/**/

/* HAS_GETFSSTAT:
 *	This symbol, if defined, indicates that the getfsstat routine is
 *	available to stat filesystems in bulk.
 */
#$d_getfsstat HAS_GETFSSTAT		/**/

/* HAS_GETGRENT:
 *	This symbol, if defined, indicates that the getgrent routine is
 *	available for sequential access of the group database.
 */
#$d_getgrent HAS_GETGRENT		/**/

/* HAS_GETHOSTBYADDR:
 *	This symbol, if defined, indicates that the gethostbyaddr() routine is
 *	available to look up hosts by their IP addresses.
 */
#$d_gethbyaddr HAS_GETHOSTBYADDR		/**/

/* HAS_GETHOSTBYNAME:
 *	This symbol, if defined, indicates that the gethostbyname() routine is
 *	available to look up host names in some data base or other.
 */
#$d_gethbyname HAS_GETHOSTBYNAME		/**/

/* HAS_GETHOSTENT:
 *	This symbol, if defined, indicates that the gethostent() routine is
 *	available to look up host names in some data base or another.
 */
#$d_gethent HAS_GETHOSTENT		/**/

/* HAS_GETHOSTNAME:
 *	This symbol, if defined, indicates that the C program may use the
 *	gethostname() routine to derive the host name.  See also HAS_UNAME
 *	and PHOSTNAME.
 */
/* HAS_UNAME:
 *	This symbol, if defined, indicates that the C program may use the
 *	uname() routine to derive the host name.  See also HAS_GETHOSTNAME
 *	and PHOSTNAME.
 */
/* PHOSTNAME:
 *	This symbol, if defined, indicates the command to feed to the
 *	popen() routine to derive the host name.  See also HAS_GETHOSTNAME
 *	and HAS_UNAME.	Note that the command uses a fully qualified path,
 *	so that it is safe even if used by a process with super-user
 *	privileges.
 */
/* HAS_PHOSTNAME:
 *	This symbol, if defined, indicates that the C program may use the
 *	contents of PHOSTNAME as a command to feed to the popen() routine
 *	to derive the host name.
 */
#$d_gethname HAS_GETHOSTNAME	/**/
#$d_uname HAS_UNAME		/**/
#$d_phostname HAS_PHOSTNAME	/**/
#ifdef HAS_PHOSTNAME
#define PHOSTNAME "$aphostname"	/* How to get the host name */
#endif

/* HAS_GETHOST_PROTOS:
 *	This symbol, if defined, indicates that <netdb.h> includes
 *	prototypes for gethostent(), gethostbyname(), and
 *	gethostbyaddr().  Otherwise, it is up to the program to guess
 *	them.  See netdbtype.U for probing for various Netdb_xxx_t types.
 */
#$d_gethostprotos	HAS_GETHOST_PROTOS	/**/

/* HAS_GETMNT:
 *	This symbol, if defined, indicates that the getmnt routine is
 *	available to get filesystem mount info by filename.
 */
#$d_getmnt HAS_GETMNT		/**/

/* HAS_GETMNTENT:
 *	This symbol, if defined, indicates that the getmntent routine is
 *	available to iterate through mounted file systems to get their info.
 */
#$d_getmntent HAS_GETMNTENT		/**/

/* HAS_GETNETBYADDR:
 *	This symbol, if defined, indicates that the getnetbyaddr() routine is
 *	available to look up networks by their IP addresses.
 */
#$d_getnbyaddr HAS_GETNETBYADDR		/**/

/* HAS_GETNETBYNAME:
 *	This symbol, if defined, indicates that the getnetbyname() routine is
 *	available to look up networks by their names.
 */
#$d_getnbyname HAS_GETNETBYNAME		/**/

/* HAS_GETNETENT:
 *	This symbol, if defined, indicates that the getnetent() routine is
 *	available to look up network names in some data base or another.
 */
#$d_getnent HAS_GETNETENT		/**/

/* HAS_GETNET_PROTOS:
 *	This symbol, if defined, indicates that <netdb.h> includes
 *	prototypes for getnetent(), getnetbyname(), and
 *	getnetbyaddr().  Otherwise, it is up to the program to guess
 *	them.  See netdbtype.U for probing for various Netdb_xxx_t types.
 */
#$d_getnetprotos	HAS_GETNET_PROTOS	/**/

/* HAS_GETPROTOENT:
 *	This symbol, if defined, indicates that the getprotoent() routine is
 *	available to look up protocols in some data base or another.
 */
#$d_getpent HAS_GETPROTOENT		/**/

/* HAS_GETPROTOBYNAME:
 *	This symbol, if defined, indicates that the getprotobyname()
 *	routine is available to look up protocols by their name.
 */
/* HAS_GETPROTOBYNUMBER:
 *	This symbol, if defined, indicates that the getprotobynumber()
 *	routine is available to look up protocols by their number.
 */
#$d_getpbyname HAS_GETPROTOBYNAME		/**/
#$d_getpbynumber HAS_GETPROTOBYNUMBER		/**/

/* HAS_GETPROTO_PROTOS:
 *	This symbol, if defined, indicates that <netdb.h> includes
 *	prototypes for getprotoent(), getprotobyname(), and
 *	getprotobyaddr().  Otherwise, it is up to the program to guess
 *	them.  See netdbtype.U for probing for various Netdb_xxx_t types.
 */
#$d_getprotoprotos	HAS_GETPROTO_PROTOS	/**/

/* HAS_GETPWENT:
 *	This symbol, if defined, indicates that the getpwent routine is
 *	available for sequential access of the passwd database.
 *	If this is not available, the older getpw() function may be available.
 */
#$d_getpwent HAS_GETPWENT		/**/

/* HAS_GETSERVENT:
 *	This symbol, if defined, indicates that the getservent() routine is
 *	available to look up network services in some data base or another.
 */
#$d_getsent HAS_GETSERVENT		/**/

/* HAS_GETSERV_PROTOS:
 *	This symbol, if defined, indicates that <netdb.h> includes
 *	prototypes for getservent(), getservbyname(), and
 *	getservbyaddr().  Otherwise, it is up to the program to guess
 *	them.  See netdbtype.U for probing for various Netdb_xxx_t types.
 */
#$d_getservprotos	HAS_GETSERV_PROTOS	/**/

/* HAS_GETSPENT:
 *	This symbol, if defined, indicates that the getspent system call is
 *	available to retrieve SysV shadow password entries sequentially.
 */
#$d_getspent HAS_GETSPENT		/**/

/* HAS_GETSPNAM:
 *	This symbol, if defined, indicates that the getspnam system call is
 *	available to retrieve SysV shadow password entries by name.
 */
#$d_getspnam HAS_GETSPNAM		/**/

/* HAS_GETSERVBYNAME:
 *	This symbol, if defined, indicates that the getservbyname()
 *	routine is available to look up services by their name.
 */
/* HAS_GETSERVBYPORT:
 *	This symbol, if defined, indicates that the getservbyport()
 *	routine is available to look up services by their port.
 */
#$d_getsbyname HAS_GETSERVBYNAME		/**/
#$d_getsbyport HAS_GETSERVBYPORT		/**/

/* HAS_GNULIBC:
 *	This symbol, if defined, indicates to the C program that 
 *	the GNU C library is being used.
 */
#$d_gnulibc HAS_GNULIBC  	/**/
#if defined(HAS_GNULIBC) && !defined(_GNU_SOURCE)
#   define _GNU_SOURCE
#endif
/* HAS_HASMNTOPT:
 *	This symbol, if defined, indicates that the hasmntopt routine is
 *	available to query the mount options of file systems.
 */
#$d_hasmntopt HAS_HASMNTOPT		/**/

/* HAS_HTONL:
 *	This symbol, if defined, indicates that the htonl() routine (and
 *	friends htons() ntohl() ntohs()) are available to do network
 *	order byte swapping.
 */
/* HAS_HTONS:
 *	This symbol, if defined, indicates that the htons() routine (and
 *	friends htonl() ntohl() ntohs()) are available to do network
 *	order byte swapping.
 */
/* HAS_NTOHL:
 *	This symbol, if defined, indicates that the ntohl() routine (and
 *	friends htonl() htons() ntohs()) are available to do network
 *	order byte swapping.
 */
/* HAS_NTOHS:
 *	This symbol, if defined, indicates that the ntohs() routine (and
 *	friends htonl() htons() ntohl()) are available to do network
 *	order byte swapping.
 */
#$d_htonl HAS_HTONL		/**/
#$d_htonl HAS_HTONS		/**/
#$d_htonl HAS_NTOHL		/**/
#$d_htonl HAS_NTOHS		/**/

/* HAS_ICONV:
 *	This symbol, if defined, indicates that the iconv routine is
 *	available to do character set conversions.
 */
#$d_iconv HAS_ICONV		/**/

/* HAS_INT64_T:
 *     This symbol will defined if the C compiler supports int64_t.
 *     Usually the <inttypes.h> needs to be included, but sometimes
 *	<sys/types.h> is enough.
 */
#$d_int64_t     HAS_INT64_T               /**/

/* HAS_ISASCII:
 *	This manifest constant lets the C program know that isascii 
 *	is available.
 */
#$d_isascii HAS_ISASCII		/**/

/* HAS_LDBL_DIG:
 *	This symbol, if defined, indicates that this system's <float.h>
 *	or <limits.h> defines the symbol LDBL_DIG, which is the number
 *	of significant digits in a long double precision number. Unlike
 *	for DBL_DIG, there's no good guess for LDBL_DIG if it is undefined.
 */
#$d_ldbl_dig HAS_LDBL_DIG 	/* */

/* HAS_LONG_DOUBLE:
 *	This symbol will be defined if the C compiler supports long
 *	doubles.
 */
/* LONG_DOUBLESIZE:
 *	This symbol contains the size of a long double, so that the 
 *	C preprocessor can make decisions based on it.  It is only
 *	defined if the system supports long doubles.
 */
#$d_longdbl HAS_LONG_DOUBLE		/**/
#ifdef HAS_LONG_DOUBLE
#define LONG_DOUBLESIZE $longdblsize		/**/
#endif

/* HAS_LONG_LONG:
 *	This symbol will be defined if the C compiler supports long long.
 */
/* LONGLONGSIZE:
 *	This symbol contains the size of a long long, so that the 
 *	C preprocessor can make decisions based on it.  It is only
 *	defined if the system supports long long.
 */
#$d_longlong HAS_LONG_LONG		/**/
#ifdef HAS_LONG_LONG
#define LONGLONGSIZE $longlongsize		/**/
#endif

/* HAS_LSEEK_PROTO:
 *	This symbol, if defined, indicates that the system provides
 *	a prototype for the lseek() function.  Otherwise, it is up
 *	to the program to supply one.  A good guess is
 *		extern off_t lseek(int, off_t, int);
 */
#$d_lseekproto	HAS_LSEEK_PROTO	/**/

/* HAS_MADVISE:
 *	This symbol, if defined, indicates that the madvise system call is
 *	available to map a file into memory.
 */
#$d_madvise HAS_MADVISE		/**/

/* HAS_MEMCHR:
 *	This symbol, if defined, indicates that the memchr routine is available
 *	to locate characters within a C string.
 */
#$d_memchr HAS_MEMCHR	/**/

/* HAS_MKDTEMP:
 *	This symbol, if defined, indicates that the mkdtemp routine is
 *	available to exclusively create a uniquely named temporary directory.
 */
#$d_mkdtemp HAS_MKDTEMP		/**/

/* HAS_MKSTEMP:
 *	This symbol, if defined, indicates that the mkstemp routine is
 *	available to exclusively create and open a uniquely named
 *	temporary file.
 */
#$d_mkstemp HAS_MKSTEMP		/**/

/* HAS_MKSTEMPS:
 *	This symbol, if defined, indicates that the mkstemps routine is
 *	available to excluslvely create and open a uniquely named
 *	(with a suffix) temporary file.
 */
#$d_mkstemps HAS_MKSTEMPS		/**/

/* HAS_MMAP:
 *	This symbol, if defined, indicates that the mmap system call is
 *	available to map a file into memory.
 */
/* Mmap_t:
 *	This symbol holds the return type of the mmap() system call
 *	(and simultaneously the type of the first argument).
 *	Usually set to 'void *' or 'cadd_t'.
 */
#$d_mmap HAS_MMAP		/**/
#define Mmap_t $mmaptype	/**/

/* HAS_MPROTECT:
 *	This symbol, if defined, indicates that the mprotect system call is
 *	available to modify the access protection of a memory mapped file.
 */
#$d_mprotect HAS_MPROTECT		/**/

/* HAS_MSG:
 *	This symbol, if defined, indicates that the entire msg*(2) library is
 *	supported (IPC mechanism based on message queues).
 */
#$d_msg HAS_MSG		/**/

/* HAS_OFF64_T:
 *	This symbol will be defined if the C compiler supports off64_t.
 */
#$d_off64_t	HAS_OFF64_T    		/**/

/* HAS_OPEN3:
 *	This manifest constant lets the C program know that the three
 *	argument form of open(2) is available.
 */
#$d_open3 HAS_OPEN3		/**/

/* OLD_PTHREAD_CREATE_JOINABLE:
 *	This symbol, if defined, indicates how to create pthread
 *	in joinable (aka undetached) state.  NOTE: not defined
 *	if pthread.h already has defined PTHREAD_CREATE_JOINABLE
 *	(the new version of the constant).
 *	If defined, known values are PTHREAD_CREATE_UNDETACHED
 *	and __UNDETACHED.
 */
#$d_old_pthread_create_joinable OLD_PTHREAD_CREATE_JOINABLE $old_pthread_create_joinable /**/

/* HAS_PTHREAD_YIELD:
 *	This symbol, if defined, indicates that the pthread_yield 
 *	routine is available to yield the execution of the current
 *	thread.	 sched_yield is preferable to pthread_yield.
 */
/* SCHED_YIELD:
 *	This symbol defines the way to yield the execution of
 *	the current thread.  Known ways are sched_yield,
 *	pthread_yield, and pthread_yield with NULL.
 */
/* HAS_SCHED_YIELD:
 *	This symbol, if defined, indicates that the sched_yield
 *	routine is available to yield the execution of the current
 *	thread.	 sched_yield is preferable to pthread_yield.
 */
#$d_pthread_yield HAS_PTHREAD_YIELD	/**/
#define SCHED_YIELD	$sched_yield	/**/
#$d_sched_yield HAS_SCHED_YIELD	/**/

/* HAS_SAFE_BCOPY:
 *	This symbol, if defined, indicates that the bcopy routine is available
 *	to copy potentially overlapping memory blocks. Otherwise you should
 *	probably use memmove() or memcpy(). If neither is defined, roll your
 *	own version.
 */
#$d_safebcpy HAS_SAFE_BCOPY	/**/

/* HAS_SAFE_MEMCPY:
 *	This symbol, if defined, indicates that the memcpy routine is available
 *	to copy potentially overlapping memory blocks. Otherwise you should
 *	probably use memmove() or memcpy(). If neither is defined, roll your
 *	own version.
 */
#$d_safemcpy HAS_SAFE_MEMCPY	/**/

/* HAS_SANE_MEMCMP:
 *	This symbol, if defined, indicates that the memcmp routine is available
 *	and can be used to compare relative magnitudes of chars with their high
 *	bits set.  If it is not defined, roll your own version.
 */
#$d_sanemcmp HAS_SANE_MEMCMP	/**/

/* HAS_SEM:
 *	This symbol, if defined, indicates that the entire sem*(2) library is
 *	supported.
 */
#$d_sem HAS_SEM		/**/

/* HAS_SETGRENT:
 *	This symbol, if defined, indicates that the setgrent routine is
 *	available for initializing sequential access of the group database.
 */
#$d_setgrent HAS_SETGRENT		/**/

/* HAS_SETGROUPS:
 *	This symbol, if defined, indicates that the setgroups() routine is
 *	available to set the list of process groups.  If unavailable, multiple
 *	groups are probably not supported.
 */
#$d_setgrps HAS_SETGROUPS		/**/

/* HAS_SETHOSTENT:
 *	This symbol, if defined, indicates that the sethostent() routine is
 *	available.
 */
#$d_sethent HAS_SETHOSTENT		/**/

/* HAS_SETNETENT:
 *	This symbol, if defined, indicates that the setnetent() routine is
 *	available.
 */
#$d_setnent HAS_SETNETENT		/**/

/* HAS_SETPROTOENT:
 *	This symbol, if defined, indicates that the setprotoent() routine is
 *	available.
 */
#$d_setpent HAS_SETPROTOENT		/**/

/* HAS_SETPROCTITLE:
 *	This symbol, if defined, indicates that the setproctitle routine is
 *	available to set process title.
 */
#$d_setproctitle HAS_SETPROCTITLE		/**/

/* HAS_SETPWENT:
 *	This symbol, if defined, indicates that the setpwent routine is
 *	available for initializing sequential access of the passwd database.
 */
#$d_setpwent HAS_SETPWENT		/**/

/* HAS_SETSERVENT:
 *	This symbol, if defined, indicates that the setservent() routine is
 *	available.
 */
#$d_setsent HAS_SETSERVENT		/**/

/* HAS_SETSPENT:
 *	This symbol, if defined, indicates that the setspent system call is
 *	available to initialize the scan of SysV shadow password entries.
 */
#$d_setspent HAS_SETSPENT		/**/

/* HAS_SETVBUF:
 *	This symbol, if defined, indicates that the setvbuf routine is
 *	available to change buffering on an open stdio stream.
 *	to a line-buffered mode.
 */
#$d_setvbuf HAS_SETVBUF		/**/

/* USE_SFIO:
 *	This symbol, if defined, indicates that sfio should
 *	be used.
 */
#$d_sfio	USE_SFIO		/**/

/* HAS_SHM:
 *	This symbol, if defined, indicates that the entire shm*(2) library is
 *	supported.
 */
#$d_shm HAS_SHM		/**/

/* HAS_SIGACTION:
 *	This symbol, if defined, indicates that Vr4's sigaction() routine
 *	is available.
 */
#$d_sigaction HAS_SIGACTION	/**/

/* HAS_SIGSETJMP:
 *	This variable indicates to the C program that the sigsetjmp()
 *	routine is available to save the calling process's registers
 *	and stack environment for later use by siglongjmp(), and
 *	to optionally save the process's signal mask.  See
 *	Sigjmp_buf, Sigsetjmp, and Siglongjmp.
 */
/* Sigjmp_buf:
 *	This is the buffer type to be used with Sigsetjmp and Siglongjmp.
 */
/* Sigsetjmp:
 *	This macro is used in the same way as sigsetjmp(), but will invoke
 *	traditional setjmp() if sigsetjmp isn't available.
 *	See HAS_SIGSETJMP.
 */
/* Siglongjmp:
 *	This macro is used in the same way as siglongjmp(), but will invoke
 *	traditional longjmp() if siglongjmp isn't available.
 *	See HAS_SIGSETJMP.
 */
#$d_sigsetjmp HAS_SIGSETJMP	/**/
#ifdef HAS_SIGSETJMP
#define Sigjmp_buf sigjmp_buf
#define Sigsetjmp(buf,save_mask) sigsetjmp((buf),(save_mask))
#define Siglongjmp(buf,retval) siglongjmp((buf),(retval))
#else
#define Sigjmp_buf jmp_buf
#define Sigsetjmp(buf,save_mask) setjmp((buf))
#define Siglongjmp(buf,retval) longjmp((buf),(retval))
#endif

/* HAS_SOCKET:
 *	This symbol, if defined, indicates that the BSD socket interface is
 *	supported.
 */
/* HAS_SOCKETPAIR:
 *	This symbol, if defined, indicates that the BSD socketpair() call is
 *	supported.
 */
/* HAS_MSG_CTRUNC:
 *	This symbol, if defined, indicates that the MSG_CTRUNC is supported.
 *	Checking just with #ifdef might not be enough because this symbol
 *	has been known to be an enum.
 */
/* HAS_MSG_DONTROUTE:
 *	This symbol, if defined, indicates that the MSG_DONTROUTE is supported.
 *	Checking just with #ifdef might not be enough because this symbol
 *	has been known to be an enum.
 */
/* HAS_MSG_OOB:
 *	This symbol, if defined, indicates that the MSG_OOB is supported.
 *	Checking just with #ifdef might not be enough because this symbol
 *	has been known to be an enum.
 */
/* HAS_MSG_PEEK:
 *	This symbol, if defined, indicates that the MSG_PEEK is supported.
 *	Checking just with #ifdef might not be enough because this symbol
 *	has been known to be an enum.
 */
/* HAS_MSG_PROXY:
 *	This symbol, if defined, indicates that the MSG_PROXY is supported.
 *	Checking just with #ifdef might not be enough because this symbol
 *	has been known to be an enum.
 */
/* HAS_SCM_RIGHTS:
 *	This symbol, if defined, indicates that the SCM_RIGHTS is supported.
 *	Checking just with #ifdef might not be enough because this symbol
 *	has been known to be an enum.
 */
#$d_socket	HAS_SOCKET		/**/
#$d_sockpair	HAS_SOCKETPAIR	/**/
#$d_msg_ctrunc	HAS_MSG_CTRUNC	/**/
#$d_msg_dontroute	HAS_MSG_DONTROUTE	/**/
#$d_msg_oob	HAS_MSG_OOB	/**/
#$d_msg_peek	HAS_MSG_PEEK	/**/
#$d_msg_proxy	HAS_MSG_PROXY	/**/
#$d_scm_rights	HAS_SCM_RIGHTS	/**/

/* HAS_SQRTL:
 *	This symbol, if defined, indicates that the sqrtl routine is
 *	available to do long double square roots.
 */
#$d_sqrtl HAS_SQRTL		/**/

/* USE_STAT_BLOCKS:
 *	This symbol is defined if this system has a stat structure declaring
 *	st_blksize and st_blocks.
 */
#ifndef USE_STAT_BLOCKS
#$d_statblks USE_STAT_BLOCKS 	/**/
#endif

/* HAS_STRUCT_STATFS_F_FLAGS:
 *	This symbol, if defined, indicates that the struct statfs
 *	does have the f_flags member containing the mount flags of
 *	the filesystem containing the file.
 *	This kind of struct statfs is coming from <sys/mount.h> (BSD 4.3),
 *	not from <sys/statfs.h> (SYSV).  Older BSDs (like Ultrix) do not
 *	have statfs() and struct statfs, they have ustat() and getmnt()
 *	with struct ustat and struct fs_data.
 */
#$d_statfs_f_flags HAS_STRUCT_STATFS_F_FLAGS		/**/

/* HAS_STRUCT_STATFS:
 *	This symbol, if defined, indicates that the struct statfs
 *	to do statfs() is supported.
 */
#$d_statfs_s HAS_STRUCT_STATFS	/**/

/* HAS_FSTATVFS:
 *	This symbol, if defined, indicates that the fstatvfs routine is
 *	available to stat filesystems by file descriptors.
 */
#$d_fstatvfs HAS_FSTATVFS		/**/

/* USE_STDIO_PTR:
 *	This symbol is defined if the _ptr and _cnt fields (or similar)
 *	of the stdio FILE structure can be used to access the stdio buffer
 *	for a file handle.  If this is defined, then the FILE_ptr(fp)
 *	and FILE_cnt(fp) macros will also be defined and should be used
 *	to access these fields.
 */
/* FILE_ptr:
 *	This macro is used to access the _ptr field (or equivalent) of the
 *	FILE structure pointed to by its argument. This macro will always be
 *	defined if USE_STDIO_PTR is defined.
 */
/* STDIO_PTR_LVALUE:
 *	This symbol is defined if the FILE_ptr macro can be used as an
 *	lvalue.
 */
/* FILE_cnt:
 *	This macro is used to access the _cnt field (or equivalent) of the
 *	FILE structure pointed to by its argument. This macro will always be
 *	defined if USE_STDIO_PTR is defined.
 */
/* STDIO_CNT_LVALUE:
 *	This symbol is defined if the FILE_cnt macro can be used as an
 *	lvalue.
 */
#$d_stdstdio USE_STDIO_PTR 	/**/
#ifdef USE_STDIO_PTR
#define FILE_ptr(fp)	$stdio_ptr
#$d_stdio_ptr_lval STDIO_PTR_LVALUE 		/**/
#define FILE_cnt(fp)	$stdio_cnt
#$d_stdio_cnt_lval STDIO_CNT_LVALUE 		/**/
#endif

/* USE_STDIO_BASE:
 *	This symbol is defined if the _base field (or similar) of the
 *	stdio FILE structure can be used to access the stdio buffer for
 *	a file handle.  If this is defined, then the FILE_base(fp) macro
 *	will also be defined and should be used to access this field.
 *	Also, the FILE_bufsiz(fp) macro will be defined and should be used
 *	to determine the number of bytes in the buffer.  USE_STDIO_BASE
 *	will never be defined unless USE_STDIO_PTR is.
 */
/* FILE_base:
 *	This macro is used to access the _base field (or equivalent) of the
 *	FILE structure pointed to by its argument. This macro will always be
 *	defined if USE_STDIO_BASE is defined.
 */
/* FILE_bufsiz:
 *	This macro is used to determine the number of bytes in the I/O
 *	buffer pointed to by _base field (or equivalent) of the FILE
 *	structure pointed to its argument. This macro will always be defined
 *	if USE_STDIO_BASE is defined.
 */
#$d_stdiobase USE_STDIO_BASE 	/**/
#ifdef USE_STDIO_BASE
#define FILE_base(fp)	$stdio_base
#define FILE_bufsiz(fp)	$stdio_bufsiz
#endif

/* HAS_STRERROR:
 *	This symbol, if defined, indicates that the strerror routine is
 *	available to translate error numbers to strings. See the writeup
 *	of Strerror() in this file before you try to define your own.
 */
/* HAS_SYS_ERRLIST:
 *	This symbol, if defined, indicates that the sys_errlist array is
 *	available to translate error numbers to strings. The extern int
 *	sys_nerr gives the size of that table.
 */
/* Strerror:
 *	This preprocessor symbol is defined as a macro if strerror() is
 *	not available to translate error numbers to strings but sys_errlist[]
 *	array is there.
 */
#$d_strerror HAS_STRERROR		/**/
#$d_syserrlst HAS_SYS_ERRLIST	/**/
#define Strerror(e) $d_strerrm

/* HAS_STRTOLD:
 *	This symbol, if defined, indicates that the strtold routine is
 *	available to convert strings to long doubles.
 */
#$d_strtold HAS_STRTOLD		/**/

/* HAS_STRTOLL:
 *	This symbol, if defined, indicates that the strtoll routine is
 *	available to convert strings to long longs.
 */
#$d_strtoll HAS_STRTOLL		/**/

/* HAS_STRTOULL:
 *	This symbol, if defined, indicates that the strtoull routine is
 *	available to convert strings to unsigned long longs.
 */
#$d_strtoull HAS_STRTOULL		/**/

/* HAS_STRTOUQ:
 *	This symbol, if defined, indicates that the strtouq routine is
 *	available to convert strings to unsigned long longs (quads).
 */
#$d_strtouq HAS_STRTOUQ		/**/

/* HAS_TELLDIR_PROTO:
 *	This symbol, if defined, indicates that the system provides
 *	a prototype for the telldir() function.  Otherwise, it is up
 *	to the program to supply one.  A good guess is
 *		extern long telldir _((DIR*));
 */
#$d_telldirproto	HAS_TELLDIR_PROTO	/**/

/* Time_t:
 *	This symbol holds the type returned by time(). It can be long,
 *	or time_t on BSD sites (in which case <sys/types.h> should be
 *	included).
 */
#define Time_t $timetype		/* Time type */

/* HAS_TIMES:
 *	This symbol, if defined, indicates that the times() routine exists.
 *	Note that this became obsolete on some systems (SUNOS), which now
 * use getrusage(). It may be necessary to include <sys/times.h>.
 */
#$d_times HAS_TIMES		/**/

/* HAS_UNION_SEMUN:
 *	This symbol, if defined, indicates that the union semun is
 *	defined by including <sys/sem.h>.  If not, the user code
 *	probably needs to define it as:
 *	union semun {
 *	    int val;
 *	    struct semid_ds *buf;
 *	    unsigned short *array;
 *	}
 */
/* USE_SEMCTL_SEMUN:
 *	This symbol, if defined, indicates that union semun is
 *	used for semctl IPC_STAT.
 */
/* USE_SEMCTL_SEMID_DS:
 *	This symbol, if defined, indicates that struct semid_ds * is
 *	used for semctl IPC_STAT.
 */
#$d_union_semun HAS_UNION_SEMUN	/**/
#$d_semctl_semun USE_SEMCTL_SEMUN	/**/
#$d_semctl_semid_ds USE_SEMCTL_SEMID_DS	/**/

/* HAS_USTAT:
 *	This symbol, if defined, indicates that the ustat system call is
 *	available to query file system statistics by dev_t.
 */
#$d_ustat HAS_USTAT		/**/

/* HAS_VFORK:
 *	This symbol, if defined, indicates that vfork() exists.
 */
#$d_vfork HAS_VFORK	/**/

/* Signal_t:
 *	This symbol's value is either "void" or "int", corresponding to the
 *	appropriate return type of a signal handler.  Thus, you can declare
 *	a signal handler using "Signal_t (*handler)()", and define the
 *	handler using "Signal_t handler(sig)".
 */
#define Signal_t $signal_t	/* Signal handler's return type */

/* HAS_VPRINTF:
 *	This symbol, if defined, indicates that the vprintf routine is available
 *	to printf with a pointer to an argument list.  If unavailable, you
 *	may need to write your own, probably in terms of _doprnt().
 */
/* USE_CHAR_VSPRINTF:
 *	This symbol is defined if this system has vsprintf() returning type
 *	(char*).  The trend seems to be to declare it as "int vsprintf()".  It
 *	is up to the package author to declare vsprintf correctly based on the
 *	symbol.
 */
#$d_vprintf HAS_VPRINTF	/**/
#$d_charvspr USE_CHAR_VSPRINTF 	/**/

/* USE_DYNAMIC_LOADING:
 *	This symbol, if defined, indicates that dynamic loading of
 *	some sort is available.
 */
#$usedl USE_DYNAMIC_LOADING		/**/

/* DOUBLESIZE:
 *	This symbol contains the size of a double, so that the C preprocessor
 *	can make decisions based on it.
 */
#define DOUBLESIZE $doublesize		/**/

/* EBCDIC:
 *     This symbol, if defined, indicates that this system uses
 *	EBCDIC encoding.
 */
#$ebcdic	EBCDIC 		/**/

/* FFLUSH_NULL:
 *	This symbol, if defined, tells that fflush(NULL) does flush
 *	all pending stdio output.
 */
/* FFLUSH_ALL:
 *	This symbol, if defined, tells that to flush
 *	all pending stdio output one must loop through all
 *	the stdio file handles stored in an array and fflush them.
 *	Note that if fflushNULL is defined, fflushall will not
 *	even be probed for and will be left undefined.
 */
#$fflushNULL	FFLUSH_NULL 		/**/
#$fflushall	FFLUSH_ALL 		/**/

/* Fpos_t:
 *	This symbol holds the type used to declare file positions in libc.
 *	It can be fpos_t, long, uint, etc... It may be necessary to include
 *	<sys/types.h> to get any typedef'ed information.
 */
#define Fpos_t $fpostype		/* File position type */

/* Gid_t_f:
 *	This symbol defines the format string used for printing a Gid_t.
 */
#define	Gid_t_f		$gidformat		/**/

/* Gid_t_sign:
 *	This symbol holds the signedess of a Gid_t.
 *	1 for unsigned, -1 for signed.
 */
#define Gid_t_sign	$gidsign		/* GID sign */

/* Gid_t_size:
 *	This symbol holds the size of a Gid_t in bytes.
 */
#define Gid_t_size $gidsize		/* GID size */

/* Gid_t:
 *	This symbol holds the return type of getgid() and the type of
 *	argument to setrgid() and related functions.  Typically,
 *	it is the type of group ids in the kernel. It can be int, ushort,
 *	gid_t, etc... It may be necessary to include <sys/types.h> to get
 *	any typedef'ed information.
 */
#define Gid_t $gidtype		/* Type for getgid(), etc... */

/* Groups_t:
 *	This symbol holds the type used for the second argument to
 *	getgroups() and setgroups().  Usually, this is the same as
 *	gidtype (gid_t) , but sometimes it isn't.
 *	It can be int, ushort, gid_t, etc... 
 *	It may be necessary to include <sys/types.h> to get any 
 *	typedef'ed information.  This is only required if you have
 *	getgroups() or setgroups()..
 */
#if defined(HAS_GETGROUPS) || defined(HAS_SETGROUPS)
#define Groups_t $groupstype	/* Type for 2nd arg to [sg]etgroups() */
#endif

/* DB_Prefix_t:
 *	This symbol contains the type of the prefix structure element
 *	in the <db.h> header file.  In older versions of DB, it was
 *	int, while in newer ones it is u_int32_t.
 */
/* DB_Hash_t:
 *	This symbol contains the type of the prefix structure element
 *	in the <db.h> header file.  In older versions of DB, it was
 *	int, while in newer ones it is size_t.
 */
#define DB_Hash_t	$db_hashtype		/**/
#define DB_Prefix_t	$db_prefixtype  	/**/

/* I_GRP:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <grp.h>.
 */
/* GRPASSWD:
 *	This symbol, if defined, indicates to the C program that struct group
 *	in <grp.h> contains gr_passwd.
 */
#$i_grp I_GRP		/**/
#$d_grpasswd GRPASSWD	/**/

/* I_ICONV:
 *	This symbol, if defined, indicates that <iconv.h> exists and
 *	should be included.
 */
#$i_iconv	I_ICONV		/**/

/* I_IEEEFP:
 *	This symbol, if defined, indicates that <ieeefp.h> exists and
 *	should be included.
 */
#$i_ieeefp	I_IEEEFP		/**/

/* I_INTTYPES:
 *     This symbol, if defined, indicates to the C program that it should
 *     include <inttypes.h>.
 */
#$i_inttypes   I_INTTYPES                /**/

/* I_MACH_CTHREADS:
 *     This symbol, if defined, indicates to the C program that it should
 *     include <mach/cthreads.h>.
 */
#$i_machcthr   I_MACH_CTHREADS	/**/

/* I_MNTENT:
 *	This symbol, if defined, indicates that <mntent.h> exists and
 *	should be included.
 */
#$i_mntent	I_MNTENT		/**/

/* I_NETDB:
 *	This symbol, if defined, indicates that <netdb.h> exists and
 *	should be included.
 */
#$i_netdb I_NETDB		/**/

/* I_NETINET_TCP:
 *     This symbol, if defined, indicates to the C program that it should
 *     include <netinet/tcp.h>.
 */
#$i_netinettcp   I_NETINET_TCP                /**/

/* I_POLL:
 *	This symbol, if defined, indicates that <poll.h> exists and
 *	should be included.
 */
#$i_poll	I_POLL		/**/

/* I_PTHREAD:
 *     This symbol, if defined, indicates to the C program that it should
 *     include <pthread.h>.
 */
#$i_pthread   I_PTHREAD	/**/

/* I_PWD:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <pwd.h>.
 */
/* PWQUOTA:
 *	This symbol, if defined, indicates to the C program that struct passwd
 *	contains pw_quota.
 */
/* PWAGE:
 *	This symbol, if defined, indicates to the C program that struct passwd
 *	contains pw_age.
 */
/* PWCHANGE:
 *	This symbol, if defined, indicates to the C program that struct passwd
 *	contains pw_change.
 */
/* PWCLASS:
 *	This symbol, if defined, indicates to the C program that struct passwd
 *	contains pw_class.
 */
/* PWEXPIRE:
 *	This symbol, if defined, indicates to the C program that struct passwd
 *	contains pw_expire.
 */
/* PWCOMMENT:
 *	This symbol, if defined, indicates to the C program that struct passwd
 *	contains pw_comment.
 */
/* PWGECOS:
 *	This symbol, if defined, indicates to the C program that struct passwd
 *	contains pw_gecos.
 */
/* PWPASSWD:
 *	This symbol, if defined, indicates to the C program that struct passwd
 *	contains pw_passwd.
 */
#$i_pwd I_PWD		/**/
#$d_pwquota PWQUOTA	/**/
#$d_pwage PWAGE	/**/
#$d_pwchange PWCHANGE	/**/
#$d_pwclass PWCLASS	/**/
#$d_pwexpire PWEXPIRE	/**/
#$d_pwcomment PWCOMMENT	/**/
#$d_pwgecos PWGECOS	/**/
#$d_pwpasswd PWPASSWD	/**/

/* I_SHADOW:
 *	This symbol, if defined, indicates that <shadow.h> exists and
 *	should be included.
 */
#$i_shadow	I_SHADOW		/**/

/* I_SOCKS:
 *	This symbol, if defined, indicates that <socks.h> exists and
 *	should be included.
 */
#$i_socks	I_SOCKS		/**/

/* I_SUNMATH:
 *	This symbol, if defined, indicates that <sunmath.h> exists and
 *	should be included.
 */
#$i_sunmath	I_SUNMATH		/**/

/* I_SYSLOG:
 *	This symbol, if defined, indicates that <syslog.h> exists and
 *	should be included.
 */
#$i_syslog	I_SYSLOG		/**/

/* I_SYSMODE:
 *	This symbol, if defined, indicates that <sys/mode.h> exists and
 *	should be included.
 */
#$i_sysmode	I_SYSMODE		/**/

/* I_SYS_MOUNT:
 *	This symbol, if defined, indicates that <sys/mount.h> exists and
 *	should be included.
 */
#$i_sysmount	I_SYS_MOUNT		/**/

/* I_SYS_STATFS:
 *	This symbol, if defined, indicates that <sys/statfs.h> exists.
 */
#$i_sysstatfs	I_SYS_STATFS		/**/

/* I_SYS_STATVFS:
 *	This symbol, if defined, indicates that <sys/statvfs.h> exists and
 *	should be included.
 */
#$i_sysstatvfs	I_SYS_STATVFS		/**/

/* I_SYSUIO:
 *	This symbol, if defined, indicates that <sys/uio.h> exists and
 *	should be included.
 */
#$i_sysuio	I_SYSUIO		/**/

/* I_SYSUTSNAME:
 *	This symbol, if defined, indicates that <sys/utsname.h> exists and
 *	should be included.
 */
#$i_sysutsname	I_SYSUTSNAME		/**/

/* I_SYS_VFS:
 *	This symbol, if defined, indicates that <sys/vfs.h> exists and
 *	should be included.
 */
#$i_sysvfs	I_SYS_VFS		/**/

/* I_TIME:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <time.h>.
 */
/* I_SYS_TIME:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/time.h>.
 */
/* I_SYS_TIME_KERNEL:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/time.h> with KERNEL defined.
 */
#$i_time I_TIME		/**/
#$i_systime I_SYS_TIME		/**/
#$i_systimek I_SYS_TIME_KERNEL		/**/

/* I_USTAT:
 *	This symbol, if defined, indicates that <ustat.h> exists and
 *	should be included.
 */
#$i_ustat	I_USTAT		/**/

/* PERL_INC_VERSION_LIST:
 *	This variable specifies the list of subdirectories in over
 *	which perl.c:incpush() and lib/lib.pm will automatically
 *	search when adding directories to @INC, in a format suitable
 *	for a C initialization string.  See the inc_version_list entry
 *	in Porting/Glossary for more details.
 */
#define PERL_INC_VERSION_LIST $inc_version_list_init		/**/

/* INSTALL_USR_BIN_PERL:
 *	This symbol, if defined, indicates that Perl is to be installed
 * 	also as /usr/bin/perl.
 */
#$installusrbinperl INSTALL_USR_BIN_PERL	/**/

/* PERL_PRIfldbl:
 *	This symbol, if defined, contains the string used by stdio to
 *	format long doubles (format 'f') for output.
 */
/* PERL_PRIgldbl:
 *	This symbol, if defined, contains the string used by stdio to
 *	format long doubles (format 'g') for output.
 */
#$d_PRIfldbl PERL_PRIfldbl	$sPRIfldbl	/**/
#$d_PRIgldbl PERL_PRIgldbl	$sPRIgldbl	/**/

/* Off_t:
 *	This symbol holds the type used to declare offsets in the kernel.
 *	It can be int, long, off_t, etc... It may be necessary to include
 *	<sys/types.h> to get any typedef'ed information.
 */
/* LSEEKSIZE:
 *	This symbol holds the number of bytes used by the Off_t.
 */
/* Off_t_size:
 *	This symbol holds the number of bytes used by the Off_t.
 */
#define Off_t $lseektype		/* <offset> type */
#define LSEEKSIZE $lseeksize		/* <offset> size */
#define Off_t_size $lseeksize	/* <offset> size */

/* Free_t:
 *	This variable contains the return type of free().  It is usually
 * void, but occasionally int.
 */
/* Malloc_t:
 *	This symbol is the type of pointer returned by malloc and realloc.
 */
#define Malloc_t $malloctype			/**/
#define Free_t $freetype			/**/

/* MYMALLOC:
 *	This symbol, if defined, indicates that we're using our own malloc.
 */
#$d_mymalloc MYMALLOC			/**/

/* Mode_t:
 *	This symbol holds the type used to declare file modes 
 *	for systems calls.  It is usually mode_t, but may be
 *	int or unsigned short.  It may be necessary to include <sys/types.h>
 *	to get any typedef'ed information.
 */
#define Mode_t $modetype	 /* file mode parameter for system calls */

/* VAL_O_NONBLOCK:
 *	This symbol is to be used during open() or fcntl(F_SETFL) to turn on
 *	non-blocking I/O for the file descriptor. Note that there is no way
 *	back, i.e. you cannot turn it blocking again this way. If you wish to
 *	alternatively switch between blocking and non-blocking, use the
 *	ioctl(FIOSNBIO) call instead, but that is not supported by all devices.
 */
/* VAL_EAGAIN:
 *	This symbol holds the errno error code set by read() when no data was
 *	present on the non-blocking file descriptor.
 */
/* RD_NODATA:
 *	This symbol holds the return code from read() when no data is present
 *	on the non-blocking file descriptor. Be careful! If EOF_NONBLOCK is
 *	not defined, then you can't distinguish between no data and EOF by
 *	issuing a read(). You'll have to find another way to tell for sure!
 */
/* EOF_NONBLOCK:
 *	This symbol, if defined, indicates to the C program that a read() on
 *	a non-blocking file descriptor will return 0 on EOF, and not the value
 *	held in RD_NODATA (-1 usually, in that case!).
 */
#define VAL_O_NONBLOCK $o_nonblock
#define VAL_EAGAIN $eagain
#define RD_NODATA $rd_nodata
#$d_eofnblk EOF_NONBLOCK

/* Netdb_host_t:
 *	This symbol holds the type used for the 1st argument
 *	to gethostbyaddr().
 */
/* Netdb_hlen_t:
 *	This symbol holds the type used for the 2nd argument
 *	to gethostbyaddr().
 */
/* Netdb_name_t:
 *	This symbol holds the type used for the argument to
 *	gethostbyname().
 */
/* Netdb_net_t:
 *	This symbol holds the type used for the 1st argument to
 *	getnetbyaddr().
 */
#define Netdb_host_t		$netdb_host_type /**/
#define Netdb_hlen_t		$netdb_hlen_type /**/
#define Netdb_name_t		$netdb_name_type /**/
#define Netdb_net_t		$netdb_net_type /**/

/* IVTYPE:
 *	This symbol defines the C type used for Perl's IV.
 */
/* UVTYPE:
 *	This symbol defines the C type used for Perl's UV.
 */
/* I8TYPE:
 *	This symbol defines the C type used for Perl's I8.
 */
/* U8TYPE:
 *	This symbol defines the C type used for Perl's U8.
 */
/* I16TYPE:
 *	This symbol defines the C type used for Perl's I16.
 */
/* U16TYPE:
 *	This symbol defines the C type used for Perl's U16.
 */
/* I32TYPE:
 *	This symbol defines the C type used for Perl's I32.
 */
/* U32TYPE:
 *	This symbol defines the C type used for Perl's U32.
 */
/* I64TYPE:
 *	This symbol defines the C type used for Perl's I64.
 */
/* U64TYPE:
 *	This symbol defines the C type used for Perl's U64.
 */
/* NVTYPE:
 *	This symbol defines the C type used for Perl's NV.
 */
/* IVSIZE:
 *	This symbol contains the sizeof(IV).
 */
/* UVSIZE:
 *	This symbol contains the sizeof(UV).
 */
/* I8SIZE:
 *	This symbol contains the sizeof(I8).
 */
/* U8SIZE:
 *	This symbol contains the sizeof(U8).
 */
/* I16SIZE:
 *	This symbol contains the sizeof(I16).
 */
/* U16SIZE:
 *	This symbol contains the sizeof(U16).
 */
/* I32SIZE:
 *	This symbol contains the sizeof(I32).
 */
/* U32SIZE:
 *	This symbol contains the sizeof(U32).
 */
/* I64SIZE:
 *	This symbol contains the sizeof(I64).
 */
/* U64SIZE:
 *	This symbol contains the sizeof(U64).
 */
/* NV_PRESERVES_UV:
 *	This symbol, if defined, indicates that a variable of type NVTYPE
 *	can preserve all the bit of a variable of type UVSIZE.
 */
#define	IVTYPE		$ivtype		/**/
#define	UVTYPE		$uvtype		/**/
#define	I8TYPE		$i8type		/**/
#define	U8TYPE		$u8type		/**/
#define	I16TYPE		$i16type	/**/
#define	U16TYPE		$u16type	/**/
#define	I32TYPE		$i32type	/**/
#define	U32TYPE		$u32type	/**/
#ifdef HAS_QUAD
#define	I64TYPE		$i64type	/**/
#define	U64TYPE		$u64type	/**/
#endif
#define	NVTYPE		$nvtype		/**/
#define	IVSIZE		$ivsize		/**/
#define	UVSIZE		$uvsize		/**/
#define	I8SIZE		$i8size		/**/
#define	U8SIZE		$u8size		/**/
#define	I16SIZE		$i16size	/**/
#define	U16SIZE		$u16size	/**/
#define	I32SIZE		$i32size	/**/
#define	U32SIZE		$u32size	/**/
#ifdef HAS_QUAD
#define	I64SIZE		$i64size	/**/
#define	U64SIZE		$u64size	/**/
#endif
#$d_nv_preserves_uv	NV_PRESERVES_UV

/* IVdf:
 *	This symbol defines the format string used for printing a Perl IV
 *	as a signed decimal integer.
 */
/* UVuf:
 *	This symbol defines the format string used for printing a Perl UV
 *	as an unsigned decimal integer.
 */
/* UVof:
 *	This symbol defines the format string used for printing a Perl UV
 *	as an unsigned octal integer.
 */
/* UVxf:
 *	This symbol defines the format string used for printing a Perl UV
 *	as an unsigned hexadecimal integer.
 */
#define	IVdf		$ivdformat		/**/
#define	UVuf		$uvuformat		/**/
#define	UVof		$uvoformat		/**/
#define	UVxf		$uvxformat		/**/

/* Pid_t:
 *	This symbol holds the type used to declare process ids in the kernel.
 *	It can be int, uint, pid_t, etc... It may be necessary to include
 *	<sys/types.h> to get any typedef'ed information.
 */
#define Pid_t $pidtype		/* PID type */

/* PRIVLIB:
 *	This symbol contains the name of the private library for this package.
 *	The library is private in the sense that it needn't be in anyone's
 *	execution path, but it should be accessible by the world.  The program
 *	should be prepared to do ~ expansion.
 */
/* PRIVLIB_EXP:
 *	This symbol contains the ~name expanded version of PRIVLIB, to be used
 *	in programs that are not prepared to deal with ~ expansion at run-time.
 */
#define PRIVLIB "$privlib"		/**/
#define PRIVLIB_EXP "$privlibexp"		/**/

/* PTRSIZE:
 *	This symbol contains the size of a pointer, so that the C preprocessor
 *	can make decisions based on it.  It will be sizeof(void *) if
 *	the compiler supports (void *); otherwise it will be
 *	sizeof(char *).
 */
#define PTRSIZE $ptrsize		/**/

/* Drand01:
 *	This macro is to be used to generate uniformly distributed
 *	random numbers over the range [0., 1.[.  You may have to supply
 *	an 'extern double drand48();' in your program since SunOS 4.1.3
 *	doesn't provide you with anything relevant in it's headers.
 *	See HAS_DRAND48_PROTO.
 */
/* Rand_seed_t:
 *	This symbol defines the type of the argument of the
 *	random seed function.
 */
/* seedDrand01:
 *	This symbol defines the macro to be used in seeding the
 *	random number generator (see Drand01).
 */
/* RANDBITS:
 *	This symbol indicates how many bits are produced by the
 *	function used to generate normalized random numbers.
 *	Values include 15, 16, 31, and 48.
 */
#define Drand01()		$drand01		/**/
#define Rand_seed_t		$randseedtype		/**/
#define seedDrand01(x)	$seedfunc((Rand_seed_t)x)	/**/
#define RANDBITS		$randbits		/**/

/* SELECT_MIN_BITS:
 *	This symbol holds the minimum number of bits operated by select.
 *	That is, if you do select(n, ...), how many bits at least will be
 *	cleared in the masks if some activity is detected.  Usually this
 *	is either n or 32*ceil(n/32), especially many little-endians do
 *	the latter.  This is only useful if you have select(), naturally.
 */
#define SELECT_MIN_BITS 	$selectminbits	/**/

/* Select_fd_set_t:
 *	This symbol holds the type used for the 2nd, 3rd, and 4th
 *	arguments to select.  Usually, this is 'fd_set *', if HAS_FD_SET
 *	is defined, and 'int *' otherwise.  This is only useful if you 
 *	have select(), of course.
 */
#define Select_fd_set_t 	$selecttype	/**/

/* SIG_NAME:
 *	This symbol contains a list of signal names in order of
 *	signal number. This is intended
 *	to be used as a static array initialization, like this:
 *		char *sig_name[] = { SIG_NAME };
 *	The signals in the list are separated with commas, and each signal
 *	is surrounded by double quotes. There is no leading SIG in the signal
 *	name, i.e. SIGQUIT is known as "QUIT".
 *	Gaps in the signal numbers (up to NSIG) are filled in with NUMnn,
 *	etc., where nn is the actual signal number (e.g. NUM37).
 *	The signal number for sig_name[i] is stored in sig_num[i].
 *	The last element is 0 to terminate the list with a NULL.  This
 *	corresponds to the 0 at the end of the sig_num list.
 */
/* SIG_NUM:
 *	This symbol contains a list of signal numbers, in the same order as the
 *	SIG_NAME list. It is suitable for static array initialization, as in:
 *		int sig_num[] = { SIG_NUM };
 *	The signals in the list are separated with commas, and the indices
 *	within that list and the SIG_NAME list match, so it's easy to compute
 *	the signal name from a number or vice versa at the price of a small
 *	dynamic linear lookup. 
 *	Duplicates are allowed, but are moved to the end of the list.
 *	The signal number corresponding to sig_name[i] is sig_number[i].
 *	if (i < NSIG) then sig_number[i] == i.  
 *	The last element is 0, corresponding to the 0 at the end of
 *	the sig_name list.
 */
#define SIG_NAME $sig_name_init		/**/
#define SIG_NUM  $sig_num_init		/**/

/* SITEARCH:
 *	This symbol contains the name of the private library for this package.
 *	The library is private in the sense that it needn't be in anyone's
 *	execution path, but it should be accessible by the world.  The program
 *	should be prepared to do ~ expansion.
 *	The standard distribution will put nothing in this directory.
 *	After perl has been installed, users may install their own local
 *	architecture-dependent modules in this directory with
 *		MakeMaker Makefile.PL
 *	or equivalent.  See INSTALL for details.
 */
/* SITEARCH_EXP:
 *	This symbol contains the ~name expanded version of SITEARCH, to be used
 *	in programs that are not prepared to deal with ~ expansion at run-time.
 */
#define SITEARCH "$sitearch"		/**/
#define SITEARCH_EXP "$sitearchexp"		/**/

/* SITELIB:
 *	This symbol contains the name of the private library for this package.
 *	The library is private in the sense that it needn't be in anyone's
 *	execution path, but it should be accessible by the world.  The program
 *	should be prepared to do ~ expansion.
 *	The standard distribution will put nothing in this directory.
 *	After perl has been installed, users may install their own local
 *	architecture-independent modules in this directory with
 *		MakeMaker Makefile.PL
 *	or equivalent.  See INSTALL for details.
 */
/* SITELIB_EXP:
 *	This symbol contains the ~name expanded version of SITELIB, to be used
 *	in programs that are not prepared to deal with ~ expansion at run-time.
 */
/* SITELIB_STEM:
 *	This define is SITELIB_EXP with any trailing version-specific component
 *	removed.  The elements in inc_version_list (inc_version_list.U) can
 *	be tacked onto this variable to generate a list of directories to search.
 */
#define SITELIB "$sitelib"		/**/
#define SITELIB_EXP "$sitelibexp"		/**/
#define SITELIB_STEM "$sitelib_stem"		/**/

/* Size_t_size:
 *	This symbol holds the size of a Size_t in bytes.
 */
#define Size_t_size $sizesize		/* */

/* Size_t:
 *	This symbol holds the type used to declare length parameters
 *	for string functions.  It is usually size_t, but may be
 *	unsigned long, int, etc.  It may be necessary to include
 *	<sys/types.h> to get any typedef'ed information.
 */
#define Size_t $sizetype	 /* length paramater for string functions */

/* Sock_size_t:
 *	This symbol holds the type used for the size argument of
 *	various socket calls (just the base type, not the pointer-to).
 */
#define Sock_size_t		$socksizetype /**/

/* SSize_t:
 *	This symbol holds the type used by functions that return
 *	a count of bytes or an error condition.  It must be a signed type.
 *	It is usually ssize_t, but may be long or int, etc.
 *	It may be necessary to include <sys/types.h> or <unistd.h>
 *	to get any typedef'ed information.
 *	We will pick a type such that sizeof(SSize_t) == sizeof(Size_t).
 */
#define SSize_t $ssizetype	 /* signed count of bytes */

/* STARTPERL:
 *	This variable contains the string to put in front of a perl
 *	script to make sure (one hopes) that it runs with perl and not
 *	some shell.
 */
#define STARTPERL "$startperl"		/**/

/* HAS_STDIO_STREAM_ARRAY:
 *	This symbol, if defined, tells that there is an array
 *	holding the stdio streams.
 */
/* STDIO_STREAM_ARRAY:
 *	This symbol tells the name of the array holding the stdio streams.
 *	Usual values include _iob, __iob, and __sF.
 */
#$d_stdio_stream_array	HAS_STDIO_STREAM_ARRAY	/**/
#define STDIO_STREAM_ARRAY	$stdio_stream_array

/* Uid_t_f:
 *	This symbol defines the format string used for printing a Uid_t.
 */
#define	Uid_t_f		$uidformat		/**/

/* Uid_t_sign:
 *	This symbol holds the signedess of a Uid_t.
 *	1 for unsigned, -1 for signed.
 */
#define Uid_t_sign	$uidsign		/* UID sign */

/* Uid_t_size:
 *	This symbol holds the size of a Uid_t in bytes.
 */
#define Uid_t_size $uidsize		/* UID size */

/* Uid_t:
 *	This symbol holds the type used to declare user ids in the kernel.
 *	It can be int, ushort, uid_t, etc... It may be necessary to include
 *	<sys/types.h> to get any typedef'ed information.
 */
#define Uid_t $uidtype		/* UID type */

/* USE_64_BIT_INT:
 *	This symbol, if defined, indicates that 64-bit integers should
 *	be used when available.  If not defined, the native integers
 *	will be employed (be they 32 or 64 bits).  The minimal possible
 *	64-bitness is used, just enough to get 64-bit integers into Perl.
 *	This may mean using for example "long longs", while your memory
 *	may still be limited to 2 gigabytes.
 */
/* USE_64_BIT_ALL:
 *	This symbol, if defined, indicates that 64-bit integers should
 *	be used when available.  If not defined, the native integers
 *	will be used (be they 32 or 64 bits).  The maximal possible
 *	64-bitness is employed: LP64 or ILP64, meaning that you will
 *	be able to use more than 2 gigabytes of memory.  This mode is
 *	even more binary incompatible than USE_64_BIT_INT. You may not
 *	be able to run the resulting executable in a 32-bit CPU at all or
 *	you may need at least to reboot your OS to 64-bit mode.
 */
#ifndef USE_64_BIT_INT
#$use64bitint	USE_64_BIT_INT		/**/
#endif

#ifndef USE_64_BIT_ALL
#$use64bitall	USE_64_BIT_ALL		/**/
#endif

/* USE_LARGE_FILES:
 *	This symbol, if defined, indicates that large file support
 *	should be used when available.
 */
#ifndef USE_LARGE_FILES
#$uselargefiles	USE_LARGE_FILES		/**/
#endif

/* USE_LONG_DOUBLE:
 *	This symbol, if defined, indicates that long doubles should
 *	be used when available.
 */
#ifndef USE_LONG_DOUBLE
#$uselongdouble	USE_LONG_DOUBLE		/**/
#endif

/* USE_MORE_BITS:
 *	This symbol, if defined, indicates that 64-bit interfaces and
 *	long doubles should be used when available.
 */
#ifndef USE_MORE_BITS
#$usemorebits	USE_MORE_BITS		/**/
#endif

/* MULTIPLICITY:
 *	This symbol, if defined, indicates that Perl should
 *	be built to use multiplicity.
 */
#ifndef MULTIPLICITY
#$usemultiplicity	MULTIPLICITY		/**/
#endif

/* USE_PERLIO:
 *	This symbol, if defined, indicates that the PerlIO abstraction should
 *	be used throughout.  If not defined, stdio should be
 *	used in a fully backward compatible manner.
 */
#ifndef USE_PERLIO
#$useperlio	USE_PERLIO		/**/
#endif

/* USE_SOCKS:
 *	This symbol, if defined, indicates that Perl should
 *	be built to use socks.
 */
#ifndef USE_SOCKS
#$usesocks	USE_SOCKS		/**/
#endif

/* USE_ITHREADS:
 *	This symbol, if defined, indicates that Perl should be built to
 *	use the interpreter-based threading implementation.
 */
/* USE_5005THREADS:
 *	This symbol, if defined, indicates that Perl should be built to
 *	use the 5.005-based threading implementation.
 */
/* OLD_PTHREADS_API:
 *	This symbol, if defined, indicates that Perl should
 *	be built to use the old draft POSIX threads API.
 */
#$use5005threads	USE_5005THREADS		/**/
#$useithreads	USE_ITHREADS		/**/
#if defined(USE_5005THREADS) && !defined(USE_ITHREADS)
#define		USE_THREADS		/* until src is revised*/
#endif
#$d_oldpthreads	OLD_PTHREADS_API		/**/

/* PERL_VENDORARCH:
 *	If defined, this symbol contains the name of a private library.
 *	The library is private in the sense that it needn't be in anyone's
 *	execution path, but it should be accessible by the world.
 *	It may have a ~ on the front. 
 *	The standard distribution will put nothing in this directory.
 *	Vendors who distribute perl may wish to place their own
 *	architecture-dependent modules and extensions in this directory with
 *		MakeMaker Makefile.PL INSTALLDIRS=vendor 
 *	or equivalent.  See INSTALL for details.
 */
/* PERL_VENDORARCH_EXP:
 *	This symbol contains the ~name expanded version of PERL_VENDORARCH, to be used
 *	in programs that are not prepared to deal with ~ expansion at run-time.
 */
#$d_vendorarch PERL_VENDORARCH "$vendorarch"		/**/
#$d_vendorarch PERL_VENDORARCH_EXP "$vendorarchexp"		/**/

/* PERL_VENDORLIB_EXP:
 *	This symbol contains the ~name expanded version of VENDORLIB, to be used
 *	in programs that are not prepared to deal with ~ expansion at run-time.
 */
/* PERL_VENDORLIB_STEM:
 *	This define is PERL_VENDORLIB_EXP with any trailing version-specific component
 *	removed.  The elements in inc_version_list (inc_version_list.U) can
 *	be tacked onto this variable to generate a list of directories to search.
 */
#$d_vendorlib PERL_VENDORLIB_EXP "$vendorlibexp"		/**/
#$d_vendorlib PERL_VENDORLIB_STEM "$vendorlib_stem"		/**/

/* VOIDFLAGS:
 *	This symbol indicates how much support of the void type is given by this
 *	compiler.  What various bits mean:
 *
 *	    1 = supports declaration of void
 *	    2 = supports arrays of pointers to functions returning void
 *	    4 = supports comparisons between pointers to void functions and
 *		    addresses of void functions
 *	    8 = suports declaration of generic void pointers
 *
 *	The package designer should define VOIDUSED to indicate the requirements
 *	of the package.  This can be done either by #defining VOIDUSED before
 *	including config.h, or by defining defvoidused in Myinit.U.  If the
 *	latter approach is taken, only those flags will be tested.  If the
 *	level of void support necessary is not present, defines void to int.
 */
#ifndef VOIDUSED
#define VOIDUSED $defvoidused
#endif
#define VOIDFLAGS $voidflags
#if (VOIDFLAGS & VOIDUSED) != VOIDUSED
#define void int		/* is void to be avoided? */
#define M_VOID			/* Xenix strikes again */
#endif

/* PERL_XS_APIVERSION:
 *	This variable contains the version of the oldest perl binary
 *	compatible with the present perl.  perl.c:incpush() and
 *	lib/lib.pm will automatically search in $sitearch for older
 *	directories across major versions back to xs_apiversion.
 *	This is only useful if you have a perl library directory tree
 *	structured like the default one.
 *	See INSTALL for how this works.
 *	The versioned site_perl directory was introduced in 5.005,
 *	so that is the lowest possible value.
 *	Since this can depend on compile time options (such as
 *	bincompat) it is set by Configure.  Other non-default sources
 *	of potential incompatibility, such as multiplicity, threads,
 *	debugging, 64bits, sfio, etc., are not checked for currently,
 *	though in principle we could go snooping around in old
 *	Config.pm files.
 */
/* PERL_PM_APIVERSION:
 *	This variable contains the version of the oldest perl
 *	compatible with the present perl.  (That is, pure perl modules
 *	written for pm_apiversion will still work for the current
 *	version).  perl.c:incpush() and lib/lib.pm will automatically
 *	search in $sitelib for older directories across major versions
 *	back to pm_apiversion.  This is only useful if you have a perl
 *	library directory tree structured like the default one.  The
 *	versioned site_perl library was introduced in 5.005, so that's
 *	the default setting for this variable.  It's hard to imagine
 *	it changing before Perl6.  It is included here for symmetry
 *	with xs_apiveprsion -- the searching algorithms will
 *	(presumably) be similar.
 *	See the INSTALL file for how this works.
 */
#define PERL_XS_APIVERSION "$xs_apiversion"
#define PERL_PM_APIVERSION "$pm_apiversion"

/* HAS_LCHOWN:
 *	This symbol, if defined, indicates that the lchown routine is
 *	available to operate on a symbolic link (instead of following the
 *	link).
 */
#$d_lchown HAS_LCHOWN		/**/

/* FLEXFILENAMES:
 *	This symbol, if defined, indicates that the system supports filenames
 *	longer than 14 characters.
 */
#$d_flexfnam	FLEXFILENAMES		/**/

/* I_LIBUTIL:
 *	This symbol, if defined, indicates that <libutil.h> exists and
 *	should be included.
 */
#$i_libutil	I_LIBUTIL		/**/

#endif
!GROK!THIS!
