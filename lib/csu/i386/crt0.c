/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)crt0.c	5.7 (Berkeley) 7/3/91";
#endif /* LIBC_SCCS and not lint */


/*
 *	C start up routine.
 *	Robert Henry, UCB, 20 Oct 81
 *
 *	We make the following (true) assumption:
 *	1) The only register variable that we can trust is the frame pointer,
 *	ebp, which points to the base of the kernel calling frame.
 */

char	**environ = (char **)0;
int	errno = 0;

asm(".text");
asm(".long 0xc000c000");
#if 1 /* more needed for alignment on i486/gcc-2.3.3 */
asm(".long 0xc000c000");
asm(".long 0xc000c000");
asm(".long 0xc000c000");
#endif

extern	unsigned char	etext;
extern	unsigned char	eprol asm ("eprol");
extern			start() asm("start");
extern			mcount() asm ("mcount");

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

#ifdef lint
	kfp = 0;
	initcode = initcode = 0;
#else not lint
	/* just above the saved frame pointer */
	asm ("lea 4(%%ebp), %0" : "=r" (kfp) );
#endif not lint
	for (argv = targv = &kfp->kargv[0]; *targv++; /* void */)
		/* void */ ;
	if (targv >= (char **)(*argv))
		--targv;
	environ = targv;
asm("eprol:");

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup(&eprol, &etext);
#endif MCRT0
	exit(main(kfp->kargc, argv, environ));
}

#ifdef CRT0
/*
 * null mcount and moncontrol,
 * just in case some routine is compiled for profiling
 */
moncontrol(val)
	int val;
{

}

mcount() { }
#endif CRT0
