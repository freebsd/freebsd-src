/*
 *                     RCS utilities
 */

/* Copyright (C) 1982, 1988, 1989 Walter Tichy
   Copyright 1990, 1991 by Paul Eggert
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
along with RCS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

Report problems and direct all questions to:

    rcs-bugs@cs.purdue.edu

*/




/* $Log: rcsutil.c,v $
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

libId(utilId, "$Id: rcsutil.c,v 5.10 1991/10/07 17:32:46 eggert Exp $")

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

#if lint
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
	alloced = nil;
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
			    ||
				!(name = cgetenv("LOGNAME"))
			    &&  !(name = cgetenv("USER")))
			&&  !(name = getlogin())
#		    else
			suspicious
			||
				!(name = cgetenv("LOGNAME"))
			    &&  !(name = cgetenv("USER"))
			    &&  !(name = getlogin())
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
			faterror("Who are you?  Please set LOGNAME.");
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

	static signal_type
catchsig(s)
	int s;
{
	char const *sname;
	char buf[BUFSIZ];

#if sig_zaps_handler
	/* If a signal arrives before we reset the signal handler, we lose. */
	VOID signal(s, SIG_IGN);
#endif
	if (holdlevel) {
		heldsignal = s;
		return;
	}
	ignoreints();
	setrid();
	if (!quietflag) {
	    sname = nil;
#if has_sys_siglist && defined(NSIG)
	    if ((unsigned)s < NSIG) {
#		ifndef sys_siglist
		    extern char const *sys_siglist[];
#		endif
		sname = sys_siglist[s];
	    }
#else
	    switch (s) {
#ifdef SIGHUP
		case SIGHUP:	sname = "Hangup";  break;
#endif
#ifdef SIGINT
		case SIGINT:	sname = "Interrupt";  break;
#endif
#ifdef SIGPIPE
		case SIGPIPE:	sname = "Broken pipe";  break;
#endif
#ifdef SIGQUIT
		case SIGQUIT:	sname = "Quit";  break;
#endif
#ifdef SIGTERM
		case SIGTERM:	sname = "Terminated";  break;
#endif
#ifdef SIGXCPU
		case SIGXCPU:	sname = "Cputime limit exceeded";  break;
#endif
#ifdef SIGXFSZ
		case SIGXFSZ:	sname = "Filesize limit exceeded";  break;
#endif
	    }
#endif
	    if (sname)
		VOID sprintf(buf, "\nRCS: %s.  Cleaning up.\n", sname);
	    else
		VOID sprintf(buf, "\nRCS: Signal %d.  Cleaning up.\n", s);
	    VOID write(STDERR_FILENO, buf, strlen(buf));
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
		VOID catchsig(heldsignal);
}


static int const sig[] = {
#ifdef SIGHUP
	SIGHUP,
#endif
#ifdef SIGINT
	SIGINT,
#endif
#ifdef SIGPIPE
	SIGPIPE,
#endif
#ifdef SIGQUIT
	SIGQUIT,
#endif
#ifdef SIGTERM
	SIGTERM,
#endif
#ifdef SIGXCPU
	SIGXCPU,
#endif
#ifdef SIGXFSZ
	SIGXFSZ,
#endif
};
#define SIGS (sizeof(sig)/sizeof(*sig))


#if has_sigaction

	static void
  check_sig(r)
	int r;
  {
	if (r != 0)
		efaterror("signal");
  }

	static void
  setup_catchsig()
  {
	register int i;
	sigset_t blocked;
	struct sigaction act;

	check_sig(sigemptyset(&blocked));
	for (i=SIGS; 0<=--i; )
	    check_sig(sigaddset(&blocked, sig[i]));
	for (i=SIGS; 0<=--i; ) {
	    check_sig(sigaction(sig[i], (struct sigaction*)nil, &act));
	    if (act.sa_handler != SIG_IGN) {
		    act.sa_handler = catchsig;
		    act.sa_mask = blocked;
		    check_sig(sigaction(sig[i], &act, (struct sigaction*)nil));
	    }
	}
  }

#else
#if has_sigblock

	static void
  setup_catchsig()
  {
	register int i;
	int mask;

	mask = 0;
	for (i=SIGS; 0<=--i; )
		mask |= sigmask(sig[i]);
	mask = sigblock(mask);
	for (i=SIGS; 0<=--i; )
		if (
		    signal(sig[i], catchsig) == SIG_IGN  &&
		    signal(sig[i], SIG_IGN) != catchsig
		)
			faterror("signal catcher failure");
	VOID sigsetmask(mask);
  }

#else

	static void
  setup_catchsig()
  {
	register i;

	for (i=SIGS; 0<=--i; )
		if (
		    signal(sig[i], SIG_IGN) != SIG_IGN  &&
		    signal(sig[i], catchsig) != SIG_IGN
		)
			faterror("signal catcher failure");
  }

#endif
#endif

	void
catchints()
{
	static int catching_ints;
	if (!catching_ints) {
		catching_ints = true;
		setup_catchsig();
	}
}

#endif /* has_signal */


	void
fastcopy(inf,outf)
	register RILE *inf;
	FILE *outf;
/* Function: copies the remainder of file inf to outf.
 */
{
#if large_memory
#	if has_mmap
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

#if !has_spawn
	static void
tryopen(fd,file,flags)
	int fd, flags;
	char const *file;
{
	if (file  &&  fdreopen(fd,file,flags) != fd)
		efaterror(file);
}
#else
	static int
tryopen(fd,file,flags)
	int fd, flags;
	char const *file;
{
	int newfd = -1;
	if (file  &&  ((newfd=dup(fd)) < 0  ||  fdreopen(fd,file,flags) != fd))
		efaterror(file);
	return newfd;
}
	static void
redirect(old, new)
	int old, new;
{
	if (0 <= old   &&   (close(new) != 0  ||  movefd(old,new) < 0))
		efaterror("spawn I/O redirection");
}
#endif



#if !has_fork && !has_spawn
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

/*
* Run a command specified by the strings in 'inoutargs'.
* inoutargs[0], if nonnil, is the name of the input file.
* inoutargs[1], if nonnil, is the name of the output file.
* inoutargs[2..] form the command to be run.
*/
	int
runv(inoutargs)
	char const **inoutargs;
{
	register char const **p;
	int wstatus;

	oflush();
	eflush();
    {
#if has_spawn
	int in, out;
	p = inoutargs;
	in = tryopen(STDIN_FILENO, *p++, O_BINARY|O_RDONLY);
	out = tryopen(STDOUT_FILENO, *p++, O_BINARY|O_CREAT|O_TRUNC|O_WRONLY);
	wstatus = spawn_RCS(0, *p, (char*const*)p);
	if (wstatus == -1  &&  errno == ENOEXEC) {
		*--p = RCS_SHELL;
		wstatus = spawnv(0, *p, (char*const*)p);
	}
	redirect(in, STDIN_FILENO);
	redirect(out, STDOUT_FILENO);
#else
#if has_fork
	pid_t pid;
#	if !has_waitpid
		pid_t w;
#	endif
	if (!(pid = vfork())) {
		p = inoutargs;
		tryopen(STDIN_FILENO, *p++, O_BINARY|O_RDONLY);
		tryopen(STDOUT_FILENO, *p++, O_BINARY|O_CREAT|O_TRUNC|O_WRONLY);
		VOID exec_RCS(*p, (char*const*)p);
		if (errno == ENOEXEC) {
			*--p = RCS_SHELL;
			VOID execv(*p, (char*const*)p);
		}
		VOID write(STDERR_FILENO, *p, strlen(*p));
		VOID write(STDERR_FILENO, ": not found\n", 12);
		_exit(EXIT_TROUBLE);
	}
	if (pid < 0)
		efaterror("fork");
#	if has_waitpid
		if (waitpid(pid, &wstatus, 0) < 0)
			efaterror("waitpid");
#	else
		do {
			if ((w = wait(&wstatus)) < 0)
				efaterror("wait");
		} while (w != pid);
#	endif
#else
	static struct buf b;

	/* Use system().  On many hosts system() discards signals.  Yuck!  */
	p = inoutargs+2;
	bufscpy(&b, *p);
	while (*++p)
		bufargcat(&b, ' ', *p);
	if (inoutargs[0])
		bufargcat(&b, '<', inoutargs[0]);
	if (inoutargs[1])
		bufargcat(&b, '>', inoutargs[1]);
	wstatus = system(b.string);
#endif
#endif
    }
	if (!WIFEXITED(wstatus))
		faterror("%s failed", inoutargs[2]);
	return WEXITSTATUS(wstatus);
}

#define CARGSMAX 20
/*
* Run a command.
* The first two arguments are the input and output files (if nonnil);
* the rest specify the command and its arguments.
*/
	int
#if has_prototypes
run(char const *infile, char const *outfile, ...)
#else
	/*VARARGS2*/
run(infile, outfile, va_alist)
	char const *infile;
	char const *outfile;
	va_dcl
#endif
{
	va_list ap;
	char const *rgargs[CARGSMAX];
	register i = 0;
	rgargs[0] = infile;
	rgargs[1] = outfile;
	vararg_start(ap, outfile);
	for (i = 2;  (rgargs[i++] = va_arg(ap, char const*));  )
		if (CARGSMAX <= i)
			faterror("too many command arguments");
	va_end(ap);
	return runv(rgargs);
}


	char const *
date2str(date, datebuf)
	char const date[datesize];
	char datebuf[datesize];
/*
* Format a user-readable form of the RCS format DATE into the buffer DATEBUF.
* Yield DATEBUF.
*/
{
	register char const *p = date;

	while (*p++ != '.')
		;
	VOID sprintf(datebuf,
		"19%.*s/%.2s/%.2s %.2s:%.2s:%s" +
			(date[2]=='.' && VERSION(5)<=RCSversion  ?  0  :  2),
		(int)(p-date-1), date,
		p, p+3, p+6, p+9, p+12
	);
	return datebuf;
}


int RCSversion;

	void
setRCSversion(str)
	char const *str;
{
	static int oldversion;

	register char const *s = str + 2;
	int v = VERSION_DEFAULT;

	if (oldversion)
		redefined('V');
	oldversion = true;

	if (*s) {
		v = 0;
		while (isdigit(*s))
			v  =  10*v + *s++ - '0';
		if (*s)
			faterror("%s isn't a number", str);
		if (v < VERSION_min  ||  VERSION_max < v)
			faterror("%s out of range %d..%d", str, VERSION_min, VERSION_max);
	}

	RCSversion = VERSION(v);
}

	int
getRCSINIT(argc, argv, newargv)
	int argc;
	char **argv, ***newargv;
{
	register char *p, *q, **pp;
	unsigned n;

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
			;
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
set_uid_to(u)
	uid_t u;
/* Become user u.  */
{
	static int looping;

	if (euid() == ruid())
		return;
#if (has_fork||has_spawn) && DIFF_ABSOLUTE
	if (seteuid(u) != 0)
		efaterror("setuid");
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
