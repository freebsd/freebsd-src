/*
 * Copyright (c) 1989 The Regents of the University of California.
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
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00027
 * --------------------         -----   ----------------------
 *
 * 02 Aug 92	Wiljo Heinen		Fixed toupper()/tolower() range check
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)isctype.c	5.2 (Berkeley) 6/1/90";
#endif /* LIBC_SCCS and not lint */

#define _ANSI_LIBRARY
#include <ctype.h>

#undef isalnum
isalnum(c)
	int c;
{
	return((_ctype_ + 1)[c] & (_U|_L|_N));
}

#undef isalpha
isalpha(c)
	int c;
{
	return((_ctype_ + 1)[c] & (_U|_L));
}

#undef iscntrl
iscntrl(c)
	int c;
{
	return((_ctype_ + 1)[c] & _C);
}

#undef isdigit
isdigit(c)
	int c;
{
	return((_ctype_ + 1)[c] & _N);
}

#undef isgraph
isgraph(c)
	int c;
{
	return((_ctype_ + 1)[c] & (_P|_U|_L|_N));
}

#undef islower
islower(c)
	int c;
{
	return((_ctype_ + 1)[c] & _L);
}

#undef isprint
isprint(c)
	int c;
{
	return((_ctype_ + 1)[c] & (_P|_U|_L|_N|_B));
}

#undef ispunct
ispunct(c)
	int c;
{
	return((_ctype_ + 1)[c] & _P);
}

#undef isspace
isspace(c)
	int c;
{
	return((_ctype_ + 1)[c] & _S);
}

#undef isupper
isupper(c)
	int c;
{
	return((_ctype_ + 1)[c] & _U);
}

#undef isxdigit
isxdigit(c)
	int c;
{
	return((_ctype_ + 1)[c] & (_N|_X));
}

#undef tolower
tolower(c)
	int c;
{
/* was:	return((c) - 'A' + 'a');*/
	return ( isupper(c) ? c - 'A' + 'a' : c);
}

#undef toupper
toupper(c)
	int c;
{
/* was:	return((c) - 'a' + 'A');*/
	return ( islower(c) ? c - 'a' + 'A' : c);
}
