/*
 * Copyright (c) 1993 Paul Kranenburg
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: crt0.c,v 1.18 1995/09/27 23:13:33 nate Exp $
 */

#include <sys/param.h>
#ifdef UGLY_LOCALE_HACK
#include <locale.h>
#endif
#include <stdlib.h>

#ifdef DYNAMIC
#include <sys/types.h>
#include <sys/syscall.h>
#include <a.out.h>
#include <string.h>
#include <sys/mman.h>
#include <link.h>

/* !!!
 * This is gross, ld.so is a ZMAGIC a.out, but has `sizeof(hdr)' for
 * an entry point and not at PAGSIZ as the N_*ADDR macros assume.
 */
#undef N_DATADDR
#define N_DATADDR(x)	((x).a_text)

#undef N_BSSADDR
#define N_BSSADDR(x)	((x).a_text + (x).a_data)

#ifndef N_GETMAGIC
#define N_GETMAGIC(x)	((x).a_magic)
#endif /* N_GETMAGIC */

#ifndef	MAP_PRIVATE
#define MAP_PRIVATE	MAP_COPY
#endif /* MAP_PRIVATE */

#ifndef MAP_FILE
#define MAP_FILE	0
#endif /* MAP_FILE */

#ifndef MAP_ANON
#define MAP_ANON	0
#endif /* MAP_ANON */

#ifdef DEBUG 
/*
 * We need these two because we are going to call them before the ld.so is
 * finished (as a matter of fact before we know if it exists !) so we must
 * provide these versions for them
 */
static char		*_getenv();
static int		_strncmp();
#endif /* DEBUG */

#ifndef LDSO
#define LDSO		"/usr/libexec/ld.so"
#endif /* LDSO */

extern struct _dynamic	_DYNAMIC;
static struct ld_entry	*ld_entry;
static void		__do_dynamic_link ();
#endif /* DYNAMIC */

#ifdef UGLY_LOCALE_HACK
extern void		_startup_setlocale __P((int, const char *));
#endif
int			_callmain();
int			errno;
static char		empty[1];
char			*__progname = empty;
char			**environ;

extern	unsigned char	etext;
extern	unsigned char	eprol asm ("eprol");
extern			start() asm("start");
extern			mcount() asm ("mcount");
extern	int		main(int argc, char **argv, char **envp);
int			__syscall(int syscall,...);
#ifdef MCRT0
void			monstartup(void *low, void *high);
#endif /* MCRT0 */


/*
 * We need these system calls, but can't use library stubs because the are
 * not accessible until we have done the ld.so stunt.
 */

#define _exit(v) \
	__syscall(SYS_exit, (int)(v))
#define _open(name, f, m) \
	__syscall(SYS_open, (char *)(name), (int)(f), (int)(m))
#define _read(fd, s, n) \
	__syscall(SYS_read, (int)(fd), (void *)(s), (size_t)(n))
#define _write(fd, s, n) \
	__syscall(SYS_write, (int)(fd), (void *)(s), (size_t)(n))
#define _mmap(addr, len, prot, flags, fd, off)	\
	(caddr_t) __syscall(SYS_mmap, (caddr_t)(addr), (size_t)(len), \
		(int)(prot), (int)(flags), (int)(fd), (long)0L, (off_t)(off))

#define _PUTNMSG(str, len)	_write(2, (str), (len))
#define _PUTMSG(str)		_PUTNMSG((str), sizeof (str) - 1)
#define _FATAL(str)		( _PUTMSG(str), _exit(1) )


int
start()
{
	struct kframe {
		int	kargc;
		char	*kargv[1];	/* size depends on kargc */
		char	kargstr[1];	/* size varies */
		char	kenvstr[1];	/* size varies */
	};
	/*
	 *	ALL REGISTER VARIABLES!!!
	 */
	register struct kframe *kfp;
	register char **targv;
	register char **argv;
	extern void _mcleanup();
#ifdef DYNAMIC
	volatile caddr_t x;
#endif

#ifdef lint
	kfp = 0;
	initcode = initcode = 0;
#else /* not lint */
	/* just above the saved frame pointer */
	asm ("lea 4(%%ebp), %0" : "=r" (kfp) );
#endif /* not lint */
	for (argv = targv = &kfp->kargv[0]; *targv++; /* void */)
		/* void */ ;
	if (targv >= (char **)(*argv))
		--targv;
	environ = targv;

	if (argv[0]) {
		register char *s;
		__progname = argv[0];
		for (s=__progname; *s != '\0'; s++)
			if (*s == '/')
				__progname = s+1;
	}

#ifdef DYNAMIC
	/* ld(1) convention: if DYNAMIC = 0 then statically linked */
	/* sometimes GCC is too smart/stupid for its own good */
	x = (caddr_t)&_DYNAMIC;
	if (x)
		__do_dynamic_link();
#endif /* DYNAMIC */

asm("eprol:");

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup(&eprol, &etext);
#endif /* MCRT0 */

#ifdef UGLY_LOCALE_HACK
	if (getenv("ENABLE_STARTUP_LOCALE") != NULL)
		_startup_setlocale(LC_ALL, "");
#endif /* UGLY_LOCALE_HACK */

asm ("__callmain:");		/* Defined for the benefit of debuggers */
	exit(main(kfp->kargc, argv, environ));
}

#ifdef DYNAMIC
static void
__do_dynamic_link ()
{
	struct crt_ldso	crt;
	struct exec	hdr;
	char		*ldso;
	int		(*entry)();
	int		ret;

#ifdef DEBUG
	/* Provision for alternate ld.so - security risk! */
	if (!(ldso = _getenv("LDSO")))
#endif
		ldso = LDSO;

	crt.crt_ldfd = _open(ldso, 0, 0);
	if (crt.crt_ldfd == -1) {
		_PUTMSG("Couldn't open ");
		_PUTMSG(LDSO);
		_FATAL(".\n");
	}

	/* Read LDSO exec header */
	if (_read(crt.crt_ldfd, &hdr, sizeof hdr) < sizeof hdr) {
		_FATAL("Failure reading ld.so\n");
	}
	if ((N_GETMAGIC_NET(hdr) != ZMAGIC) && (N_GETMAGIC(hdr) != QMAGIC)) {
		_FATAL("Bad magic: ld.so\n");
	}

	/* We use MAP_ANON */
	crt.crt_dzfd = -1;

	/* Map in ld.so */
	crt.crt_ba = (int)_mmap(0, hdr.a_text,
			PROT_READ|PROT_EXEC,
			MAP_FILE|MAP_PRIVATE,
			crt.crt_ldfd, N_TXTOFF(hdr));
	if (crt.crt_ba == -1) {
		_FATAL("Cannot map ld.so (text)\n");
	}

	/* Map in data segment of ld.so writable */
	if ((int)_mmap((caddr_t)(crt.crt_ba+N_DATADDR(hdr)), hdr.a_data,
			PROT_READ|PROT_WRITE,
			MAP_FIXED|MAP_FILE|MAP_PRIVATE,
			crt.crt_ldfd, N_DATOFF(hdr)) == -1) {
		_FATAL("Cannot map ld.so (data)\n");
	}

	/* Map bss segment of ld.so zero */
	if (hdr.a_bss && (int)_mmap((caddr_t)(crt.crt_ba+N_BSSADDR(hdr)),
			hdr.a_bss,
			PROT_READ|PROT_WRITE,
			MAP_FIXED|MAP_ANON|MAP_PRIVATE,
			crt.crt_dzfd, 0) == -1) {
		_FATAL("Cannot map ld.so (bss)\n");
	}

	crt.crt_dp = &_DYNAMIC;
	crt.crt_ep = environ;
	crt.crt_bp = (caddr_t)_callmain;
	crt.crt_prog = __progname;

	entry = (int (*)())(crt.crt_ba + sizeof hdr);
	ret = (*entry)(CRT_VERSION_BSD_3, &crt);
	if (ret == -1) {
		_PUTMSG("ld.so failed");
		if(_DYNAMIC.d_entry != NULL) {
			char *msg = (_DYNAMIC.d_entry->dlerror)();
			if(msg != NULL) {
				char *endp;
				_PUTMSG(": ");
				for(endp = msg;  *endp != '\0';  ++endp)
					;	/* Find the end */
				_PUTNMSG(msg, endp - msg);
			}
		}
		_FATAL("\n");
	}

	ld_entry = _DYNAMIC.d_entry;

	if (ret >= LDSO_VERSION_HAS_DLEXIT)
		atexit(ld_entry->dlexit);

	return;
}

/*
 * DL stubs
 */

void *
dlopen(name, mode)
char	*name;
int	mode;
{
	if (ld_entry == NULL)
		return NULL;

	return (ld_entry->dlopen)(name, mode);
}

int
dlclose(fd)
void	*fd;
{
	if (ld_entry == NULL)
		return -1;

	return (ld_entry->dlclose)(fd);
}

void *
dlsym(fd, name)
void	*fd;
char	*name;
{
	if (ld_entry == NULL)
		return NULL;

	return (ld_entry->dlsym)(fd, name);
}


char *
dlerror()
{
	if (ld_entry == NULL)
		return "Service unavailable";

	return (ld_entry->dlerror)();
}


/*
 * Support routines
 */

#ifdef DEBUG
static int
_strncmp(s1, s2, n)
	register char *s1, *s2;
	register n;
{

	if (n == 0)
		return (0);
	do {
		if (*s1 != *s2++)
			return (*(unsigned char *)s1 - *(unsigned char *)--s2);
		if (*s1++ == 0)
			break;
	} while (--n != 0);
	return (0);
}

static char *
_getenv(name)
	register char *name;
{
	extern char **environ;
	register int len;
	register char **P, *C;

	for (C = name, len = 0; *C && *C != '='; ++C, ++len);
	for (P = environ; *P; ++P)
		if (!_strncmp(*P, name, len))
			if (*(C = *P + len) == '=') {
				return(++C);
			}
	return (char *)0;
}

#endif /* DEBUG */

	asm("	___syscall:");
	asm("		popl %ecx");
	asm("		popl %eax");
	asm("		pushl %ecx");
	asm("		.byte 0x9a");
	asm("		.long 0");
	asm("		.word 7");
	asm("		pushl %ecx");
	asm("		jc 1f");
	asm("		ret");
	asm("	1:");
	asm("		movl	$-1,%eax");
	asm("		ret");

#endif /* DYNAMIC */

#ifdef MCRT0
asm ("	.text");
asm ("_eprol:");
#endif
