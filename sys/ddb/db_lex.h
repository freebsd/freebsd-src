/*-
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * $FreeBSD: src/sys/ddb/db_lex.h,v 1.14.10.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _DDB_DB_LEX_H_
#define	_DDB_DB_LEX_H_

/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */
/*
 * Lexical analyzer.
 */
void	 db_flush_lex(void);
char	*db_get_line(void);
void	 db_inject_line(const char *command);
int	 db_read_line(void);
int	 db_read_token(void);
void	 db_unread_token(int t);

extern db_expr_t	db_tok_number;
#define	TOK_STRING_SIZE		120
extern char	db_tok_string[TOK_STRING_SIZE];

#define	tEOF		(-1)
#define	tEOL		1
#define	tNUMBER		2
#define	tIDENT		3
#define	tPLUS		4
#define	tMINUS		5
#define	tDOT		6
#define	tSTAR		7
#define	tSLASH		8
#define	tEQ		9
#define	tLPAREN		10
#define	tRPAREN		11
#define	tPCT		12
#define	tHASH		13
#define	tCOMMA		14
#define	tDITTO		15
#define	tDOLLAR		16
#define	tEXCL		17
#define	tSHIFT_L	18
#define	tSHIFT_R	19
#define	tDOTDOT		20
#define	tSEMI		21

#endif /* !_DDB_DB_LEX_H_ */
