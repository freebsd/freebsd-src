/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
 *	@(#)token.h	8.1 (Berkeley) 6/6/93
 */

#define token		(cx.x_token)
#define token_num	(cx.x_val.v_num)
#define token_str	(cx.x_val.v_str)

#define T_EOL		1
#define T_EOF		2
#define T_COMP		3
#define T_PLUS		4
#define T_MINUS		5
#define T_MUL		6
#define T_DIV		7
#define T_LP		8
#define T_RP		9
#define T_LB		10
#define T_RB		11
#define T_DOLLAR	12
#define T_COMMA		13
#define T_QUEST		14
#define T_COLON		15
#define T_CHAR		16
#define T_STR		17
#define T_NUM		18
#define T_MOD		19
#define T_XOR		20
#define T_DQ		21		/* $? */
#define T_GE		22
#define T_RS		23
#define T_GT		24
#define T_LE		25
#define T_LS		26
#define T_LT		27
#define T_EQ		28
#define T_ASSIGN	29
#define T_NE		30
#define T_NOT		31
#define T_ANDAND	32
#define T_AND		33
#define T_OROR		34
#define T_OR		35

#define T_IF		40
#define T_THEN		41
#define T_ELSIF		42
#define T_ELSE		43
#define T_ENDIF		44
