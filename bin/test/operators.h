/*-
 * Copyright (c) 1993, 1994
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
 *
 *	@(#)operators.h	8.3 (Berkeley) 4/2/94
 *	$Id: operators.h,v 1.5 1997/02/22 14:06:22 peter Exp $
 */

#define	NOT		0
#define	ISBLOCK		1
#define	ISCHAR		2
#define	ISDIR		3
#define	ISEXIST		4
#define	ISFILE		5
#define	ISSETGID	6
#define	ISSYMLINK	7
#define	ISSTICKY	8
#define	STRLEN		9
#define	ISFIFO		10
#define	ISREAD		11
#define	ISSIZE		12
#define	ISTTY		13
#define	ISSETUID	14
#define	ISWRITE		15
#define	ISEXEC		16
#define	NULSTR		17
#define	ISSOCK		18

#define	FIRST_BINARY_OP	19
#define	OR1		FIRST_BINARY_OP
#define	OR2		(FIRST_BINARY_OP + 1)
#define	AND1		(FIRST_BINARY_OP + 2)
#define	AND2		(FIRST_BINARY_OP + 3)
#define	STREQ		(FIRST_BINARY_OP + 4)
#define	STRNE		(FIRST_BINARY_OP + 5)
#define	EQ		(FIRST_BINARY_OP + 6)
#define	NE		(FIRST_BINARY_OP + 7)
#define	GT		(FIRST_BINARY_OP + 8)
#define	LT		(FIRST_BINARY_OP + 9)
#define	LE		(FIRST_BINARY_OP + 10)
#define	GE		(FIRST_BINARY_OP + 11)


#define	OP_INT		1	/* arguments to operator are integer */
#define	OP_STRING	2	/* arguments to operator are string */
#define	OP_FILE		3	/* argument is a file name */

extern const char *const unary_op[];
extern const char *const binary_op[];
extern const char *const andor_op[];
extern const char op_priority[];
extern const char op_argflag[];
