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
static char sccsid[] = "@(#)memalloc.c	8.3 (Berkeley) 5/4/95";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include "shell.h"
#include "output.h"
#include "memalloc.h"
#include "error.h"
#include "mystring.h"
#include "expand.h"
#include <stdlib.h>
#include <unistd.h>

/*
 * Like malloc, but returns an error when out of space.
 */

pointer
ckmalloc(size_t nbytes)
{
	pointer p;

	INTOFF;
	p = malloc(nbytes);
	INTON;
	if (p == NULL)
		error("Out of space");
	return p;
}


/*
 * Same for realloc.
 */

pointer
ckrealloc(pointer p, int nbytes)
{
	INTOFF;
	p = realloc(p, nbytes);
	INTON;
	if (p == NULL)
		error("Out of space");
	return p;
}

void
ckfree(pointer p)
{
	INTOFF;
	free(p);
	INTON;
}


/*
 * Make a copy of a string in safe storage.
 */

char *
savestr(const char *s)
{
	char *p;

	p = ckmalloc(strlen(s) + 1);
	scopy(s, p);
	return p;
}


/*
 * Parse trees for commands are allocated in lifo order, so we use a stack
 * to make this more efficient, and also to avoid all sorts of exception
 * handling code to handle interrupts in the middle of a parse.
 *
 * The size 496 was chosen because with 16-byte alignment the total size
 * for the allocated block is 512.
 */

#define MINSIZE 496		/* minimum size of a block. */


struct stack_block {
	struct stack_block *prev;
	/* Data follows */
};
#define SPACE(sp)	((char*)(sp) + ALIGN(sizeof(struct stack_block)))

STATIC struct stack_block *stackp;
STATIC struct stackmark *markp;
char *stacknxt;
int stacknleft;
int sstrnleft;
int herefd = -1;


static void
stnewblock(int nbytes)
{
	struct stack_block *sp;
	int allocsize;

	if (nbytes < MINSIZE)
		nbytes = MINSIZE;

	allocsize = ALIGN(sizeof(struct stack_block)) + ALIGN(nbytes);

	INTOFF;
	sp = ckmalloc(allocsize);
	sp->prev = stackp;
	stacknxt = SPACE(sp);
	stacknleft = allocsize - (stacknxt - (char*)sp);
	stackp = sp;
	INTON;
}


pointer
stalloc(int nbytes)
{
	char *p;

	nbytes = ALIGN(nbytes);
	if (nbytes > stacknleft)
		stnewblock(nbytes);
	p = stacknxt;
	stacknxt += nbytes;
	stacknleft -= nbytes;
	return p;
}


void
stunalloc(pointer p)
{
	if (p == NULL) {		/*DEBUG */
		write(STDERR_FILENO, "stunalloc\n", 10);
		abort();
	}
	stacknleft += stacknxt - (char *)p;
	stacknxt = p;
}



void
setstackmark(struct stackmark *mark)
{
	mark->stackp = stackp;
	mark->stacknxt = stacknxt;
	mark->stacknleft = stacknleft;
	mark->marknext = markp;
	markp = mark;
}


void
popstackmark(struct stackmark *mark)
{
	struct stack_block *sp;

	INTOFF;
	markp = mark->marknext;
	while (stackp != mark->stackp) {
		sp = stackp;
		stackp = sp->prev;
		ckfree(sp);
	}
	stacknxt = mark->stacknxt;
	stacknleft = mark->stacknleft;
	INTON;
}


/*
 * When the parser reads in a string, it wants to stick the string on the
 * stack and only adjust the stack pointer when it knows how big the
 * string is.  Stackblock (defined in stack.h) returns a pointer to a block
 * of space on top of the stack and stackblocklen returns the length of
 * this block.  Growstackblock will grow this space by at least one byte,
 * possibly moving it (like realloc).  Grabstackblock actually allocates the
 * part of the block that has been used.
 */

void
growstackblock(void)
{
	char *p;
	int newlen;
	char *oldspace;
	int oldlen;
	struct stack_block *sp;
	struct stack_block *oldstackp;
	struct stackmark *xmark;

	newlen = (stacknleft == 0) ? MINSIZE : stacknleft * 2 + 100;
	newlen = ALIGN(newlen);
	oldspace = stacknxt;
	oldlen = stacknleft;

	if (stackp != NULL && stacknxt == SPACE(stackp)) {
		INTOFF;
		oldstackp = stackp;
		stackp = oldstackp->prev;
		sp = ckrealloc((pointer)oldstackp, newlen);
		sp->prev = stackp;
		stackp = sp;
		stacknxt = SPACE(sp);
		stacknleft = newlen - (stacknxt - (char*)sp);

		/*
		 * Stack marks pointing to the start of the old block
		 * must be relocated to point to the new block
		 */
		xmark = markp;
		while (xmark != NULL && xmark->stackp == oldstackp) {
			xmark->stackp = stackp;
			xmark->stacknxt = stacknxt;
			xmark->stacknleft = stacknleft;
			xmark = xmark->marknext;
		}
		INTON;
	} else {
		p = stalloc(newlen);
		if (oldlen != 0)
			memcpy(p, oldspace, oldlen);
		stunalloc(p);
	}
}



void
grabstackblock(int len)
{
	len = ALIGN(len);
	stacknxt += len;
	stacknleft -= len;
}



/*
 * The following routines are somewhat easier to use that the above.
 * The user declares a variable of type STACKSTR, which may be declared
 * to be a register.  The macro STARTSTACKSTR initializes things.  Then
 * the user uses the macro STPUTC to add characters to the string.  In
 * effect, STPUTC(c, p) is the same as *p++ = c except that the stack is
 * grown as necessary.  When the user is done, she can just leave the
 * string there and refer to it using stackblock().  Or she can allocate
 * the space for it using grabstackstr().  If it is necessary to allow
 * someone else to use the stack temporarily and then continue to grow
 * the string, the user should use grabstack to allocate the space, and
 * then call ungrabstr(p) to return to the previous mode of operation.
 *
 * USTPUTC is like STPUTC except that it doesn't check for overflow.
 * CHECKSTACKSPACE can be called before USTPUTC to ensure that there
 * is space for at least one character.
 */


char *
growstackstr(void)
{
	int len;

	len = stackblocksize();
	if (herefd >= 0 && len >= 1024) {
		xwrite(herefd, stackblock(), len);
		sstrnleft = len - 1;
		return stackblock();
	}
	growstackblock();
	sstrnleft = stackblocksize() - len - 1;
	return stackblock() + len;
}


/*
 * Called from CHECKSTRSPACE.
 */

char *
makestrspace(void)
{
	int len;

	len = stackblocksize() - sstrnleft;
	growstackblock();
	sstrnleft = stackblocksize() - len;
	return stackblock() + len;
}



void
ungrabstackstr(char *s, char *p)
{
	stacknleft += stacknxt - s;
	stacknxt = s;
	sstrnleft = stacknleft - (p - s);
}
