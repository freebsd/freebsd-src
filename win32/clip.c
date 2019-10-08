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

/*
 * clip.c : support for clipboard functions.
 * -amol
 *
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "sh.h"
#include "ed.h"



CCRETVAL
e_dosify_next(Char c)
{
	register Char *cp, *buf, *bp;
	int len;
    BOOL bDone = FALSE;


	USE(c);
	if (Cursor == LastChar)
		return(CC_ERROR);

	// worst case assumption
	buf = heap_alloc(( LastChar - Cursor + 1)*2*sizeof(Char));

	cp = Cursor;
	bp = buf;
	len = 0;

	while(  cp < LastChar) {
		if ( ((*cp & CHAR) == ' ') && ((cp[-1] & CHAR) != '\\') )
			bDone = TRUE;
		if (!bDone &&  (*cp & CHAR) == '/')  {
			*bp++ = '\\'  | (Char)(*cp & ~(*cp & CHAR) );
			*bp++ = '\\'  | (Char)(*cp & ~(*cp & CHAR) );

			len++;

			cp++;
		}
		else 
			*bp++ = *cp++;

		len++;
	}
	if (Cursor+ len >= InputLim) {
		heap_free(buf);
		return CC_ERROR;
	}
	cp = Cursor;
	bp = buf;
	while(len > 0) {
		*cp++ = *bp++;
		len--;
	}

	heap_free(buf);

	Cursor =  cp;

    if(LastChar < Cursor + len)
        LastChar = Cursor + len;

	return (CC_REFRESH);
}
/*ARGSUSED*/
CCRETVAL
e_dosify_prev(Char c)
{
	register Char *cp;

	USE(c);
	if (Cursor == InputBuf)
		return(CC_ERROR);
	/* else */

	cp = Cursor-1;
	/* Skip trailing spaces */
	while ((cp > InputBuf) && ( (*cp & CHAR) == ' '))
		cp--;

	while (cp > InputBuf) {
		if ( ((*cp & CHAR) == ' ') && ((cp[-1] & CHAR) != '\\') )
			break;
		cp--;
	}
	if(cp != InputBuf)
	  Cursor = cp + 1;
	else
	  Cursor = cp;
	
	return e_dosify_next(0);
}
extern BOOL ConsolePageUpOrDown(BOOL);
CCRETVAL
e_page_up(Char c) //blukas@broadcom.com
{
    USE(c);
	ConsolePageUpOrDown(TRUE);
	return (CC_REFRESH);
}
CCRETVAL
e_page_down(Char c)
{
    USE(c);
	ConsolePageUpOrDown(FALSE);
	return (CC_REFRESH);
}
