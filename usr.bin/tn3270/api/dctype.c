/*-
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#ifndef lint
static char sccsid[] = "@(#)dctype.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include "dctype.h"

unsigned char dctype[192] = {
/*00*/
	D_SPACE,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
/*10*/
	D_SPACE,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	0,
	0,
	0,
	0,
/*20*/
	D_DIGIT|D_PRINT,
	D_DIGIT|D_PRINT,
	D_DIGIT|D_PRINT,
	D_DIGIT|D_PRINT,
	D_DIGIT|D_PRINT,
	D_DIGIT|D_PRINT,
	D_DIGIT|D_PRINT,
	D_DIGIT|D_PRINT,
	D_DIGIT|D_PRINT,
	D_DIGIT|D_PRINT,
	0,
	0,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
/*30*/
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
	0,
	0,
	0,
	0,
	D_PUNCT|D_PRINT,
	0,
	D_PUNCT|D_PRINT,
	0,
	0,
/*40*/
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
/*50*/
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
/*60*/
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
/*70*/
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
/*80*/
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
/*90*/
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	D_LOWER|D_PRINT,
	0,
	0,
	0,
	0,
	0,
	0,
/*A0*/
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
/*B0*/
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	D_UPPER|D_PRINT,
	0,
	0,
	0,
	0,
	D_PUNCT|D_PRINT,
	D_PUNCT|D_PRINT,
};
