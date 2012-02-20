/*$Header: /p/tcsh/cvsroot/tcsh/win32/nt.const.c,v 1.4 2003/02/08 20:03:29 christos Exp $*/
/*
 * nt.const.c: NT-specific String constants for tcsh.
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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
#define _h_tc_const
#include "sh.h"
#ifdef WINNT_NATIVE
Char STRoldtitle[]	= { 'o', 'l', 'd', 't', 'i', 't', 'l', 'e', '\0' };
Char STRNTlamepathfix[] = { 'N', 'T', 'l', 'a', 'm', 'e', 'p', 'a', 't', 'h',
			    'f', 'i','x','\0'};
Char STRtcshlang[]	= { 'T', 'C', 'S', 'H', 'L', 'A', 'N', 'G', '\0' };
Char STRtcshsubsthb[]	= { 'T', 'C', 'S', 'H', 'S', 'U', 'B', 'S', 'T',
			    'H', 'B', '\0' };
Char STRtcshonlystartexes[] = { 'T', 'C', 'S', 'H', 'O', 'N', 'L', 'Y', 'S',
				'T', 'A', 'R', 'T', 'E', 'X', 'E', 'S', '\0' };
Char STRNTslowexec[] = {'N','T','s','l','o','w','e','x','e','c','\0'};

Char STRNTcaseifypwd[]={'N','T','c','a','s','e','i','f','y','p','w','d','\0'};
Char STRdotEXE[] = {'.','E','X','E','\0'};

Char STRNTnoquoteprotect[]={'N','T','n','o','q','u','o','t','e','p','r','o','t','e','c','t','\0'};
#endif /* WINNT_NATIVE */
