/*
 * Copyright (c) 1996, 1997, 1998 Shigio Yamaguchi. All rights reserved.
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
 *	This product includes software developed by Shigio Yamaguchi.
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
 *	C.h						20-Aug-98
 */
#define DECLARATIONS	0
#define RULES		1
#define PROGRAMS	2

#define C___P		1001
#define C_AUTO		1002
#define C_BREAK		1003
#define C_CASE		1004
#define C_CHAR		1005
#define C_CONTINUE	1006
#define C_DEFAULT	1007
#define C_DO		1008
#define C_DOUBLE	1009
#define C_ELSE		1010
#define C_EXTERN	1011
#define C_FLOAT		1012
#define C_FOR		1013
#define C_GOTO		1014
#define C_IF		1015
#define C_INT		1016
#define C_LONG		1017
#define C_REGISTER	1018
#define C_RETURN	1019
#define C_SHORT		1020
#define C_SIZEOF	1021
#define C_STATIC	1022
#define C_STRUCT	1023
#define C_SWITCH	1024
#define C_TYPEDEF	1025
#define C_UNION		1026
#define C_UNSIGNED	1027
#define C_VOID		1028
#define C_WHILE		1029
#define CP_ELIF		2001
#define CP_ELSE		2002
#define CP_DEFINE	2003
#define CP_IF		2004
#define CP_IFDEF	2005
#define CP_IFNDEF	2006
#define CP_INCLUDE	2007
#define CP_PRAGMA	2008
#define CP_SHARP	2009
#define CP_ERROR	2010
#define CP_UNDEF	2011
#define CP_ENDIF	2012
#define CP_LINE		2013
#define YACC_SEP	3001
#define YACC_BEGIN	3002
#define YACC_END	3003
#define YACC_LEFT	3004
#define YACC_NONASSOC	3005
#define YACC_RIGHT	3006
#define YACC_START	3007
#define YACC_TOKEN	3008
#define YACC_TYPE	3009

#define IS_CTOKEN(c)	(c > 1000 && c < 2001)
#define IS_CPTOKEN(c)	(c > 2000 && c < 3001)
#define IS_YACCTOKEN(c)	(c > 3000 && c < 4001)
#define MAXPIFSTACK	100
