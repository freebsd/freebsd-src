/*
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator   or   Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the rights
 * to redistribute these changes.
 */
/*
 *  nxtarg -- strip off arguments from a string
 *
 *  Usage:  p = nxtarg (&q,brk);
 *	char *p,*q,*brk;
 *	extern char _argbreak;
 *
 *	q is pointer to next argument in string
 *	after call, p points to string containing argument,
 *	q points to remainder of string
 *
 *  Leading blanks and tabs are skipped; the argument ends at the
 *  first occurence of one of the characters in the string "brk".
 *  When such a character is found, it is put into the external
 *  variable "_argbreak", and replaced by a null character; if the
 *  arg string ends before that, then the null character is
 *  placed into _argbreak;
 *  If "brk" is 0, then " " is substituted.
 *
 *  HISTORY
 * 01-Jul-83  Steven Shafer (sas) at Carnegie-Mellon University
 *	Bug fix: added check for "back >= front" in loop to chop trailing
 *	white space.
 *
 * 20-Nov-79  Steven Shafer (sas) at Carnegie-Mellon University
 *	Rewritten for VAX.  By popular demand, a table of break characters
 *	has been added (implemented as a string passed into nxtarg).
 *
 *  Originally	from klg (Ken Greer); IUS/SUS UNIX.
 */

char _argbreak;
char *skipto();

char *nxtarg (q,brk)
char **q,*brk;
{
	register char *front,*back;
	front = *q;			/* start of string */
	/* leading blanks and tabs */
	while (*front && (*front == ' ' || *front == '\t')) front++;
	/* find break character at end */
	if (brk == 0)  brk = " ";
	back = skipto (front,brk);
	_argbreak = *back;
	*q = (*back ? back+1 : back);	/* next arg start loc */
	/* elim trailing blanks and tabs */
	back -= 1;
	while ((back >= front) && (*back == ' ' || *back == '\t')) back--;
	back++;
	if (*back)  *back = '\0';
	return (front);
}
