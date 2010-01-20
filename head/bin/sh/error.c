/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)error.c	8.2 (Berkeley) 5/4/95";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Errors and exceptions.
 */

#include "shell.h"
#include "main.h"
#include "options.h"
#include "output.h"
#include "error.h"
#include "nodes.h" /* show.h needs nodes.h */
#include "show.h"
#include "trap.h"
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>


/*
 * Code to handle exceptions in C.
 */

struct jmploc *handler;
volatile sig_atomic_t exception;
volatile sig_atomic_t suppressint;
volatile sig_atomic_t intpending;
char *commandname;


static void exverror(int, const char *, va_list) __printf0like(2, 0) __dead2;

/*
 * Called to raise an exception.  Since C doesn't include exceptions, we
 * just do a longjmp to the exception handler.  The type of exception is
 * stored in the global variable "exception".
 *
 * Interrupts are disabled; they should be reenabled when the exception is
 * caught.
 */

void
exraise(int e)
{
	INTOFF;
	if (handler == NULL)
		abort();
	exception = e;
	longjmp(handler->loc, 1);
}


/*
 * Called from trap.c when a SIGINT is received.  (If the user specifies
 * that SIGINT is to be trapped or ignored using the trap builtin, then
 * this routine is not called.)  Suppressint is nonzero when interrupts
 * are held using the INTOFF macro.  If SIGINTs are not suppressed and
 * the shell is not a root shell, then we want to be terminated if we
 * get here, as if we were terminated directly by a SIGINT.  Arrange for
 * this here.
 */

void
onint(void)
{
	sigset_t sigset;

	/*
	 * The !in_dotrap here is safe.  The only way we can arrive here
	 * with in_dotrap set is that a trap handler set SIGINT to SIG_DFL
	 * and killed itself.
	 */

	if (suppressint && !in_dotrap) {
		intpending++;
		return;
	}
	intpending = 0;
	sigemptyset(&sigset);
	sigprocmask(SIG_SETMASK, &sigset, NULL);

	/*
	 * This doesn't seem to be needed, since main() emits a newline.
	 */
#if 0
	if (tcgetpgrp(0) == getpid())
		write(STDERR_FILENO, "\n", 1);
#endif
	if (rootshell && iflag)
		exraise(EXINT);
	else {
		signal(SIGINT, SIG_DFL);
		kill(getpid(), SIGINT);
	}
}


/*
 * Exverror is called to raise the error exception.  If the first argument
 * is not NULL then error prints an error message using printf style
 * formatting.  It then raises the error exception.
 */
static void
exverror(int cond, const char *msg, va_list ap)
{
	/*
	 * An interrupt trumps an error.  Certain places catch error
	 * exceptions or transform them to a plain nonzero exit code
	 * in child processes, and if an error exception can be handled,
	 * an interrupt can be handled as well.
	 *
	 * exraise() will disable interrupts for the exception handler.
	 */
	FORCEINTON;

#ifdef DEBUG
	if (msg)
		TRACE(("exverror(%d, \"%s\") pid=%d\n", cond, msg, getpid()));
	else
		TRACE(("exverror(%d, NULL) pid=%d\n", cond, getpid()));
#endif
	if (msg) {
		if (commandname)
			outfmt(out2, "%s: ", commandname);
		doformat(out2, msg, ap);
		out2c('\n');
	}
	flushall();
	exraise(cond);
}


void
error(const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	exverror(EXERROR, msg, ap);
	va_end(ap);
}


void
exerror(int cond, const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	exverror(cond, msg, ap);
	va_end(ap);
}
