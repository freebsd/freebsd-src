/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
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
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libc_r/uthread/uthread_autoinit.cc,v 1.3 2000/01/06 12:16:16 deischen Exp $
 */

/*
 * This module uses the magic of C++ static constructors to initialize the
 * threads package at program start-up time.
 *
 * Note: Because of a bug in certain versions of "/usr/lib/c++rt0.o", you
 * should _not_ enclose the body of this module in an "#ifdef _THREAD_SAFE"
 * conditional.
 */

extern "C" void _thread_init(void);

/*
 * First, we declare a class with a constructor.
 */
class _thread_init_invoker {
public:
	_thread_init_invoker();	/* Constructor declaration. */
};

/*
 * Here is the definition of the constructor.  All it does is call the
 * threads initialization function, "_thread_init".
 */
_thread_init_invoker::_thread_init_invoker()
{
	_thread_init();
}

/*
 * Here is a single, static instance of our "_thread_init_invoker" class.
 * The mere existance of this instance will result in its constructor
 * being called, automatically, at program start-up time.
 */
static _thread_init_invoker the_thread_init_invoker;

/*
 * For the shared version of the threads library, the above is sufficient.
 * But for the archive version of the library, we need a little bit more.
 * Namely, we must arrange for this particular module to be pulled in from
 * the archive library at link time.  To accomplish that, we define and
 * initialize a variable, "_thread_autoinit_dummy_decl".  This variable is
 * referenced (as an extern) from libc/stdlib/exit.c. This will always
 * create a need for this module, ensuring that it is present in the
 * executable.
 *
 * We know that, if the user does _anything_ at all with threads, then the
 * "uthread_init.c" module will be linked in.  That is the case because
 * "uthread_init.c" is the module that defines all of the global variables
 * used by the threads library.  The presence of "uthread_init.c" will, in
 * turn, force this module to be linked in.  And the presence of this module
 * in the executable will result in the constructor being invoked, and
 * "_thread_init" being called.
 */
extern "C" int _thread_autoinit_dummy_decl;	/* Declare with "C" linkage */
int _thread_autoinit_dummy_decl = 0;
