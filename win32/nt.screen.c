/*$Header: /p/tcsh/cvsroot/tcsh/win32/nt.screen.c,v 1.14 2006/03/14 01:22:57 mitr Exp $*/
/*
 * ed.screen.c: Editor/termcap-curses interface
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
#include "sh.h"


#include "ed.h"
#include "tc.h"
#include "ed.defns.h"


/* #define DEBUG_LITERAL */

/*
 * IMPORTANT NOTE: these routines are allowed to look at the current screen
 * and the current possition assuming that it is correct.  If this is not
 * true, then the update will be WRONG!  This is (should be) a valid
 * assumption...
 */



extern int nt_getsize(int*,int*,int*);
extern int nt_ClearEOL( void) ;
extern void NT_ClearEOD( void) ;
extern void NT_ClearScreen(void) ;
extern void NT_VisibleBell(void);
extern void NT_WrapHorizontal(void);

static int GetSize(int *lins, int *cols);

int DisplayWindowHSize;
	void
terminit(void)
{
	return;
}



int T_ActualWindowSize;

static	void	ReBufferDisplay	(void);


/*ARGSUSED*/
	void
TellTC(void)
{

	xprintf(CGETS(7, 1, "\n\tYou're using a Windows console.\n"));
}


	static void
ReBufferDisplay(void)
{
	register int i;
	Char **b;
	Char **bufp;
	int lins,cols;

	nt_getsize(&lins,&cols,&DisplayWindowHSize);

	b = Display;
	Display = NULL;
	if (b != NULL) {
		for (bufp = b; *bufp != NULL; bufp++)
			xfree((ptr_t) * bufp);
		xfree((ptr_t) b);
	}
	b = Vdisplay;
	Vdisplay = NULL;
	if (b != NULL) {
		for (bufp = b; *bufp != NULL; bufp++)
			xfree((ptr_t) * bufp);
		xfree((ptr_t) b);
	}
	TermH = cols;

	TermV = (INBUFSIZE * 4) / TermH + 1;/*FIXBUF*/
	b = (Char **) xmalloc((size_t) (sizeof(*b) * (TermV + 1)));
	for (i = 0; i < TermV; i++)
		b[i] = (Char *) xmalloc((size_t) (sizeof(*b[i]) * (TermH + 1)));
	b[TermV] = NULL;
	Display = b;
	b = (Char **) xmalloc((size_t) (sizeof(*b) * (TermV + 1)));
	for (i = 0; i < TermV; i++)
		b[i] = (Char *) xmalloc((size_t) (sizeof(*b[i]) * (TermH + 1)));
	b[TermV] = NULL;
	Vdisplay = b;
}

	void
SetTC(char *what, char *how)
{
	int li,win,co;

	nt_getsize(&li,&co,&win);
	if (!lstrcmp(what,"li")) {
		li = atoi(how);

	}else if(!lstrcmp(what,"co")) { //set window, not buffer size
		win = atoi(how);
	}
	else
		stderror(ERR_SYSTEM, "SetTC","Sorry, this function is not supported");

	ChangeSize(li,win);
	return;
}


/*
 * Print the termcap string out with variable substitution
 */
	void
EchoTC(Char **v)
{
	Char **globbed;
	char    cv[BUFSIZE];/*FIXBUF*/
	int     verbose = 0, silent = 0;
	static char *fmts = "%s\n", *fmtd = "%d\n";
	int li,co;


	setname("echotc");

	v = glob_all_or_error(v);
	globbed = v;
	cleanup_push(globbed, blk_cleanup);

	if (!v || !*v || *v[0] == '\0')
		goto end;
	if (v[0][0] == '-') {
		switch (v[0][1]) {
			case 'v':
				verbose = 1;
				break;
			case 's':
				silent = 1;
				break;
			default:
				stderror(ERR_NAME | ERR_TCUSAGE);
				break;
		}
		v++;
	}
	if (!*v || *v[0] == '\0')
		goto end;
	(void) StringCbCopy(cv,sizeof(cv), short2str(*v));

	GetSize(&li,&co);

	if(!lstrcmp(cv,"rows") || !lstrcmp(cv,"lines") ) {
		xprintf(fmtd,T_Lines);
		goto end;
	}
	else if(!lstrcmp(cv,"cols") ) {
		xprintf(fmtd,T_ActualWindowSize);
		goto end;
	}
	else if(!lstrcmp(cv,"buffer") ) {
		xprintf(fmtd,T_Cols);
		goto end;
	}
	else
		stderror(ERR_SYSTEM, "EchoTC","Sorry, this function is not supported");

end:
	cleanup_until(globbed);
}

int    GotTermCaps = 0;


	void
ResetArrowKeys(void)
{
}

	void
DefaultArrowKeys(void)
{
}


	int
SetArrowKeys(const CStr *name, XmapVal *fun, int type)
{
	UNREFERENCED_PARAMETER(name);
	UNREFERENCED_PARAMETER(fun);
	UNREFERENCED_PARAMETER(type);
	return -1;
}

	int
IsArrowKey(Char *name)
{
	UNREFERENCED_PARAMETER(name);
	return 0;
}

	int
ClearArrowKeys(const CStr *name)
{
	UNREFERENCED_PARAMETER(name);
	return -1;
}

	void
PrintArrowKeys(const CStr *name)
{
	UNREFERENCED_PARAMETER(name);
	return;
}


	void
BindArrowKeys(void)
{
	return;
}

#define GoodStr(ignore)  1
	void
SetAttributes(Char atr)
{
	atr &= ATTRIBUTES;
}

/* PWP 6-27-88 -- if the tty driver thinks that we can tab, we ask termcap */
	int
CanWeTab(void)
{
	return 1;
}

/* move to line <where> (first line == 0) as efficiently as possible; */
	void
MoveToLine(int where)
{
	int     del;

	if (where == CursorV)
		return;

	if (where > TermV) {
#ifdef DEBUG_SCREEN
		xprintf("MoveToLine: where is ridiculous: %d\r\n", where);
		flush();
#endif /* DEBUG_SCREEN */
		return;
	}

	del = where - CursorV;

	NT_MoveToLineOrChar(del, 1);

	CursorV = where;		/* now where is here */
}

/* move to character position (where) as efficiently as possible */
	void
MoveToChar(int where)		
{
	if (where == CursorH)
		return;

	if (where >= TermH) {
#ifdef DEBUG_SCREEN
		xprintf("MoveToChar: where is riduculous: %d\r\n", where);
		flush();
#endif /* DEBUG_SCREEN */
		return;
	}

	if (!where) {		/* if where is first column */
		//(void) putraw('\r');	/* do a CR */
		NT_MoveToLineOrChar(where, 0);
		flush();
		CursorH = 0;
		return;
	}

	NT_MoveToLineOrChar(where, 0);
	CursorH = where;		/* now where is here */
}

	void
so_write(register Char *cp, register int n)
{
	if (n <= 0)
		return;			/* catch bugs */

	if (n > TermH) {
		return;
	}

	do {
		if (*cp & LITERAL) {
			Char   *d;

			for (d = litptr + (*cp++ & ~LITERAL) * LIT_FACTOR; *d;
					d++)
				(void) putraw(*d);
		}
		else
			(void) putraw(*cp++);
		CursorH++;
	} while (--n);

	if (CursorH >= TermH) { /* wrap? */
		CursorH = 0;
		CursorV++;
		NT_WrapHorizontal();

	}
	else if(CursorH >= DisplayWindowHSize) {
		flush();
		NT_MoveToLineOrChar(CursorH,0);
	}
}


	void
DeleteChars(int num)		/* deletes <num> characters */
{
	if (num <= 0)
		return;

	if (!T_CanDel) {
#ifdef DEBUG_EDIT
		xprintf(CGETS(7, 16, "ERROR: cannot delete\r\n"));
#endif /* DEBUG_EDIT */
		flush();
		return;
	}

	if (num > TermH) {
#ifdef DEBUG_SCREEN
		xprintf(CGETS(7, 17, "DeletChars: num is riduculous: %d\r\n"), num);
		flush();
#endif /* DEBUG_SCREEN */
		return;
	}

}

/* Puts terminal in insert character mode, or inserts num characters in the
   line */
	void
Insert_write(register Char *cp, register int num)
{
	UNREFERENCED_PARAMETER(cp);

	if (num <= 0)
		return;
	if (!T_CanIns) {
#ifdef DEBUG_EDIT
		xprintf(CGETS(7, 18, "ERROR: cannot insert\r\n"));
#endif /* DEBUG_EDIT */
		flush();
		return;
	}

	if (num > TermH) {
#ifdef DEBUG_SCREEN
		xprintf(CGETS(7, 19, "StartInsert: num is riduculous: %d\r\n"), num);
		flush();
#endif /* DEBUG_SCREEN */
		return;
	}


}

/* clear to end of line.  There are num characters to clear */
	void
ClearEOL(int num)
{

	if (num <= 0)
		return;

	nt_ClearEOL();

}

	void
ClearScreen(void)
{				/* clear the whole screen and home */

	NT_ClearScreen();

}

	void
SoundBeep(void)
{				/* produce a sound */
	beep_cmd ();
	if (adrof(STRnobeep))
		return;

	if (adrof(STRvisiblebell))
		NT_VisibleBell();	/* visible bell */
	else
		MessageBeep(MB_ICONQUESTION);
}

	void
ClearToBottom(void)
{				/* clear to the bottom of the screen */
	NT_ClearEOD();

}

	void
GetTermCaps(void)
{
	int lins,cols;

	nt_getsize(&lins,&cols,&DisplayWindowHSize);

	GotTermCaps = 1;

	T_Cols = cols;
	T_Lines = lins;
	T_ActualWindowSize = DisplayWindowHSize;
	T_Margin = MARGIN_AUTO;
	T_CanCEOL  = 1;
	T_CanDel = 0;
	T_CanIns = 0;
	T_CanUP = 1;

	ReBufferDisplay();
	ClearDisp();

	return;
}
/* GetSize():
 *	Return the new window size in lines and cols, and
 *	true if the size was changed.
 */
	int
GetSize(int *lins, int *cols)
{

	int ret = 0;

	*lins = T_Lines;

	*cols = T_Cols;

	nt_getsize(lins,cols,&DisplayWindowHSize);

	// compare the actual visible window size,but return the console buffer size
	// this is seriously demented.
	ret =   (T_Lines != *lins || T_ActualWindowSize != DisplayWindowHSize);

	T_Lines = *lins;
	T_Cols = *cols;
	T_ActualWindowSize = DisplayWindowHSize;

	return ret;
}
	void
ChangeSize(int lins, int cols)
{

	int rc = 0;
	// here we're setting the window size, not the buffer size.
	// 
	nt_set_size(lins,cols);

	rc = GetSize(&lins,&cols);


	ReBufferDisplay();		/* re-make display buffers */
	ClearDisp();
}
	void
PutPlusOne(Char c, int width)
{
	extern int OldvcV;

	while (width > 1 && CursorH + width > DisplayWindowHSize)
		PutPlusOne(' ', 1);
	if ((c & LITERAL) != 0) { 
		Char *d;
		for (d = litptr + (c & ~LITERAL) * LIT_FACTOR; *d; d++)
			(void) putwraw(*d);
	} else {
		(void) putwraw(c);
	}

	Display[CursorV][CursorH++] = (Char) c;
	while (--width > 0)
		Display[CursorV][CursorH++] = CHAR_DBWIDTH;

	if (CursorH >= TermH) {	/* if we must overflow */
		CursorH = 0;
		CursorV++;
		OldvcV++;
		NT_WrapHorizontal();
	}
	else if(CursorH >= DisplayWindowHSize) {
		NT_MoveToLineOrChar(CursorH,0);
	}
}
