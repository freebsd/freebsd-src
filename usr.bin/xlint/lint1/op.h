/*	$NetBSD: op.h,v 1.2 1995/07/03 21:24:27 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
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
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Various information about operators
 */
typedef	struct {
	u_int	m_binary : 1;	/* binary op. */
	u_int	m_logop : 1;	/* logical op., result is int */
	u_int	m_rqint : 1;	/* operands must have integer type */
	u_int	m_rqsclt : 1;	/* operands must have scalar type */
	u_int	m_rqatyp : 1;	/* operands must have arithmetic type */
	u_int	m_fold : 1;	/* operands should be folded */
	u_int	m_vctx : 1;	/* value context for left operand */
	u_int	m_tctx : 1;	/* test context for left operand */
	u_int	m_balance : 1;	/* op. requires balancing */
	u_int	m_sideeff : 1;	/* op. has side effect */
	u_int	m_tlansiu : 1;	/* warning if left op. is unsign. in ANSI C */
	u_int	m_transiu : 1;	/* warning if right op. is unsign. in ANSI C */
	u_int	m_tpconf : 1;	/* test possible precedence confusion */
	u_int	m_comp : 1;	/* op. performs comparison */
	u_int	m_enumop : 1;	/* valid operation on enums */
	u_int	m_badeop : 1;	/* dubious operation on enums */
	u_int	m_eqwarn : 1;	/* warning if on operand stems from == */
	const char *m_name;	/* name of op. */
} mod_t;

typedef	enum {
	NOOP	= 0,
	ARROW,
	POINT,
	NOT,
	COMPL,
	INC,
	DEC,
	INCBEF,
	DECBEF,
	INCAFT,
	DECAFT,
	UPLUS,
	UMINUS,
	STAR,
	AMPER,
	MULT,
	DIV,
	MOD,
	PLUS,
	MINUS,
	SHL,
	SHR,
	LT,
	LE,
	GT,
	GE,
	EQ,
	NE,
	AND,
	XOR,
	OR,
	LOGAND,
	LOGOR,
	QUEST,
	COLON,
	ASSIGN,
	MULASS,
	DIVASS,
	MODASS,
	ADDASS,
	SUBASS,
	SHLASS,
	SHRASS,
	ANDASS,
	XORASS,
	ORASS,
	NAME,
	CON,
	STRING,
	FSEL,
	CALL,
	COMMA,
	CVT,
	ICALL,
	LOAD,
	PUSH,
	RETURN,
	INIT,		/* pseudo op, not used in trees */
	CASE,		/* pseudo op, not used in trees */
	FARG		/* pseudo op, not used in trees */
#define NOPS	((int)FARG + 1)
} op_t;
