/* 05 Jun 93*/
/*
 * mycall.c
 *
 * 05 Jun 93	Terry Lambert		Split out of newsyscall.c
 *
 * Copyright (c) 1993 Terrence R. Lambert.
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
 *      This product includes software developed by Terrence R. Lambert.
 * 4. The name Terrence R. Lambert may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TERRENCE R. LAMBERT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE TERRENCE R. LAMBERT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/param.h>
#include <sys/systm.h>

/* XXX this should be in a header. */
extern int	mycall __P((struct proc *p, void *uap, int retval[]));

/*
 * This is the actual code for the system call... it can't be static because
 * it is exported to another part of the module... the only place it needs
 * to be referenced is the sysent we are interested in.
 *
 * To write your own system call using this as a template, you could strip
 * out this code and use the rest as a prototype module, changing only the
 * function names and the number of arguments to the call in the module
 * specific "sysent".
 *
 * You would have to use the "-R" option of "ld" to ensure a linkable file
 * if you were to do this, since you would need to combine multiple ".o"
 * files into a single ".o" file for use by "modload".
 */
int
mycall( p, uap, retval)
struct proc	*p;
void		*uap;
int		retval[];
{
	/*
	 * Our new system call simply prints a message; it takes no
	 * arguments.
	 */

	printf( "\nI am a loaded system call.\n");
	printf( "I was loaded using the syscall module loader interface.\n");
	printf( "I don't do anything except call the kernel's printf().\n");
	printf( "I will print this message each time I am called!\n");

	return( 0);	/* success (or error code from errno.h)*/
}
