/* RCS utility functions */

/* Copyright 1982, 1988, 1989 Walter Tichy
   Copyright 1990, 1991, 1992, 1993, 1994, 1995 Paul Eggert
   Distributed under license by the Free Software Foundation, Inc.

This file is part of RCS.

RCS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

RCS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RCS; see the file COPYING.
If not, write to the Free Software Foundation,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

Report problems and direct all questions to:

    rcs-bugs@cs.purdue.edu

*/




/*
 * Revision 5.20  1995/06/16 06:19:24  eggert
 * (catchsig): Remove `return'.
 * Update FSF address.
 *
 * Revision 5.19  1995/06/02 18:19:00  eggert
 * (catchsigaction): New name for `catchsig', for sa_sigaction signature.
 * Use nRCS even if !has_psiginfo, to remove unused variable warning.
 * (setup_catchsig): Use sa_sigaction only if has_sa_sigaction.
 * Use ENOTSUP only if defined.
 *
 * Revision 5.18  1995/06/01 16:23:43  eggert
 * (catchsig, restoreints, setup_catchsig): Use SA_SIGINFO, not has_psiginfo,
 * to determine whether to use SA_SIGINFO feature,
 * but also check at runtime whether the feature works.
 * (catchsig): If an mmap_signal occurs, report the affected file name.
 * (unsupported_SA_SIGINFO, accessName): New variables.
 * (setup_catchsig): If using SA_SIGINFO, use sa_sigaction, not sa_handler.
 * If SA_SIGINFO fails, fall back on sa_handler method.
 *
 * (readAccessFilenameBuffer, dupSafer, fdSafer, fopenSafer): New functions.
 * (concatenate): Remove.
 *
 * (runv): Work around bad_wait_if_SIGCHLD_ignored bug.
 * Remove reference to OPEN_O_WORK.
 *
 * Revision 5.17  1994/03/20 04:52:58  eggert
 * Specify subprocess input via file descriptor, not file name.
 * Avoid messing with I/O buffers in the child process.
 * Define dup in terms of F_DUPFD if it exists.
 * Move setmtime to rcsedit.c.  Remove lint.
 *
 * Revision 5.16  1993/11/09 17:40:15  eggert
 * -V now prints version on stdout and exits.
 *
 * Revision 5.15  1993/11/03 17:42:27  eggert
 * Use psiginfo and setreuid if available.  Move date2str to maketime.c.
 *
 * Revision 5.14  1992/07/28  16:12:44  eggert
 * Add -V.  has_sigaction overrides sig_zaps_handler.  Fix -M bug.
 * Add mmap_signal, which minimizes signal handling for non-mmap hosts.
 *
 * Revision 5.13  1992/02/17  23:02:28  eggert
 * Work around NFS mmap SIGBUS problem.  Add -T support.
 *
 * Revision 5.12  1992/01/24  18:44:19  eggert
 * Work around NFS mmap bug that leads to SIGBUS core dumps.  lint -> RCS_lint
 *
 * Revision 5.11  1992/01/06  02:42:34  eggert
 * O_BINARY -> OPEN_O_WORK
 * while (E) ; -> while (E) continue;
 *
 * Revision 5.10  1991/10/07  17:32:46  eggert
 * Support piece tables even if !has_mmap.
 *
 * Revision 5.9  1991/08/19  03:13:55  eggert
 * Add spawn() support.  Explicate assumptions about getting invoker's name.
 * Standardize user-visible dates.  Tune.
 *
 * Revision 5.8  1991/04/21  11:58:30  eggert
 * Plug setuid security hole.
 *
 * Revision 5.6  1991/02/26  17:48:39  eggert
 * Fix setuid bug.  Use fread, fwrite more portably.
 * Support waitpid.  Don't assume -1 is acceptable to W* macros.
 * strsave -> str_save (DG/UX name clash)
 *
 * Revision 5.5  1990/12/04  05:18:49  eggert
 * Don't output a blank line after a signal diagnostic.
 * Use -I for prompts and -q for diagnostics.
 *
 * Revision 5.4  1990/11/01  05:03:53  eggert
 * Remove unneeded setid check.  Add awrite(), fremember().
 *
 * Revision 5.3  1990/10/06  00:16:45  eggert
 * Don't fread F if feof(F).
 *
 * Revision 5.2  1990/09/04  08:02:31  eggert
 * Store fread()'s result in an fread_type object.
 *
 * Revision 5.1  1990/08/29  07:14:07  eggert
 * Declare getpwuid() more carefully.
 *
 * Revision 5.0  1990/08/22  08:13:46  eggert
 * Add setuid support.  Permit multiple locks per user.
 * Remove compile-time limits; use malloc instead.
 * Switch to GMT.  Permit dates past 1999/12/31.
 * Add -V.  Remove snooping.  Ansify and Posixate.
 * Tune.  Some USG hosts define NSIG but not sys_siglist.
 * Don't run /bin/sh if it's hopeless.
 * Don't leave garbage behind if the output is an empty pipe.
 * Clean up after SIGXCPU or SIGXFSZ.  Print name of signal that caused cleanup.
 *
 * Revision 4.6  89/05/01  15:13:40  narten
 * changed copyright header to reflect current distribution rules
 *
 * Revision 4.5  88/11/08  16:01:02  narten
 * corrected use of varargs routines
 *
 * Revision 4.4  88/08/09  19:13:24  eggert
 * Check for memory exhaustion.
 * Permit signal handlers to yield either 'void' or 'int'; fix oldSIGINT botch.
 * Use execv(), not system(); yield exit status like diff(1)'s.
 *
 * Revision 4.3  87/10/18  10:40:22  narten
 * Updating version numbers. Changes relative to 1.1 actually
 * relative to 4.1
 *
 * Revision 1.3  87/09/24  14:01:01  narten
 * Sources now pass through lint (if you ignore printf/sprintf/fprintf
 * warnings)
 *
 * Revision 1.2  87/03/27  14:22:43  jenkins
 * Port to suns
 *
 * Revision 4.1  83/05/10  15:53:13  wft
 * Added getcaller() and findlock().
 * Changed catchints() to check SIGINT for SIG_IGN before setting up the signal
 * (needed for background jobs in older shells). Added restoreints().
 * Removed printing of full RCS path from logcommand().
 *
 * Revision 3.8  83/02/15  15:41:49  wft
 * Added routine fastcopy() to copy remainder of a file in blocks.
 *
 * Revision 3.7  82/12/24  15:25:19  wft
 * added catchints(), ignoreints() for catching and ingnoring interrupts;
 * fixed catchsig().
 *
 * Revision 3.6  82/12/08  21:52:05  wft
 * Using DATEFORM to format dates.
 *
 * Revision 3.5  82/12/04  18:20:49  wft
 * Replaced SNOOPDIR with SNOOPFILE; changed addlock() to update
 * lockedby-field.
 *
 * Revision 3.4  82/12/03  17:17:43  wft
 * Added check to addlock() ensuring only one lock per person.
 * Addlock also returns a pointer to the lock created. Deleted fancydate().
 *
 * Revision 3.3  82/11/27  12:24:37  wft
 * moved rmsema(), trysema(), trydiraccess(), getfullRCSname() to rcsfnms.c.
 * Introduced macro SNOOP so that snoop can be placed in directory other than
 * TARGETDIR. Changed %02d to %.2d for compatibility reasons.
 *
 * Revision 3.2  82/10/18  21:15:11  wft
 * added function getfullRCSname().
 *
 * Revision 3.1  82/10/13  16:17:37  wft
 * Cleanup message is now suppressed in quiet mode.
 */




#include "rcsbase.h"

libId(utilId, "$Id: rcsutil.c,v 1.6 1997/02/22 15:47:43 peter Exp $")

#if !has_memcmp
	int
memcmp(s1, s2, n)
	void const *s1, *s2;
	size_t n;
{
	register unsigned char const
		*p1 = (unsigned char const*)s1,
		*p2 = (unsigned char const*)s2;
	register size_t i = n;
	register int r = 0;
	while (i--  &&  !(r = (*p1++ - *p2++)))
		;
	return r;
}
#endif

#if !has_memcpy
	void *
memcpy(s1, s2, n)
	void *s1;
	void const *s2;
	size_t n;
{
	register char *p1 = (char*)s1;
	register char const *p2 = (char const*)s2;
	while (n--)
		*p1++ = *p2++;
	return s1;
}
#endif

#if RCS_lint
	malloc_type lintalloc;
#endif

/*
 * list of blocks allocated with ftestalloc()
 * These blocks can be freed by ffree when we're done with the current file.
 * We could put the free block inside struct alloclist, rather than a pointer
 * to the free block, but that would be less portable.
 */
struct alloclist {
	malloc_type alloc;
	struct alloclist *nextalloc;
};
static struct alloclist *alloced;


	static malloc_type okalloc P((malloc_type));
	static malloc_type
okalloc(p)
	malloc_type p;
{
	if (!p)
		faterror("out of memory");
	return p;
}

	malloc_type
testalloc(size)
	size_t size;
/* Allocate a block, testing that the allocation succeeded.  */
{
	return okalloc(malloc(size));
}

	malloc_type
testrealloc(ptr, size)
	malloc_type ptr;
	size_t size;
/* Reallocate a block, testing that the allocation succeeded.  */
{
	return okalloc(realloc(ptr, size));
}

	malloc_type
fremember(ptr)
	malloc_type ptr;
/* Remember PTR in 'alloced' so that it can be freed later.  Yield PTR.  */
{
	register struct alloclist *q = talloc(struct alloclist);
	q->nextalloc = alloced;
	alloced = q;
	return q->alloc = ptr;
}

	malloc_type
ftestalloc(size)
	size_t size;
/* Allocate a block, putting it in 'alloced' so it can be freed later. */
{
	return fremember(testalloc(size));
}

	void
ffree()
/* Free all blocks allocated with ftestalloc().  */
{
	register struct alloclist *p, *q;
	for (p = alloced;  p;  p = q) {
		q = p->nextalloc;
		tfree(p->alloc);
		tfree(p);
	}
	alloced = 0;
}

	void
ffree1(f)
	register char const *f;
/* Free the block f, which was allocated by ftestalloc.  */
{
	register struct alloclist *p, **a = &alloced;

	while ((p = *a)->alloc  !=  f)
		a = &p->nextalloc;
	*a = p->nextalloc;
	tfree(p->alloc);
	tfree(p);
}

	char *
str_save(s)
	char const *s;
/* Save s in permanently allocated storage. */
{
	return strcpy(tnalloc(char, strlen(s)+1), s);
}

	char *
fstr_save(s)
	char const *s;
/* Save s in storage that will be deallocated when we're done with this file. */
{
	return strcpy(ftnalloc(char, strlen(s)+1), s);
}

	char *
cgetenv(name)
	char const *name;
/* Like getenv(), but yield a copy; getenv() can overwrite old results. */
{
	register char *p;

	return (p=getenv(name)) ? str_save(p) : p;
}

	char const *
getusername(suspicious)
	int suspicious;
/* Get the caller's login name.  Trust only getwpuid if SUSPICIOUS.  */
{
	static char *name;

	if (!name) {
		if (
		    /* Prefer getenv() unless suspicious; it's much faster.  */
#		    if getlogin_is_secure
			    (suspicious
			    || (
				    !(name = cgetenv("LOGNAME"))
				&&  !(name = cgetenv("USER"))
			    ))
			&&  !(name = getlogin())
#		    else
			suspicious
			|| (
				!(name = cgetenv("LOGNAME"))
			    &&  !(name = cgetenv("USER"))
			    &&  !(name = getlogin())
			)
#		    endif
		) {
#if has_getuid && has_getpwuid
			struct passwd const *pw = getpwuid(ruid());
			if (!pw)
			    faterror("no password entry for userid %lu",
				     (unsigned long)ruid()
			    );
			name = pw->pw_name;
#else
#if has_setuid
			faterror("setuid not supported");
#else
			faterror("Who are you?  Please setenv LOGNAME.");
#endif
#endif
		}
		checksid(name);
	}
	return name;
}




#if has_signal

/*
 *	 Signal handling
 *
 * Standard C places too many restrictions on signal handlers.
 * We obey as many of them as we can.
 * Posix places fewer restrictions, and we are Posix-compatible here.
 */

static sig_atomic_t volatile heldsignal, holdlevel;
#ifdef SA_SIGINFO
	static int unsupported_SA_SIGINFO;
	static siginfo_t bufsiginfo;
	static siginfo_t *volatile heldsiginfo;
#endif


#if has_NFS && has_mmap && large_memory && mmap_signal
    static char const *accessName;

	  void
    readAccessFilenameBuffer(filename, p)
	char const *filename;
	unsigned char const *p;
    {
	unsigned char volatile t;
	accessName = filename;
	t = *p;
	accessName = 0;
    }
#else
#   define accessName ((char const *) 0)
#endif


#if !has_psignal

# define psignal my_psignal
	static void my_psignal P((int,char const*));
	static void
my_psignal(sig, s)
	int sig;
	char const *s;
{
	char const *sname = "Unknown signal";
#	if has_sys_siglist && defined(NSIG)
	    if ((unsigned)sig < NSIG)
		sname = sys_siglist[sig];
#	else
	    switch (sig) {
#	       ifdef SIGHUP
		case SIGHUP:	sname = "Hangup";  break;
#	       endif
#	       ifdef SIGINT
		case SIGINT:	sname = "Interrupt";  break;
#	       endif
#	       ifdef SIGPIPE
		case SIGPIPE:	sname = "Broken pipe";  break;
#	       endif
#	       ifdef SIGQUIT
		case SIGQUIT:	sname = "Quit";  break;
#	       endif
#	       ifdef SIGTERM
		case SIGTERM:	sname = "Terminated";  break;
#	       endif
#	       ifdef SIGXCPU
		case SIGXCPU:	sname = "Cputime limit exceeded";  break;
#	       endif
#	       ifdef SIGXFSZ
		case SIGXFSZ:	sname = "Filesize limit exceeded";  break;
#	       endif
#	      if has_mmap && large_memory
#	       if defined(SIGBUS) && mmap_signal==SIGBUS
		case SIGBUS:	sname = "Bus error";  break;
#	       endif
#	       if defined(SIGSEGV) && mmap_signal==SIGSEGV
		case SIGSEGV:	sname = "Segmentation fault";  break;
#	       endif
#	      endif
	    }
#	endif

	/* Avoid calling sprintf etc., in case they're not reentrant.  */
	{
	    char const *p;
	    char buf[BUFSIZ], *b = buf;
	    for (p = s;  *p;  *b++ = *p++)
		continue;
	    *b++ = ':';
	    *b++ = ' ';
	    for (p = sname;  *p;  *b++ = *p++)
		continue;
	    *b++ = '\n';
	    VOID write(STDERR_FILENO, buf, b - buf);
	}
}
#endif

static signal_type catchsig P((int));
#ifdef SA_SIGINFO
	static signal_type catchsigaction P((int,siginfo_t*,void*));
#endif

	static signal_type
catchsig(s)
	int s;
#ifdef SA_SIGINFO
{
	catchsigaction(s, (siginfo_t *)0, (void *)0);
}
	static signal_type
catchsigaction(s, i, c)
	int s;
	siginfo_t *i;
	void *c;
#endif
{
#   if sig_zaps_handler
	/* If a signal arrives before we reset the handler, we lose. */
	VOID signal(s, SIG_IGN);
#   endif

#   ifdef SA_SIGINFO
	if (!unsupported_SA_SIGINFO)
	    i = 0;
#   endif

    if (holdlevel) {
	heldsignal = s;
#	ifdef SA_SIGINFO
	    if (i) {
		bufsiginfo = *i;
		heldsiginfo = &bufsiginfo;
	    }
#	endif
	return;
    }

    ignoreints();
    setrid();
    if (!quietflag) {
	/* Avoid calling sprintf etc., in case they're not reentrant.  */
	char const *p;
	char buf[BUFSIZ], *b = buf;

	if ( !	(
#		if has_mmap && large_memory && mmap_signal
			/* Check whether this signal was planned.  */
			s == mmap_signal && accessName
#		else
			0
#		endif
	)) {
	    char const *nRCS = "\nRCS";
#	    if defined(SA_SIGINFO) && has_si_errno && has_mmap && large_memory && mmap_signal
		if (s == mmap_signal  &&  i  &&  i->si_errno) {
		    errno = i->si_errno;
		    perror(nRCS++);
		}
#	    endif
#	    if defined(SA_SIGINFO) && has_psiginfo
		if (i)
		    psiginfo(i, nRCS);
		else
		    psignal(s, nRCS);
#	    else
		psignal(s, nRCS);
#	    endif
	}

	for (p = "RCS: ";  *p;  *b++ = *p++)
	    continue;
#	if has_mmap && large_memory && mmap_signal
	    if (s == mmap_signal) {
		p = accessName;
		if (!p)
		    p = "Was a file changed by some other process?  ";
		else {
		    char const *p1;
		    for (p1 = p;  *p1;  p1++)
			continue;
		    VOID write(STDERR_FILENO, buf, b - buf);
		    VOID write(STDERR_FILENO, p, p1 - p);
		    b = buf;
		    p = ": Permission denied.  ";
		}
		while (*p)
		    *b++ = *p++;
	    }
#	endif
	for (p = "Cleaning up.\n";  *p;  *b++ = *p++)
	    continue;
	VOID write(STDERR_FILENO, buf, b - buf);
    }
    exiterr();
}

	void
ignoreints()
{
	++holdlevel;
}

	void
restoreints()
{
	if (!--holdlevel && heldsignal)
#	    ifdef SA_SIGINFO
		VOID catchsigaction(heldsignal, heldsiginfo, (void *)0);
#	    else
		VOID catchsig(heldsignal);
#	    endif
}


static void setup_catchsig P((int const*,int));

#if has_sigaction

	static void check_sig P((int));
	static void
  check_sig(r)
	int r;
  {
	if (r != 0)
		efaterror("signal handling");
  }

	static void
  setup_catchsig(sig, sigs)
	int const *sig;
	int sigs;
  {
	register int i, j;
	struct sigaction act;

	for (i=sigs; 0<=--i; ) {
	    check_sig(sigaction(sig[i], (struct sigaction*)0, &act));
	    if (act.sa_handler != SIG_IGN) {
		act.sa_handler = catchsig;
#		ifdef SA_SIGINFO
		    if (!unsupported_SA_SIGINFO) {
#			if has_sa_sigaction
			    act.sa_sigaction = catchsigaction;
#			else
			    act.sa_handler = catchsigaction;
#			endif
			act.sa_flags |= SA_SIGINFO;
		    }
#		endif
		for (j=sigs; 0<=--j; )
		    check_sig(sigaddset(&act.sa_mask, sig[j]));
		if (sigaction(sig[i], &act, (struct sigaction*)0) != 0) {
#		    if defined(SA_SIGINFO) && defined(ENOTSUP)
			if (errno == ENOTSUP  &&  !unsupported_SA_SIGINFO) {
			    /* Turn off use of SA_SIGINFO and try again.  */
			    unsupported_SA_SIGINFO = 1;
			    i++;
			    continue;
			}
#		    endif
		    check_sig(-1);
		}
	    }
	}
  }

#else
#if has_sigblock

	static void
  setup_catchsig(sig, sigs)
	int const *sig;
	int sigs;
  {
	register int i;
	int mask;

	mask = 0;
	for (i=sigs; 0<=--i; )
		mask |= sigmask(sig[i]);
	mask = sigblock(mask);
	for (i=sigs; 0<=--i; )
		if (
		    signal(sig[i], catchsig) == SIG_IGN  &&
		    signal(sig[i], SIG_IGN) != catchsig
		)
			faterror("signal catcher failure");
	VOID sigsetmask(mask);
  }

#else

	static void
  setup_catchsig(sig, sigs)
	int const *sig;
	int sigs;
  {
	register i;

	for (i=sigs; 0<=--i; )
		if (
		    signal(sig[i], SIG_IGN) != SIG_IGN  &&
		    signal(sig[i], catchsig) != SIG_IGN
		)
			faterror("signal catcher failure");
  }

#endif
#endif


static int const regsigs[] = {
# ifdef SIGHUP
	SIGHUP,
# endif
# ifdef SIGINT
	SIGINT,
# endif
# ifdef SIGPIPE
	SIGPIPE,
# endif
# ifdef SIGQUIT
	SIGQUIT,
# endif
# ifdef SIGTERM
	SIGTERM,
# endif
# ifdef SIGXCPU
	SIGXCPU,
# endif
# ifdef SIGXFSZ
	SIGXFSZ,
# endif
};

	void
catchints()
{
	static int catching_ints;
	if (!catching_ints) {
	    catching_ints = true;
	    setup_catchsig(regsigs, (int) (sizeof(regsigs)/sizeof(*regsigs)));
	}
}

#if has_mmap && large_memory && mmap_signal

    /*
    * If you mmap an NFS file, and someone on another client removes the last
    * link to that file, and you later reference an uncached part of that file,
    * you'll get a SIGBUS or SIGSEGV (depending on the operating system).
    * Catch the signal and report the problem to the user.
    * Unfortunately, there's no portable way to differentiate between this
    * problem and actual bugs in the program.
    * This NFS problem is rare, thank goodness.
    *
    * This can also occur if someone truncates the file, even without NFS.
    */

    static int const mmapsigs[] = { mmap_signal };

	    void
    catchmmapints()
    {
	static int catching_mmap_ints;
	if (!catching_mmap_ints) {
	    catching_mmap_ints = true;
	    setup_catchsig(mmapsigs, (int)(sizeof(mmapsigs)/sizeof(*mmapsigs)));
	}
    }
#endif

#endif /* has_signal */


	void
fastcopy(inf,outf)
	register RILE *inf;
	FILE *outf;
/* Function: copies the remainder of file inf to outf.
 */
{
#if large_memory
#	if maps_memory
	    awrite((char const*)inf->ptr, (size_t)(inf->lim - inf->ptr), outf);
	    inf->ptr = inf->lim;
#	else
	    for (;;) {
		awrite((char const*)inf->ptr, (size_t)(inf->readlim - inf->ptr), outf);
		inf->ptr = inf->readlim;
		if (inf->ptr == inf->lim)
		    break;
		VOID Igetmore(inf);
	    }
#	endif
#else
	char buf[BUFSIZ*8];
	register fread_type rcount;

        /*now read the rest of the file in blocks*/
	while (!feof(inf)) {
		if (!(rcount = Fread(buf,sizeof(*buf),sizeof(buf),inf))) {
			testIerror(inf);
			return;
		}
		awrite(buf, (size_t)rcount, outf);
        }
#endif
}

#ifndef SSIZE_MAX
 /* This does not work in #ifs, but it's good enough for us.  */
 /* Underestimating SSIZE_MAX may slow us down, but it won't break us.  */
#	define SSIZE_MAX ((unsigned)-1 >> 1)
#endif

	void
awrite(buf, chars, f)
	char const *buf;
	size_t chars;
	FILE *f;
{
	/* Posix 1003.1-1990 ssize_t hack */
	while (SSIZE_MAX < chars) {
		if (Fwrite(buf, sizeof(*buf), SSIZE_MAX, f)  !=  SSIZE_MAX)
			Oerror();
		buf += SSIZE_MAX;
		chars -= SSIZE_MAX;
	}

	if (Fwrite(buf, sizeof(*buf), chars, f)  !=  chars)
		Oerror();
}

/* dup a file descriptor; the result must not be stdin, stdout, or stderr.  */
	static int dupSafer P((int));
	static int
dupSafer(fd)
	int fd;
{
#	ifdef F_DUPFD
	    return fcntl(fd, F_DUPFD, STDERR_FILENO + 1);
#	else
	    int e, f, i, used = 0;
	    while (STDIN_FILENO <= (f = dup(fd))  &&  f <= STDERR_FILENO)
		    used |= 1<<f;
	    e = errno;
	    for (i = STDIN_FILENO;  i <= STDERR_FILENO;  i++)
		    if (used & (1<<i))
			    VOID close(i);
	    errno = e;
	    return f;
#	endif
}

/* Renumber a file descriptor so that it's not stdin, stdout, or stderr.  */
	int
fdSafer(fd)
	int fd;
{
	if (STDIN_FILENO <= fd  &&  fd <= STDERR_FILENO) {
		int f = dupSafer(fd);
		int e = errno;
		VOID close(fd);
		errno = e;
		fd = f;
	}
	return fd;
}

/* Like fopen, except the result is never stdin, stdout, or stderr.  */
	FILE *
fopenSafer(filename, type)
	char const *filename;
	char const *type;
{
	FILE *stream = fopen(filename, type);
	if (stream) {
		int fd = fileno(stream);
		if (STDIN_FILENO <= fd  &&  fd <= STDERR_FILENO) {
			int f = dupSafer(fd);
			if (f < 0) {
				int e = errno;
				VOID fclose(stream);
				errno = e;
				return 0;
			}
			if (fclose(stream) != 0) {
				int e = errno;
				VOID close(f);
				errno = e;
				return 0;
			}
			stream = fdopen(f, type);
		}
	}
	return stream;
}


#ifdef F_DUPFD
#	undef dup
#	define dup(fd) fcntl(fd, F_DUPFD, 0)
#endif


#if has_fork || has_spawn

	static int movefd P((int,int));
	static int
movefd(old, new)
	int old, new;
{
	if (old < 0  ||  old == new)
		return old;
#	ifdef F_DUPFD
		new = fcntl(old, F_DUPFD, new);
#	else
		new = dup2(old, new);
#	endif
	return close(old)==0 ? new : -1;
}

	static int fdreopen P((int,char const*,int));
	static int
fdreopen(fd, file, flags)
	int fd;
	char const *file;
	int flags;
{
	int newfd;
	VOID close(fd);
	newfd =
#if !open_can_creat
		flags&O_CREAT ? creat(file, S_IRUSR|S_IWUSR) :
#endif
		open(file, flags, S_IRUSR|S_IWUSR);
	return movefd(newfd, fd);
}

#if has_spawn
	static void redirect P((int,int));
	static void
redirect(old, new)
	int old, new;
/*
* Move file descriptor OLD to NEW.
* If OLD is -1, do nothing.
* If OLD is -2, just close NEW.
*/
{
	if ((old != -1 && close(new) != 0) || (0 <= old && movefd(old,new) < 0))
		efaterror("spawn I/O redirection");
}
#endif


#else /* !has_fork && !has_spawn */

	static void bufargcat P((struct buf*,int,char const*));
	static void
bufargcat(b, c, s)
	register struct buf *b;
	int c;
	register char const *s;
/* Append to B a copy of C, plus a quoted copy of S.  */
{
	register char *p;
	register char const *t;
	size_t bl, sl;

	for (t=s, sl=0;  *t;  )
		sl  +=  3*(*t++=='\'') + 1;
	bl = strlen(b->string);
	bufrealloc(b, bl + sl + 4);
	p = b->string + bl;
	*p++ = c;
	*p++ = '\'';
	while (*s) {
		if (*s == '\'') {
			*p++ = '\'';
			*p++ = '\\';
			*p++ = '\'';
		}
		*p++ = *s++;
	}
	*p++ = '\'';
	*p = 0;
}

#endif

#if !has_spawn && has_fork
/*
* Output the string S to stderr, without touching any I/O buffers.
* This is useful if you are a child process, whose buffers are usually wrong.
* Exit immediately if the write does not completely succeed.
*/
static void write_stderr P((char const *));
	static void
write_stderr(s)
	char const *s;
{
	size_t slen = strlen(s);
	if (write(STDERR_FILENO, s, slen) != slen)
		_exit(EXIT_TROUBLE);
}
#endif

/*
* Run a command.
* infd, if not -1, is the input file descriptor.
* outname, if nonzero, is the name of the output file.
* args[1..] form the command to be run; args[0] might be modified.
*/
	int
runv(infd, outname, args)
	int infd;
	char const *outname, **args;
{
	int wstatus;

#if bad_wait_if_SIGCHLD_ignored
	static int fixed_SIGCHLD;
	if (!fixed_SIGCHLD) {
	    fixed_SIGCHLD = true;
#	    ifndef SIGCHLD
#	    define SIGCHLD SIGCLD
#	    endif
	    VOID signal(SIGCHLD, SIG_DFL);
	}
#endif

	oflush();
	eflush();
    {
#if has_spawn
	int in, out;
	char const *file;

	in = -1;
	if (infd != -1  &&  infd != STDIN_FILENO) {
	    if ((in = dup(STDIN_FILENO)) < 0) {
		if (errno != EBADF)
		    efaterror("spawn input setup");
		in = -2;
	    } else {
#		ifdef F_DUPFD
		    if (close(STDIN_FILENO) != 0)
			efaterror("spawn input close");
#		endif
	    }
	    if (
#		ifdef F_DUPFD
		    fcntl(infd, F_DUPFD, STDIN_FILENO) != STDIN_FILENO
#		else
		    dup2(infd, STDIN_FILENO) != STDIN_FILENO
#		endif
	    )
		efaterror("spawn input redirection");
	}

	out = -1;
	if (outname) {
	    if ((out = dup(STDOUT_FILENO)) < 0) {
		if (errno != EBADF)
		    efaterror("spawn output setup");
		out = -2;
	    }
	    if (fdreopen(
		STDOUT_FILENO, outname,
		O_CREAT | O_TRUNC | O_WRONLY
	    ) < 0)
		efaterror(outname);
	}

	wstatus = spawn_RCS(0, args[1], (char**)(args + 1));
#	ifdef RCS_SHELL
	    if (wstatus == -1  &&  errno == ENOEXEC) {
		args[0] = RCS_SHELL;
		wstatus = spawnv(0, args[0], (char**)args);
	    }
#	endif
	redirect(in, STDIN_FILENO);
	redirect(out, STDOUT_FILENO);
#else
#if has_fork
	pid_t pid;
	if (!(pid = vfork())) {
		char const *notfound;
		if (infd != -1  &&  infd != STDIN_FILENO  &&  (
#		    ifdef F_DUPFD
			(VOID close(STDIN_FILENO),
			fcntl(infd, F_DUPFD, STDIN_FILENO) != STDIN_FILENO)
#		    else
			dup2(infd, STDIN_FILENO) != STDIN_FILENO
#		    endif
		)) {
		    /* Avoid perror since it may misuse buffers.  */
		    write_stderr(args[1]);
		    write_stderr(": I/O redirection failed\n");
		    _exit(EXIT_TROUBLE);
		}

		if (outname)
		    if (fdreopen(
			STDOUT_FILENO, outname,
			O_CREAT | O_TRUNC | O_WRONLY
		    ) < 0) {
			/* Avoid perror since it may misuse buffers.  */
			write_stderr(args[1]);
			write_stderr(": ");
			write_stderr(outname);
			write_stderr(": cannot create\n");
			_exit(EXIT_TROUBLE);
		    }
		VOID exec_RCS(args[1], (char**)(args + 1));
		notfound = args[1];
#		ifdef RCS_SHELL
		    if (errno == ENOEXEC) {
			args[0] = notfound = RCS_SHELL;
			VOID execv(args[0], (char**)args);
		    }
#		endif

		/* Avoid perror since it may misuse buffers.  */
		write_stderr(notfound);
		write_stderr(": not found\n");
		_exit(EXIT_TROUBLE);
	}
	if (pid < 0)
		efaterror("fork");
#	if has_waitpid
		if (waitpid(pid, &wstatus, 0) < 0)
			efaterror("waitpid");
#	else
		{
			pid_t w;
			do {
				if ((w = wait(&wstatus)) < 0)
					efaterror("wait");
			} while (w != pid);
		}
#	endif
#else
	static struct buf b;
	char const *p;

	/* Use system().  On many hosts system() discards signals.  Yuck!  */
	p = args + 1;
	bufscpy(&b, *p);
	while (*++p)
		bufargcat(&b, ' ', *p);
	if (infd != -1  &&  infd != STDIN_FILENO) {
		char redirection[32];
		VOID sprintf(redirection, "<&%d", infd);
		bufscat(&b, redirection);
	}
	if (outname)
		bufargcat(&b, '>', outname);
	wstatus = system(b.string);
#endif
#endif
    }
	if (!WIFEXITED(wstatus)) {
		if (WIFSIGNALED(wstatus)) {
			psignal(WTERMSIG(wstatus), args[1]);
			fatcleanup(1);
		}
		faterror("%s failed for unknown reason", args[1]);
	}
	return WEXITSTATUS(wstatus);
}

#define CARGSMAX 20
/*
* Run a command.
* infd, if not -1, is the input file descriptor.
* outname, if nonzero, is the name of the output file.
* The remaining arguments specify the command and its arguments.
*/
	int
#if has_prototypes
run(int infd, char const *outname, ...)
#else
	/*VARARGS2*/
run(infd, outname, va_alist)
	int infd;
	char const *outname;
	va_dcl
#endif
{
	va_list ap;
	char const *rgargs[CARGSMAX];
	register int i;
	vararg_start(ap, outname);
	for (i = 1;  (rgargs[i++] = va_arg(ap, char const*));  )
		if (CARGSMAX <= i)
			faterror("too many command arguments");
	va_end(ap);
	return runv(infd, outname, rgargs);
}


int RCSversion;

	void
setRCSversion(str)
	char const *str;
{
	static int oldversion;

	register char const *s = str + 2;

	if (*s) {
		int v = VERSION_DEFAULT;

		if (oldversion)
			redefined('V');
		oldversion = true;
		v = 0;
		while (isdigit(*s))
			v  =  10*v + *s++ - '0';
		if (*s)
			error("%s isn't a number", str);
		else if (v < VERSION_min  ||  VERSION_max < v)
			error("%s out of range %d..%d",
				str, VERSION_min, VERSION_max
			);

		RCSversion = VERSION(v);
	} else {
		printf("RCS version %s\n", RCS_version_string);
		exit(0);
	}
}

	int
getRCSINIT(argc, argv, newargv)
	int argc;
	char **argv, ***newargv;
{
	register char *p, *q, **pp;
	char const *ev;
	size_t n;

	if ((ev = cgetenv("RCSLOCALID")))
		setRCSLocalId(ev);

	if ((ev = cgetenv("RCSINCEXC")))
		setIncExc(ev);

	if (!(q = cgetenv("RCSINIT")))
		*newargv = argv;
	else {
		n = argc + 2;
		/*
		 * Count spaces in RCSINIT to allocate a new arg vector.
		 * This is an upper bound, but it's OK even if too large.
		 */
		for (p = q;  ;  ) {
			switch (*p++) {
			    default:
				continue;

			    case ' ':
			    case '\b': case '\f': case '\n':
			    case '\r': case '\t': case '\v':
				n++;
				continue;

			    case '\0':
				break;
			}
			break;
		}
		*newargv = pp = tnalloc(char*, n);
		*pp++ = *argv++; /* copy program name */
		for (p = q;  ;  ) {
			for (;;) {
				switch (*q) {
				    case '\0':
					goto copyrest;

				    case ' ':
				    case '\b': case '\f': case '\n':
				    case '\r': case '\t': case '\v':
					q++;
					continue;
				}
				break;
			}
			*pp++ = p;
			++argc;
			for (;;) {
				switch ((*p++ = *q++)) {
				    case '\0':
					goto copyrest;

				    case '\\':
					if (!*q)
						goto copyrest;
					p[-1] = *q++;
					continue;

				    default:
					continue;

				    case ' ':
				    case '\b': case '\f': case '\n':
				    case '\r': case '\t': case '\v':
					break;
				}
				break;
			}
			p[-1] = '\0';
		}
	    copyrest:
		while ((*pp++ = *argv++))
			continue;
	}
	return argc;
}


#define cacheid(E) static uid_t i; static int s; if (!s){ s=1; i=(E); } return i

#if has_getuid
	uid_t ruid() { cacheid(getuid()); }
#endif
#if has_setuid
	uid_t euid() { cacheid(geteuid()); }
#endif


#if has_setuid

/*
 * Setuid execution really works only with Posix 1003.1a Draft 5 seteuid(),
 * because it lets us switch back and forth between arbitrary users.
 * If seteuid() doesn't work, we fall back on setuid(),
 * which works if saved setuid is supported,
 * unless the real or effective user is root.
 * This area is such a mess that we always check switches at runtime.
 */

	static void
#if has_prototypes
set_uid_to(uid_t u)
#else
 set_uid_to(u) uid_t u;
#endif
/* Become user u.  */
{
	static int looping;

	if (euid() == ruid())
		return;
#if (has_fork||has_spawn) && DIFF_ABSOLUTE
#	if has_setreuid
		if (setreuid(u==euid() ? ruid() : euid(), u) != 0)
			efaterror("setuid");
#	else
		if (seteuid(u) != 0)
			efaterror("setuid");
#	endif
#endif
	if (geteuid() != u) {
		if (looping)
			return;
		looping = true;
		faterror("root setuid not supported" + (u?5:0));
	}
}

static int stick_with_euid;

	void
/* Ignore all calls to seteid() and setrid().  */
nosetid()
{
	stick_with_euid = true;
}

	void
seteid()
/* Become effective user.  */
{
	if (!stick_with_euid)
		set_uid_to(euid());
}

	void
setrid()
/* Become real user.  */
{
	if (!stick_with_euid)
		set_uid_to(ruid());
}
#endif

	time_t
now()
{
	static time_t t;
	if (!t  &&  time(&t) == -1)
		efaterror("time");
	return t;
}
