/*
 * vi configuration file
 * We try to automatically configure to various compilers and operating
 * systems. Extend the autoconf section as needed.
 */

#ifndef _CONFIG_H
# define _CONFIG_H

/*************************** autoconf section ************************/

/* Commodore-Amiga */
#ifdef	amiga
# define AMIGA		1
# define COMPILED_BY	"Manx Aztec C 5.2b"
# define TINYSTACK	1
#endif

/* standard unix V (?) */
#ifdef	M_SYSV
# define UNIXV		1
# ifdef M_XENIX
#  ifndef M_I386
#   define TINYSTACK	1
#  endif
# endif
# undef COHERENT
#endif

/* xelos system, University of Ulm */
#ifdef	xelos
# define UNIXV		1
#endif

/* BSD UNIX? */
#ifdef bsd
# define BSD		1
#else
# ifdef sun
#  ifndef M_SYSV
#   define BSD		1
#  endif
# endif
#endif

/* Microsoft C: sorry, Watcom does the same thing */
#ifdef	M_I86
# ifndef M_SYSV
#  define MSDOS		1
#  ifdef IBMC2
#   define COMPILED_BY	"IBM C/2 1.00"
#  else
#   define MICROSOFT	1
#   define COMPILED_BY	"Microsoft C 5.10"
#  endif
#  define TINYSTACK	1
# endif
#endif

/* Borland's Turbo C */
#ifdef	__TURBOC__
# define MSDOS		1
# define TURBOC		1
# ifdef __BORLANDC__
# define COMPILED_BY	"Borland C 2.00"
# else
# define COMPILED_BY	(__TURBOC__ >= 661 ? "Turbo C++ 1.00" : "Turbo C 2.00")
# endif
# define TINYSTACK	1
#endif

/* Tos Mark-Williams */
#ifdef	M68000
# define TOS 1
# define COMPILED_BY	"Mark Williams C"
# define TINYSTACK	1
#endif

/* Tos GNU-C */
#ifdef __atarist__
# ifdef __gem__
#  define TOS 1
#  define COMPILED_BY	"GNU-C " __VERSION__
#  define TINYSTACK	1
# endif
#endif

/* OS9/68000 */
#ifdef	OSK
# define COMPILED_BY	"Microware C V2.3 Edition 40"
# define TINYSTACK	1
#endif

/* DEC Rainbow, running MS-DOS (handled by earlier MS-DOS tests) */
/* (would need -DRAINBOW in CFLAGS to compile a Rainbow-compatible .EXE) */

#ifdef VMS
# define COMPILED_BY    "VAX/VMS VAXC compiler"
# undef VMS
# define VMS 1
#endif


#ifdef COHERENT
# ifdef _I386
#  define COH_386 1
#  define COH_286 0
# else
#  define COH_386 0
#  define COH_286 1
# endif
# undef COHERENT
# define COHERENT 1
#endif

/*************************** end of autoconf section ************************/

/* All undefined symbols are defined to zero here, to allow for older    */
/* compilers which dont understand #if defined() or #if UNDEFINED_SYMBOL */

/*************************** operating systems *****************************/
 
#ifndef	BSD
# define BSD	0		/* UNIX - Berkeley 4.x */
#endif

#ifndef	UNIXV
# define UNIXV	0		/* UNIX - AT&T SYSV */
#endif

#ifndef	UNIX7
# define UNIX7	0		/* UNIX - version 7 */
#endif

#ifndef	MSDOS
# define MSDOS	0		/* PC		*/
#endif

#ifndef	TOS
# define TOS	0		/* Atari ST	*/
#endif

#ifndef	AMIGA
# define AMIGA	0		/* Commodore Amiga */
#endif

#ifndef OSK
# define OSK	0		/* OS-9 / 68k */
#endif

#ifndef COHERENT
# define COHERENT 0		/* Coherent */
#endif

#ifndef RAINBOW			/* DEC Rainbow support, under MS-DOS */
# define RAINBOW 0
#endif

#ifndef VMS
# define VMS 0                  /* VAX/VMS */
#endif

				/* Minix has no predefines */
#if !BSD && !UNIXV && !UNIX7 && !MSDOS && !TOS && !AMIGA && !OSK && !COHERENT && !VMS
# define MINIX	1
#else
# define MINIX	0
#endif

				/* generic combination of Unices */
#if UNIXV || UNIX7 || BSD || MINIX || COHERENT
# define ANY_UNIX 1
#else
# define ANY_UNIX 0
#endif

#ifndef TINYSTACK
# define TINYSTACK 0
#endif

/*************************** compilers **************************************/
 
#ifndef	AZTEC_C
# define AZTEC_C	0
#endif

#ifndef	MICROSOFT
# define MICROSOFT	0
#endif

#ifndef	TURBOC
# define TURBOC		0
#endif

/* Should we use "new style" ANSI C prototypes? */
#ifdef __STDC__
# define NEWSTYLE 1
#endif
#ifdef __cplusplus
# define NEWSTYLE 1
#endif
#ifndef NEWSTYLE
# define NEWSTYLE 0
#endif

#if NEWSTYLE
# define P_(s) s
#else
# define P_(s) ()
#endif

/******************************* Credit ************************************/

#if MSDOS
# define CREDIT "Ported to MS-DOS by Guntram Blohm & Martin Patzel"
# if RAINBOW
#  define CREDIT2 "Rainbow support added by Willett Kempton"
# endif
#endif

#if AMIGA
# define CREDIT "Ported to AmigaDOS 2.04 by Mike Rieser & Dale Rahn"
#endif

#if TOS
# define CREDIT "Ported to Atari/TOS by Guntram Blohm & Martin Patzel"
#endif

#if OSK
# define CREDIT	"Ported to Microware OS9/68k by Peter Reinig"
#endif

#if COHERENT
# define CREDIT	"Ported to Coherent by Esa Ahola"
#endif

#if VMS
# define CREDIT "Ported to VAX/VMS by John Campbell"
#endif
/*************************** functions depending on OS *********************/

/* There are two terminal-related functions that we need: ttyread() and
 * ttywrite().  The ttyread() function implements read-with-timeout and is
 * a true function on all systems.  The ttywrite() function is almost always
 * just a macro...
 */
#if !TOS && !AMIGA
# define ttywrite(buf, len)	write(1, buf, (unsigned)(len))	/* raw write */
#endif

/* The strchr() function is an official standard now, so everybody has it
 * except Unix version 7 (which is old) and BSD Unix (which is academic).
 * Those guys use something called index() to do the same thing.
 */
#if BSD || UNIX7 || OSK
# define strchr	index
# define strrchr rindex
#endif
#if !NEWSTYLE
extern char *strchr();
#endif

/* BSD uses bcopy() instead of memcpy() */
#if BSD
# define memcpy(dest, src, siz)	bcopy(src, dest, siz)
#endif

/* BSD uses getwd() instead of getcwd().  The arguments are a little different,
 * but we'll ignore that and hope for the best; adding arguments to the macro
 * would mess up an "extern" declaration of the function.
 *
 * Also, the Coherent-286 uses getwd(), but Coherent-386 uses getcwd()
 */
#if BSD
#ifndef __386BSD__
# define getcwd	getwd
#endif
#endif
#if COH_286
# define getcwd getwd
#endif
extern char *getcwd();

/* text versa binary mode for read/write */
#if !TOS
#define	tread(fd,buf,n)		read(fd,buf,(unsigned)(n))
#define twrite(fd,buf,n)	write(fd,buf,(unsigned)(n))
#endif

/**************************** Compiler quirks *********************************/

/* the UNIX version 7 and (some) TOS compilers, don't allow "void" */
#if UNIX7 || TOS
# define void int
#endif

/* as far as I know, all compilers except version 7 support unsigned char */
/* NEWFLASH: the Minix-ST compiler has subtle problems with unsigned char */
#if UNIX7 || MINIX
# define UCHAR(c)	((c) & 0xff)
# define uchar		char
#else
# define UCHAR(c)	((unsigned char)(c))
# define uchar		unsigned char
#endif

/* Some compilers prefer to have malloc declared as returning a (void *) */
/* ANSI, on the other hand, needs the arguments to free() to be cast */
#ifndef	__STDC__
# if BSD || AMIGA || MINIX
extern void *malloc();
#  define _free_(ptr)	free((void *)ptr)
# else
extern char *malloc();
#  define _free_(ptr)	free((char *)ptr)
# endif
#else
# define _free_(ptr)	free((void *)ptr)
#endif	/* __STDC__ */

/* everybody but Amiga wants lseek declared here */
#if !AMIGA
extern long lseek();
#endif

/* ANSI C has getenv() declared in stdlib.h, which we've already included.
 * Other compilers will need it declared here, though.
 */
#ifndef __STDC__
extern char *getenv();
#endif

/* Signal handler functions used to return an int value, which was ignored.
 * On newer systems, signal handlers are void functions.  Here, we try to
 * guess the proper return type for this system.
 */
#ifdef __STDC__
# define SIGTYPE void
#else
# if MSDOS
#  define SIGTYPE void
# else
#  if UNIXV
#   define SIGTYPE void		/* Note: This is wrong for SCO Xenix. */
#  endif
# endif
#endif
#ifndef SIGTYPE
# define SIGTYPE int
#endif

/******************* Names of files and environment vars **********************/

#if ANY_UNIX
# ifndef TMPDIR
#  if MINIX
#   define TMPDIR	"/usr/tmp"	/* Keep elvis' temp files off RAM disk! */
#  else
#   define TMPDIR	"/var/tmp"		/* directory where temp files live */
#  endif
# endif
# ifndef PRSVDIR
#  define PRSVDIR	"/var/preserve"	/* directory where preserved file live */
# endif
# ifndef PRSVINDEX
#  define PRSVINDEX	"/var/preserve/Index" /* index of files in PRSVDIR */
# endif
# ifndef EXRC
#  define EXRC		".exrc"		/* init file in current directory */
# endif
# define SCRATCHOUT	"%s/soXXXXXX"	/* temp file used as input to filter */
# ifndef SHELL
#  define SHELL		"/bin/sh"	/* default shell */
# endif
# if COHERENT
#  ifndef REDIRECT
#   define REDIRECT	">"		/* Coherent CC writes errors to stdout */
#  endif
# endif
# define gethome(x)	getenv("HOME")
#endif

#if AMIGA		/* Specify AMIGA environment */
# ifndef CC_COMMAND
#  define CC_COMMAND	"cc"		/* generic C compiler */
# endif
# ifndef COLON
#  define COLON		':'		/* Amiga files can also end in `:' */
# endif
# ifndef SYSEXRC
#  define SYSEXRC	"S:" EXRC	/* name of ".exrc" file in system dir */
# endif
# ifndef MAXRCLEN
#  define MAXRCLEN	2048		/* max size of a .exrc file */
# endif
# ifndef NBUFS
#  define NBUFS		10		/* must be at least 3 -- more is better */
# endif
# ifndef NEEDSYNC
#  define NEEDSYNC	TRUE		/* assume ":se sync" by default */
# endif
# ifndef PRSVDIR
#  define PRSVDIR	"Elvis:"	/* directory where preserved file live */
# endif
# ifndef PRSVINDEX
#  define PRSVINDEX	"Elvis:Index"	/* index of files in PRSVDIR */
# endif
# ifndef REDIRECT
#  define REDIRECT	">"		/* Amiga writes errors to stdout */
# endif
# ifndef SCRATCHIN
#  define SCRATCHIN	"%sSIXXXXXX"
# endif
# ifndef SCRATCHOUT
#  define SCRATCHOUT	"%sSOXXXXXX"
# endif
# ifndef SHELL
#  define SHELL		"newshell"	/* default shell */
# endif
# ifndef TERMTYPE
#  define TERMTYPE	"amiga"		/* default termtype */
# endif
# ifndef TMPDIR				/* for AMIGA should end in `:' or `/' */
#  define TMPDIR	"T:"		/* directory where temp files live */
# endif
# ifndef TMPNAME
#  define TMPNAME	"%selv_%x.%x"	/* format of names for temp files */
# endif
# define gethome(x)	getenv("HOME")
#endif

#if MSDOS || TOS
/* do not change TMPNAME and SCRATCH*: they MUST begin with '%s\\'! */
# ifndef TMPDIR
#  define TMPDIR	"C:\\tmp"	/* directory where temp files live */
# endif
# ifndef PRSVDIR
#  define PRSVDIR	"C:\\preserve"	/* directory where preserved file live */
# endif
# ifndef PRSVINDEX
#  define PRSVINDEX	"C:\\preserve\\Index" /* index of files in PRSVDIR */
# endif
# define TMPNAME	"%s\\elv_%x.%x" /* temp file */
# if MSDOS
#  if MICROSOFT
#   define CC_COMMAND	"cl -c"		/* C compiler */
#  else
#   if __BORLANDC__  /* Borland C */
#    define CC_COMMAND	"bcc"		/* C compiler */
#   else
#   if TURBOC        /* Turbo C */
#    define CC_COMMAND	"tcc"		/* C compiler */
#   endif	/* TURBOC */
#   endif	/* BORLANDC */
#  endif		/* MICROSOFT */
# endif		/* MSDOS */
# define SCRATCHIN	"%s\\siXXXXXX"	/* DOS ONLY - output of filter program */
# define SCRATCHOUT	"%s\\soXXXXXX"	/* temp file used as input to filter */
# define SLASH		'\\'
# ifndef SHELL
#  if TOS
#   define SHELL	"shell.ttp"	/* default shell */
#  else
#   define SHELL	"command.com"	/* default shell */
#  endif
# endif
# define NEEDSYNC	TRUE		/* assume ":se sync" by default */
# if TOS && __GNUC__			/* probably on other systems, too */
#  define REDIRECT	"2>"		/* GNUC reports on 2, others on 1 */
#  define CC_COMMAND	"gcc -c"
# else
#  define REDIRECT	">"		/* shell's redirection of stderr */
# endif
#endif

#if VMS
/* do not change TMPNAME, and SCRATCH*: they MUST begin with '%s\\'! */
# ifndef TMPDIR
#  define TMPDIR        "sys$scratch:"  /* directory where temp files live */
# endif
# define TMPNAME        "%selv_%x.%x;1" /* temp file */
# define SCRATCHIN      "%ssiXXXXXX"    /* DOS ONLY - output of filter program */
# define SCRATCHOUT     "%ssoXXXXXX"    /* temp file used as input to filter */
# define SLASH          '\:'  /* Worry point... jdc */
# ifndef SHELL
#   define SHELL        ""      /* default shell */
# endif
# define REDIRECT       ">"             /* shell's redirection of stderr */
# define tread(fd,buf,n)  vms_read(fd,buf,(unsigned)(n))
# define close vms_close
# define lseek vms_lseek
# define unlink vms_delete
# define delete __delete   /* local routine conflicts w/VMS rtl routine. */
# define rpipe vms_rpipe
# define rpclose vms_rpclose
# define ttyread vms_ttyread
# define gethome(x) getenv("HOME")
/* There is no sync() on vms */
# define sync()
/* jdc -- seems VMS external symbols are case insensitive */
# define m_fWord m_fw_ord
# define m_bWord m_bw_ord
# define m_eWord m_ew_ord
# define m_Nsrch m_n_srch
# define m_Fch   m_f_ch
# define m_Tch   m_t_ch
# define v_Xchar v_x_char
/* jdc -- also, braindead vms curses always found by linker. */
# define LINES elvis_LINES
# define COLS  elvis_COLS
# define curscr elvis_curscr
# define stdscr elvis_stdscr
# define initscr elvis_initscr
# define endwin  elvis_endwin
# define wrefresh elvis_wrefresh
#endif

#if OSK
# ifndef TMPDIR
#  define TMPDIR	"/dd/tmp"	   /* directory where temp files live */
# endif
# ifndef PRSVDIR
#  define PRSVDIR	"/dd/usr/preserve" /* directory where preserved file live */
# endif
# ifndef PRSVINDEX
#  define PRSVINDEX	"/dd/usr/preserve/Index" /* index of files in PRSVDIR */
# endif
# ifndef CC_COMMAND
#  define CC_COMMAND	"cc -r"		   /* name of the compiler */
# endif
# ifndef EXRC
#  define EXRC		".exrc"		   /* init file in current directory */
# endif
# define SCRATCHOUT	"%s/soXXXXXX"	   /* temp file used as input to filter */
# ifndef SHELL
#  define SHELL		"shell"		   /* default shell */
# endif
# define FILEPERMS	(S_IREAD|S_IWRITE) /* file permissions used for creat() */
# define REDIRECT	">>-"		   /* shell's redirection of stderr */
# define sync()				   /* OS9 doesn't need a sync() */
# define gethome(x)	getenv("HOME")
#endif

#ifndef	TAGS
# define TAGS		"tags"		/* name of the tags file */
#endif

#ifndef TMPNAME
# define TMPNAME	"%s/elv_%x.%x"	/* format of names for temp files */
#endif

#ifndef EXINIT
# define EXINIT		"EXINIT"	/* name of EXINIT environment variable */
#endif

#ifndef	EXRC
# define EXRC		"elvis.rc"	/* name of ".exrc" file in current dir */
#endif

#ifndef HMEXRC
# define HMEXRC		EXRC		/* name of ".exrc" file in home dir */
#endif

#ifndef	KEYWORDPRG
# define KEYWORDPRG	"ref"
#endif

#ifndef	SCRATCHOUT
# define SCRATCHIN	"%s/SIXXXXXX"
# define SCRATCHOUT	"%s/SOXXXXXX"
#endif

#ifndef ERRLIST
# define ERRLIST	"errs"
#endif

#ifndef	SLASH
# define SLASH		'/'
#endif

#ifndef SHELL
# define SHELL		"shell"
#endif

#ifndef REG
# define REG		register
#endif

#ifndef NEEDSYNC
# define NEEDSYNC	FALSE
#endif

#ifndef FILEPERMS
# define FILEPERMS	0666
#endif

#ifndef PRESERVE
# define PRESERVE	"/usr/libexec/elvispreserve"	/* name of the "preserve" program */
#endif

#ifndef CC_COMMAND
# define CC_COMMAND	"cc -c"
#endif

#ifndef MAKE_COMMAND
# define MAKE_COMMAND	"make"
#endif

#ifndef REDIRECT
# define REDIRECT	"2>"
#endif

#ifndef BLKSIZE
# ifdef CRUNCH
#  define BLKSIZE	1024
# else
#  define BLKSIZE	2048
# endif
#endif

#ifndef KEYBUFSIZE
# define KEYBUFSIZE	1000
#endif

#ifndef MAILER
# define MAILER		"mail"
#endif

#ifndef gethome
extern char *gethome();
#endif

#endif  /* ndef _CONFIG_H */
