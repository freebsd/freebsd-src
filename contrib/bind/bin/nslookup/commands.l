%{
/* $FreeBSD: src/contrib/bind/bin/nslookup/commands.l,v 1.4.2.1 2000/07/03 22:47:07 ps Exp $ */
/*
 * Copyright (c) 1985
 *    The Regents of the University of California.  All rights reserved.
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
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

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifndef lint
static char sccsid[] = "@(#)commands.l	5.13 (Berkeley) 7/24/90";
#endif /* not lint */

/*
 *******************************************************************************
 *
 *  commands.l
 *
 *	Andrew Cherenson	CS298-26  Fall 1985
 *
 *	Lex input file for the nslookup program command interpreter.
 *	When a sequence is recognized, the associated action
 *	routine is called. The action routine may need to
 *	parse the string for additional information.
 *
 *  Recognized commands: (identifiers are shown in uppercase)
 *
 *	server NAME	- set default server to NAME, using default server
 *	lserver NAME	- set default server to NAME, using initial server
 *	finger [NAME]	- finger the optional NAME
 *	exit		- exit the program
 *	root		- set default server to the root
 *	ls NAME		- list the domain NAME
 *	view FILE	- sorts and view the file with more
 *	set OPTION	- set an option
 *	help		- print help information
 *	?		- print help information
 *	NAME		- print info about the host/domain NAME
 *			  using default server.
 *	NAME1 NAME2	- as above, but use NAME2 as server
 *
 *
 *   yylex Results:
 *	0		upon end-of-file.
 *	1		after each command.
 *
 *******************************************************************************
 */

#include "port_before.h"
#include <sys/types.h>
#include <histedit.h>
#include "port_after.h"
#include "res.h"

extern char rootServerName[];
extern void PrintHelp();
extern void ViewList(char *);

#define YY_INPUT(buf, result, max_size) \
	{ \
	nslookup_yy_input((char *)buf, &result, \
			  max_size, yy_current_buffer->yy_is_interactive); \
	}

const char *
nslookup_prompt()
{
	return ("> ");
}

int
nslookup_yy_input(buf, result, max_size, intr)
	char *	buf;
	int *	result;
	int	max_size;
	int	intr;
{
	static EditLine *el = NULL;
	static History *hist = NULL;
	int num = 0;
	const char *bp = NULL;

	if (intr) {
		if (!el) {
			el = el_init("nslookup", yyin, yyout);
			hist = history_init();
			history(hist, H_EVENT, 100);
			el_set(el, EL_HIST, history, hist);
			el_set(el, EL_EDITOR, "emacs");
			el_set(el, EL_PROMPT, nslookup_prompt);
			el_set(el, EL_SIGNAL, 1);
			el_source(el, NULL);
		}
	
		if ((bp = el_gets(el, &num)) == NULL || num == 0) {
			*result = 0;
			return;
		}
	
		*result = (num > max_size) ? max_size : num;
		strncpy(buf, bp, *result);
		history(hist, H_ENTER, bp);
	} else {
		if ( ((*result = fread( buf, 1, max_size, yyin )) == 0)
			&& ferror( yyin ) )
                	yy_fatal_error( "input in flex scanner failed" );
	}

	return;
}

%}
WS	[ \t]
FLET	[A-Za-z0-9.*\\_]
LET	[A-Za-z0-9.*_]
NAME	[A-Za-z0-9.*=_/-]
%%
^{WS}*server{WS}+{LET}{NAME}*{WS}*$	{
					    /*
					     * 0 == use current server to find
					     *	    the new one.
					     * 1 == use original server to find
					     *	    the new one.
					     */
					    SetDefaultServer(yytext, 0);
					    return(1);
					}
^{WS}*lserver{WS}+{LET}{NAME}*{WS}*$	{
					    SetDefaultServer(yytext, 1);
					    return(1);
					}
^{WS}*exit{WS}*$			{
					    return(0);
					}
^{WS}*root{WS}*$			{
					    SetDefaultServer(rootServerName, 1);
					    return(1);
					}
^{WS}*finger({WS}+{LET}{NAME}*)?{WS}+>>?{WS}*{NAME}+{WS}*$	{
					    /*
					     * 2nd arg.
					     *  0 == output to stdout
					     *  1 == output to file
					     */
					    Finger(yytext, 1);
					    return(1);
					}
^{WS}*finger({WS}+{LET}{NAME}*)?{WS}*$	{
					    Finger(yytext, 0);
					    return(1);
					}
^{WS}*ls{WS}+(("-a"|"-d"|"-h"|"-m"|"-s"){WS}+)?{LET}{NAME}*{WS}+>>?{WS}+{NAME}+{WS}*$	{
					    /*
					     * 2nd arg.
					     *  0 == output to stdout
					     *  1 == output to file
					     */
					    ListHosts(yytext, 1);
					    return(1);
					}
^{WS}*ls{WS}+(("-a"|"-d"|"-h"|"-m"|"-s"){WS}+)?{LET}{NAME}*{WS}*$	{
					    ListHosts(yytext, 0);
					    return(1);
					}
^{WS}*ls{WS}+-t{WS}+({LET}{NAME}*{WS}+)?{LET}{NAME}*{WS}+>>?{WS}+{NAME}+{WS}*$ {
					    /*
					     * 2nd arg.
					     *  0 == output to stdout
					     *  1 == output to file
					     */
					    ListHostsByType(yytext, 1);
					    return(1);
					}
^{WS}*ls{WS}+-t{WS}+({LET}{NAME}*{WS}+)?{LET}{NAME}*{WS}*$ {
					    ListHostsByType(yytext, 0);
					    return(1);
					}
^{WS}*set{WS}+{NAME}+{WS}*$		{
					    SetOption(yytext);
					    return(1);
					}
^{WS}*help{WS}*$			{
					    PrintHelp();
					    return(1);
					}
^{WS}*"?"{WS}*$				{
					    extern void PrintHelp();

					    PrintHelp();
					    return(1);
					}
^{WS}*{FLET}{NAME}*{WS}+>>?{WS}*{NAME}+{WS}*$	{
					    /*
					     * 0 == output to stdout
					     * 1 == output to file
					     */
					    LookupHost(yytext, 1);
					    return(1);
					}
^{WS}*{FLET}{NAME}*{WS}*$		{
					    LookupHost(yytext, 0);
					    return(1);
					}
^{WS}*{FLET}{NAME}*{WS}+{LET}{NAME}*{WS}+>>?{WS}*{NAME}+{WS}*$	{
					    /*
					     * 0 == output to stdout
					     * 1 == output to file
					     */
					    LookupHostWithServer(yytext, 1);
					    return(1);
					}
^{WS}*{FLET}{NAME}*{WS}+{LET}{NAME}*{WS}*$	{
					    LookupHostWithServer(yytext, 0);
					    return(1);
					}
^{WS}*\n				{
					    return(1);
					}
^.*\n					{
					    printf("Unrecognized command: %s",
							yytext);
					    return(1);
					}
\n					{ ; }
%%
