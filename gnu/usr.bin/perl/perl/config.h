#ifndef config_h
#define config_h
/* config.h
 * This file was produced by running the config.h.SH script, which
 * gets its values from config.sh, which is generally produced by
 * running Configure.
 *
 * Feel free to modify any of this as the need arises.  Note, however,
 * that running config.h.SH again will wipe out any changes you've made.
 * For a more permanent change edit config.sh and rerun config.h.SH.
 */
 /*SUPPRESS 460*/


/*#undef	EUNICE		*/
/*#undef	VMS		*/

/* LOC_SED
 *     This symbol holds the complete pathname to the sed program.
 */
#define LOC_SED "/usr/bin/sed"             /**/

/* ALIGN_BYTES
 *	This symbol contains the number of bytes required to align a double.
 *	Usual values are 2, 4, and 8.
 */
#define ALIGN_BYTES 4		/**/

/* BIN
 *	This symbol holds the name of the directory in which the user wants
 *	to keep publicly executable images for the package in question.  It
 *	is most often a local directory such as /usr/local/bin.
 */
#define BIN "/usr/bin"             /**/

/* BYTEORDER
 *	This symbol contains an encoding of the order of bytes in a long.
 *	Usual values (in hex) are 0x1234, 0x4321, 0x2143, 0x3412...
 */
#define BYTEORDER 0x1234		/**/

/* CPPSTDIN
 *	This symbol contains the first part of the string which will invoke
 *	the C preprocessor on the standard input and produce to standard
 *	output.	 Typical value of "cc -E" or "/lib/cpp".
 */
/* CPPMINUS
 *	This symbol contains the second part of the string which will invoke
 *	the C preprocessor on the standard input and produce to standard
 *	output.  This symbol will have the value "-" if CPPSTDIN needs a minus
 *	to specify standard input, otherwise the value is "".
 */
#define CPPSTDIN "cc -E"
#define CPPMINUS "-"

/* HAS_BCMP
 *	This symbol, if defined, indicates that the bcmp routine is available
 *	to compare blocks of memory.  If undefined, use memcmp.  If that's
 *	not available, roll your own.
 */
#define	HAS_BCMP		/**/

/* HAS_BCOPY
 *	This symbol, if defined, indicates that the bcopy routine is available
 *	to copy blocks of memory.  Otherwise you should probably use memcpy().
 *	If neither is defined, roll your own.
 */
/* SAFE_BCOPY
 *	This symbol, if defined, indicates that the bcopy routine is available
 *	to copy potentially overlapping copy blocks of bcopy.  Otherwise you
 *	should probably use memmove() or memcpy().  If neither is defined,
 *	roll your own.
 */
#define	HAS_BCOPY		/**/
#define	SAFE_BCOPY		/**/

/* HAS_BZERO
 *	This symbol, if defined, indicates that the bzero routine is available
 *	to zero blocks of memory.  Otherwise you should probably use memset()
 *	or roll your own.
 */
#define	HAS_BZERO		/**/

/* CASTNEGFLOAT
 *	This symbol, if defined, indicates that this C compiler knows how to
 *	cast negative or large floating point numbers to unsigned longs, ints
 *	and shorts.
 */
/* CASTFLAGS
 *	This symbol contains flags that say what difficulties the compiler
 *	has casting odd floating values to unsigned long:
 *		1 = couldn't cast < 0
 *		2 = couldn't cast >= 0x80000000
 */
#define	CASTNEGFLOAT	/**/
#define	CASTFLAGS 0	/**/

/* CHARSPRINTF
 *	This symbol is defined if this system declares "char *sprintf()" in
 *	stdio.h.  The trend seems to be to declare it as "int sprintf()".  It
 *	is up to the package author to declare sprintf correctly based on the
 *	symbol.
 */
/*#undef	CHARSPRINTF 	*/

/* HAS_CHSIZE
 *	This symbol, if defined, indicates that the chsize routine is available
 *	to truncate files.  You might need a -lx to get this routine.
 */
/*#undef	HAS_CHSIZE		*/

/* HAS_CRYPT
 *	This symbol, if defined, indicates that the crypt routine is available
 *	to encrypt passwords and the like.
 */
#define	HAS_CRYPT		/**/

/* CSH
 *	This symbol, if defined, indicates that the C-shell exists.
 *	If defined, contains the full pathname of csh.
 */
#define CSH "/bin/csh"		/**/

/* DOSUID
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
/*#undef DOSUID		*/

/* HAS_DUP2
 *	This symbol, if defined, indicates that the dup2 routine is available
 *	to dup file descriptors.  Otherwise you should use dup().
 */
#define	HAS_DUP2		/**/

/* HAS_FCHMOD
 *	This symbol, if defined, indicates that the fchmod routine is available
 *	to change mode of opened files.  If unavailable, use chmod().
 */
#define	HAS_FCHMOD		/**/

/* HAS_FCHOWN
 *	This symbol, if defined, indicates that the fchown routine is available
 *	to change ownership of opened files.  If unavailable, use chown().
 */
#define	HAS_FCHOWN		/**/

/* HAS_FCNTL
 *	This symbol, if defined, indicates to the C program that
 *	the fcntl() function exists.
 */
#define	HAS_FCNTL		/**/

/* FLEXFILENAMES
 *	This symbol, if defined, indicates that the system supports filenames
 *	longer than 14 characters.
 */
#define	FLEXFILENAMES		/**/

/* HAS_FLOCK
 *	This symbol, if defined, indicates that the flock() routine is
 *	available to do file locking.
 */
#define	HAS_FLOCK		/**/

/* HAS_GETGROUPS
 *	This symbol, if defined, indicates that the getgroups() routine is
 *	available to get the list of process groups.  If unavailable, multiple
 *	groups are probably not supported.
 */
#define	HAS_GETGROUPS		/**/

/* HAS_GETHOSTENT
 *	This symbol, if defined, indicates that the gethostent() routine is
 *	available to lookup host names in some data base or other.
 */
/*#undef	HAS_GETHOSTENT		*/

/* HAS_GETPGRP
 *	This symbol, if defined, indicates that the getpgrp() routine is
 *	available to get the current process group.
 */
#define	HAS_GETPGRP		/**/

/* HAS_GETPGRP2
 *	This symbol, if defined, indicates that the getpgrp2() (as in DG/UX)
 *	routine is available to get the current process group.
 */
/*#undef	HAS_GETPGRP2		*/

/* HAS_GETPRIORITY
 *	This symbol, if defined, indicates that the getpriority() routine is
 *	available to get a process's priority.
 */
#define	HAS_GETPRIORITY		/**/

/* HAS_HTONS
 *	This symbol, if defined, indicates that the htons routine (and friends)
 *	are available to do network order byte swapping.
 */
/* HAS_HTONL
 *	This symbol, if defined, indicates that the htonl routine (and friends)
 *	are available to do network order byte swapping.
 */
/* HAS_NTOHS
 *	This symbol, if defined, indicates that the ntohs routine (and friends)
 *	are available to do network order byte swapping.
 */
/* HAS_NTOHL
 *	This symbol, if defined, indicates that the ntohl routine (and friends)
 *	are available to do network order byte swapping.
 */
#define	HAS_HTONS	/**/
#define	HAS_HTONL	/**/
#define	HAS_NTOHS	/**/
#define	HAS_NTOHL	/**/

/* index
 *	This preprocessor symbol is defined, along with rindex, if the system
 *	uses the strchr and strrchr routines instead.
 */
/* rindex
 *	This preprocessor symbol is defined, along with index, if the system
 *	uses the strchr and strrchr routines instead.
 */

/* HAS_ISASCII
 *	This symbol, if defined, indicates that the isascii routine is available
 *	to test characters for asciiness.
 */
#define	HAS_ISASCII		/**/

/* HAS_KILLPG
 *	This symbol, if defined, indicates that the killpg routine is available
 *	to kill process groups.  If unavailable, you probably should use kill
 *	with a negative process number.
 */
#define	HAS_KILLPG		/**/

/* HAS_LSTAT
 *	This symbol, if defined, indicates that the lstat() routine is
 *	available to stat symbolic links.
 */
#define	HAS_LSTAT		/**/

/* HAS_MEMCMP
 *	This symbol, if defined, indicates that the memcmp routine is available
 *	to compare blocks of memory.  If undefined, roll your own.
 */
#define	HAS_MEMCMP		/**/

/* HAS_MEMCPY
 *	This symbol, if defined, indicates that the memcpy routine is available
 *	to copy blocks of memory.  Otherwise you should probably use bcopy().
 *	If neither is defined, roll your own.
 */
/* SAFE_MEMCPY
 *	This symbol, if defined, indicates that the memcpy routine is available
 *	to copy potentially overlapping copy blocks of memory.  Otherwise you
 *	should probably use memmove() or bcopy().  If neither is defined,
 *	roll your own.
 */
#define	HAS_MEMCPY		/**/
#define	SAFE_MEMCPY		/**/

/* HAS_MEMMOVE
 *	This symbol, if defined, indicates that the memmove routine is available
 *	to move potentially overlapping blocks of memory.  Otherwise you
 *	should use bcopy() or roll your own.
 */
#define	HAS_MEMMOVE		/**/

/* HAS_MEMSET
 *	This symbol, if defined, indicates that the memset routine is available
 *	to set a block of memory to a character.  If undefined, roll your own.
 */
#define	HAS_MEMSET		/**/

/* HAS_MKDIR
 *	This symbol, if defined, indicates that the mkdir routine is available
 *	to create directories.  Otherwise you should fork off a new process to
 *	exec /bin/mkdir.
 */
#define	HAS_MKDIR		/**/


/* HAS_NDBM
 *	This symbol, if defined, indicates that ndbm.h exists and should
 *	be included.
 */
#define	HAS_NDBM		/**/

/* HAS_ODBM
 *	This symbol, if defined, indicates that dbm.h exists and should
 *	be included.
 */

/* HAS_OPEN3
 *	This manifest constant lets the C program know that the three
 *	argument form of open(2) is available.
 */
#define	HAS_OPEN3		/**/

/* HAS_READDIR
 *	This symbol, if defined, indicates that the readdir routine is available
 *	from the C library to read directories.
 */
#define	HAS_READDIR		/**/

/* HAS_RENAME
 *	This symbol, if defined, indicates that the rename routine is available
 *	to rename files.  Otherwise you should do the unlink(), link(), unlink()
 *	trick.
 */
#define	HAS_RENAME		/**/

/* HAS_REWINDDIR
 *	This symbol, if defined, indicates that the rewindir routine is
 *	available to rewind directories.
 */
#define	HAS_REWINDDIR		/**/

/* HAS_RMDIR
 *	This symbol, if defined, indicates that the rmdir routine is available
 *	to remove directories.  Otherwise you should fork off a new process to
 *	exec /bin/rmdir.
 */
#define	HAS_RMDIR		/**/

/* HAS_SEEKDIR
 *	This symbol, if defined, indicates that the seekdir routine is
 *	available to seek into directories.
 */
#define	HAS_SEEKDIR		/**/

/* HAS_SELECT
 *	This symbol, if defined, indicates that the select() subroutine
 *	exists.
 */
#define	HAS_SELECT	/**/

/* HAS_SETEGID
 *	This symbol, if defined, indicates that the setegid routine is available
 *	to change the effective gid of the current program.
 *	Do not use on systems with _POSIX_SAVED_IDS support.
 */
#define	HAS_SETEGID	/**/

/* HAS_SETEUID
 *	This symbol, if defined, indicates that the seteuid routine is available
 *	to change the effective uid of the current program.
 *	Do not use on systems with _POSIX_SAVED_IDS support.
 */
#define	HAS_SETEUID	/**/

/* HAS_SETPGRP
 *	This symbol, if defined, indicates that the setpgrp() routine is
 *	available to set the current process group.
 */
#define	HAS_SETPGRP		/**/

/* HAS_SETPGRP2
 *	This symbol, if defined, indicates that the setpgrp2() (as in DG/UX)
 *	routine is available to set the current process group.
 */

/* HAS_SETPRIORITY
 *	This symbol, if defined, indicates that the setpriority() routine is
 *	available to set a process's priority.
 */
#define	HAS_SETPRIORITY		/**/

/* HAS_SETREGID
 *	This symbol, if defined, indicates that the setregid routine is
 *	available to change the real and effective gid of the current program.
 */
/* HAS_SETRESGID
 *	This symbol, if defined, indicates that the setresgid routine is
 *	available to change the real, effective and saved gid of the current
 *	program.
 */
#define  HAS_SETREGID            /**/

/* HAS_SETREUID
 *	This symbol, if defined, indicates that the setreuid routine is
 *	available to change the real and effective uid of the current program.
 */
/* HAS_SETRESUID
 *	This symbol, if defined, indicates that the setresuid routine is
 *	available to change the real, effective and saved uid of the current
 *	program.
 */
#define  HAS_SETREUID            /**/

/* HAS_SETRGID
 *	This symbol, if defined, indicates that the setrgid routine is available
 *	to change the real gid of the current program.
 */
#define  HAS_SETRGID             /**/

/* HAS_SETRUID
 *	This symbol, if defined, indicates that the setruid routine is available
 *	to change the real uid of the current program.
 */
#define  HAS_SETRUID             /**/


/* HAS_SOCKET
 *	This symbol, if defined, indicates that the BSD socket interface is
 *	supported.
 */
/* HAS_SOCKETPAIR
 *	This symbol, if defined, indicates that the BSD socketpair call is
 *	supported.
 */
/* OLDSOCKET
 *	This symbol, if defined, indicates that the 4.1c BSD socket interface
 *	is supported instead of the 4.2/4.3 BSD socket interface.
 */
#define	HAS_SOCKET		/**/

#define	HAS_SOCKETPAIR	/**/


/* STATBLOCKS
 *	This symbol is defined if this system has a stat structure declaring
 *	st_blksize and st_blocks.
 */
#define	STATBLOCKS 	/**/

/* STDSTDIO
 *	This symbol is defined if this system has a FILE structure declaring
 *	_ptr and _cnt in stdio.h.
 */

/* STRUCTCOPY
 *	This symbol, if defined, indicates that this C compiler knows how
 *	to copy structures.  If undefined, you'll need to use a block copy
 *	routine of some sort instead.
 */
#define	STRUCTCOPY	/**/

/* HAS_STRERROR
 *	This symbol, if defined, indicates that the strerror() routine is
 *	available to translate error numbers to strings.
 */
#define	HAS_STRERROR		/**/

/* HAS_SYMLINK
 *	This symbol, if defined, indicates that the symlink routine is available
 *	to create symbolic links.
 */
#define	HAS_SYMLINK		/**/

/* HAS_SYSCALL
 *	This symbol, if defined, indicates that the syscall routine is available
 *	to call arbitrary system calls.  If undefined, that's tough.
 */
#define	HAS_SYSCALL		/**/

/* HAS_TELLDIR
 *	This symbol, if defined, indicates that the telldir routine is
 *	available to tell your location in directories.
 */
#define	HAS_TELLDIR		/**/

/* HAS_TRUNCATE
 *	This symbol, if defined, indicates that the truncate routine is
 *	available to truncate files.
 */
#define	HAS_TRUNCATE		/**/

/* HAS_VFORK
 *	This symbol, if defined, indicates that vfork() exists.
 */
#define	HAS_VFORK	/**/

/* VOIDSIG
 *	This symbol is defined if this system declares "void (*signal())()" in
 *	signal.h.  The old way was to declare it as "int (*signal())()".  It
 *	is up to the package author to declare things correctly based on the
 *	symbol.
 */
/* TO_SIGNAL
 *	This symbol's value is either "void" or "int", corresponding to the
 *	appropriate return "type" of a signal handler.  Thus, one can declare
 *	a signal handler using "TO_SIGNAL (*handler())()", and define the
 *	handler using "TO_SIGNAL handler(sig)".
 */
#define	VOIDSIG 	/**/
#define	TO_SIGNAL	int 	/**/

/* HASVOLATILE
 *	This symbol, if defined, indicates that this C compiler knows about
 *	the volatile declaration.
 */
#define	HASVOLATILE	/**/

/* HAS_VPRINTF
 *	This symbol, if defined, indicates that the vprintf routine is available
 *	to printf with a pointer to an argument list.  If unavailable, you
 *	may need to write your own, probably in terms of _doprnt().
 */
/* CHARVSPRINTF
 *	This symbol is defined if this system has vsprintf() returning type
 *	(char*).  The trend seems to be to declare it as "int vsprintf()".  It
 *	is up to the package author to declare vsprintf correctly based on the
 *	symbol.
 */
#define	HAS_VPRINTF	/**/

/* HAS_WAIT4
 *	This symbol, if defined, indicates that wait4() exists.
 */
#define	HAS_WAIT4	/**/

/* HAS_WAITPID
 *	This symbol, if defined, indicates that waitpid() exists.
 */
#define	HAS_WAITPID	/**/

/* GIDTYPE
 *	This symbol has a value like gid_t, int, ushort, or whatever type is
 *	used to declare group ids in the kernel.
 */
#define GIDTYPE gid_t		/**/

/* GROUPSTYPE
 *	This symbol has a value like gid_t, int, ushort, or whatever type is
 *	used in the return value of getgroups().
 */
#define GROUPSTYPE gid_t		/**/

/* I_FCNTL
 *	This manifest constant tells the C program to include <fcntl.h>.
 */

/* I_GDBM
 *	This symbol, if defined, indicates that gdbm.h exists and should
 *	be included.
 */

/* I_GRP
 *	This symbol, if defined, indicates to the C program that it should
 *	include grp.h.
 */
#define	I_GRP		/**/

/* I_NETINET_IN
 *	This symbol, if defined, indicates to the C program that it should
 *	include netinet/in.h.
 */
/* I_SYS_IN
 *	This symbol, if defined, indicates to the C program that it should
 *	include sys/in.h.
 */
#define	I_NETINET_IN		/**/

/* I_PWD
 *	This symbol, if defined, indicates to the C program that it should
 *	include pwd.h.
 */
/* PWQUOTA
 *	This symbol, if defined, indicates to the C program that struct passwd
 *	contains pw_quota.
 */
/* PWAGE
 *	This symbol, if defined, indicates to the C program that struct passwd
 *	contains pw_age.
 */
/* PWCHANGE
 *	This symbol, if defined, indicates to the C program that struct passwd
 *	contains pw_change.
 */
/* PWCLASS
 *	This symbol, if defined, indicates to the C program that struct passwd
 *	contains pw_class.
 */
/* PWEXPIRE
 *	This symbol, if defined, indicates to the C program that struct passwd
 *	contains pw_expire.
 */
/* PWCOMMENT
 *	This symbol, if defined, indicates to the C program that struct passwd
 *	contains pw_comment.
 */
#define	I_PWD		/**/
#define	PWCHANGE	/**/
#define	PWCLASS		/**/
#define	PWEXPIRE	/**/

/* I_SYS_FILE
 *	This manifest constant tells the C program to include <sys/file.h>.
 */
#define	I_SYS_FILE	/**/

/* I_SYSIOCTL
 *	This symbol, if defined, indicates that sys/ioctl.h exists and should
 *	be included.
 */
#define	I_SYSIOCTL		/**/

/* I_TIME
 *	This symbol is defined if the program should include <time.h>.
 */
/* I_SYS_TIME
 *	This symbol is defined if the program should include <sys/time.h>.
 */
/* SYSTIMEKERNEL
 *	This symbol is defined if the program should include <sys/time.h>
 *	with KERNEL defined.
 */
/* I_SYS_SELECT
 *	This symbol is defined if the program should include <sys/select.h>.
 */
#define	I_SYS_TIME 	/**/

/* I_UTIME
 *	This symbol, if defined, indicates to the C program that it should
 *	include utime.h.
 */
#define	I_UTIME		/**/

/* I_VARARGS
 *	This symbol, if defined, indicates to the C program that it should
 *	include varargs.h.
 */
#define	I_VARARGS		/**/

/* I_VFORK
 *	This symbol, if defined, indicates to the C program that it should
 *	include vfork.h.
 */

/* INTSIZE
 *	This symbol contains the size of an int, so that the C preprocessor
 *	can make decisions based on it.
 */
#define INTSIZE 4		/**/

/* I_DIRENT
 *	This symbol, if defined, indicates that the program should use the
 *	P1003-style directory routines, and include <dirent.h>.
 */
/* I_SYS_DIR
 *	This symbol, if defined, indicates that the program should use the
 *	directory functions by including <sys/dir.h>.
 */
/* I_NDIR
 *	This symbol, if defined, indicates that the program should include the
 *	system's version of ndir.h, rather than the one with this package.
 */
/* I_SYS_NDIR
 *	This symbol, if defined, indicates that the program should include the
 *	system's version of sys/ndir.h, rather than the one with this package.
 */
/* I_MY_DIR
 *	This symbol, if defined, indicates that the program should compile
 *	the ndir.c code provided with the package.
 */
/* DIRNAMLEN
 *	This symbol, if defined, indicates to the C program that the length
 *	of directory entry names is provided by a d_namlen field.  Otherwise
 *	you need to do strlen() on the d_name field.
 */
#define	I_DIRENT	/**/

/* MYMALLOC
 *	This symbol, if defined, indicates that we're using our own malloc.
 */
/* MALLOCPTRTYPE
 *	This symbol defines the kind of ptr returned by malloc and realloc.
 */
#define MYMALLOC			/**/

#define MALLOCPTRTYPE void         /**/


/* RANDBITS
 *	This symbol contains the number of bits of random number the rand()
 *	function produces.  Usual values are 15, 16, and 31.
 */
#define RANDBITS 31		/**/

/* SCRIPTDIR
 *	This symbol holds the name of the directory in which the user wants
 *	to keep publicly executable scripts for the package in question.  It
 *	is often a directory that is mounted across diverse architectures.
 */
#define SCRIPTDIR "/usr/bin"             /**/

/* SIG_NAME
 *	This symbol contains an list of signal names in order.
 */
#define SIG_NAME "ZERO","HUP","INT","QUIT","ILL","TRAP","ABRT","EMT","FPE","KILL","BUS","SEGV","SYS","PIPE","ALRM","TERM","URG","STOP","TSTP","CONT","CHLD","TTIN","TTOU","IO","XCPU","XFSZ","VTALRM","PROF","WINCH","INFO","USR1","USR2"		/**/

/* STDCHAR
 *	This symbol is defined to be the type of char used in stdio.h.
 *	It has the values "unsigned char" or "char".
 */
#define STDCHAR char	/**/

/* UIDTYPE
 *	This symbol has a value like uid_t, int, ushort, or whatever type is
 *	used to declare user ids in the kernel.
 */
#define UIDTYPE uid_t		/**/

/* VOIDHAVE
 *	This symbol indicates how much support of the void type is given by this
 *	compiler.  What various bits mean:
 *
 *	    1 = supports declaration of void
 *	    2 = supports arrays of pointers to functions returning void
 *	    4 = supports comparisons between pointers to void functions and
 *		    addresses of void functions
 *
 *	The package designer should define VOIDWANT to indicate the requirements
 *	of the package.  This can be done either by #defining VOIDWANT before
 *	including config.h, or by defining voidwant in Myinit.U.  If the level
 *	of void support necessary is not present, config.h defines void to "int",
 *	VOID to the empty string, and VOIDP to "char *".
 */
/* void
 *	This symbol is used for void casts.  On implementations which support
 *	void appropriately, its value is "void".  Otherwise, its value maps
 *	to "int".
 */
/* VOID
 *	This symbol's value is "void" if the implementation supports void
 *	appropriately.  Otherwise, its value is the empty string.  The primary
 *	use of this symbol is in specifying void parameter lists for function
 *	prototypes.
 */
/* VOIDP
 *	This symbol is used for casting generic pointers.  On implementations
 *	which support void appropriately, its value is "void *".  Otherwise,
 *	its value is "char *".
 */
#ifndef VOIDWANT
#define VOIDWANT 7
#endif
#define VOIDHAVE 7
#if (VOIDHAVE & VOIDWANT) != VOIDWANT
#define void int		/* is void to be avoided? */
#define VOID
#define VOIDP (char *)
#define M_VOID		/* Xenix strikes again */
#else
#define VOID void
#define VOIDP (void *)
#endif

/* PRIVLIB
 *	This symbol contains the name of the private library for this package.
 *	The library is private in the sense that it needn't be in anyone's
 *	execution path, but it should be accessible by the world.  The program
 *	should be prepared to do ~ expansion.
 */
#define PRIVLIB "/usr/share/perl"		/**/

#endif
