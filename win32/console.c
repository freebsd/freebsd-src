/*$Header: /p/tcsh/cvsroot/tcsh/win32/console.c,v 1.9 2006/08/27 01:13:28 amold Exp $*/
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
 * console.c: hacks to do various cursor movement/attribute things
 * -amol
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincon.h>
#include <stdio.h>
#include "ntport.h"


// int to SHORT. caused by all the stupid functions that take WORDs
#pragma warning(disable:4244)

void ScrollBuf(HANDLE,CONSOLE_SCREEN_BUFFER_INFO*,int);
void NT_MoveToLineOrChar(int ,int ) ;
WORD get_attributes();


#define	FSHIN	16		/* Preferred desc for shell input */
#define	FSHOUT	17		/* Preferred desc for shell input */

#define FOREGROUND_BLACK (FOREGROUND_RED |FOREGROUND_GREEN | FOREGROUND_BLUE)
#define FOREGROUND_WHITE 0
#define BACKGROUND_BLACK (BACKGROUND_RED |BACKGROUND_GREEN | BACKGROUND_BLUE)
#define BACKGROUND_WHITE 0

static WORD wNormalAttributes;


static int nt_is_raw;
//
// The following are used to optimize some console routines. It avoids having
// to call GetConsoleScreenBufferInfo.
// Seems to have helped the speed a bit. -amol
//
HANDLE ghstdout;
HANDLE ghReverse;

//
// This function is called to set the values for above variables.
//
void redo_console(void) {

	CONSOLE_SCREEN_BUFFER_INFO scrbuf;
	HANDLE hTemp= GetStdHandle(STD_OUTPUT_HANDLE);
	WORD dbga;
	DWORD wrote;
	COORD origin = {0,0};

	if (!DuplicateHandle(GetCurrentProcess(),hTemp,GetCurrentProcess(),
				&ghstdout,0,TRUE,DUPLICATE_SAME_ACCESS) ) {
		;
	}

	if(!GetConsoleScreenBufferInfo(ghstdout, &scrbuf) ) {
		wNormalAttributes = FOREGROUND_BLACK | BACKGROUND_WHITE;
	}
	else
		wNormalAttributes = scrbuf.wAttributes;

	ghReverse = CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			CONSOLE_TEXTMODE_BUFFER,
			NULL);

	dbga = ((wNormalAttributes & 0x00f0) >> 4) |
		((wNormalAttributes & 0x000f) << 4) ;

	FillConsoleOutputAttribute(ghReverse,dbga,
			scrbuf.dwSize.X*scrbuf.dwSize.Y,
			origin,
			&wrote);
}
void nt_term_cleanup(void) {
	CloseHandle(ghstdout);
}
void nt_term_init() {

	DWORD dwmode;
	HANDLE hinput =GetStdHandle(STD_INPUT_HANDLE);

	if (!GetConsoleMode(hinput,&dwmode) ){
		;
	}
	if(!SetConsoleMode(hinput,dwmode | ENABLE_WINDOW_INPUT) ){
		return;
	}

	redo_console();

	return;
}
int do_nt_check_cooked_mode(void) {

	return !nt_is_raw;
}
void do_nt_raw_mode() {

	DWORD dwmode;
	HANDLE hinput =(HANDLE)_get_osfhandle(FSHIN);

	if (hinput == INVALID_HANDLE_VALUE)
		return;
	if (!GetConsoleMode(hinput,&dwmode) ){
		;
	}
	if(!SetConsoleMode(hinput,dwmode & (~(
						ENABLE_LINE_INPUT |ENABLE_ECHO_INPUT 
						| ENABLE_PROCESSED_INPUT)| ENABLE_WINDOW_INPUT )
				) ){
		return;
	}
	nt_is_raw = 1;
	return;
}
void do_nt_cooked_mode() {

	DWORD dwmode;
	HANDLE hinput =(HANDLE)_get_osfhandle(FSHIN);

	if (hinput == INVALID_HANDLE_VALUE)
		return;
	if (!GetConsoleMode(hinput,&dwmode) ){
		;
	}
	if(!SetConsoleMode(hinput,dwmode | ( (
						ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT |
						ENABLE_PROCESSED_INPUT) )
				) ){
	}
	nt_is_raw = 0;
	return;
}
//
// this function is a bit ugly, but I don't know how to do it better
// -amol
//
int nt_ClearEOL( void) {

	CONSOLE_SCREEN_BUFFER_INFO scrbuf;
	HANDLE hStdout =ghstdout ;
	DWORD numwrote;
	char errbuf[128];/*FIXME: uninitialized*/
	int num=0;
	COORD savepos;


	if (hStdout == INVALID_HANDLE_VALUE){
		ExitProcess(0xFFFF);
	}
	if(!GetConsoleScreenBufferInfo(hStdout, &scrbuf) ) {
		return 0 ;
	}
	num =2048;

	savepos = scrbuf.dwCursorPosition;
	if (!FillConsoleOutputCharacter(hStdout,' ',num,scrbuf.dwCursorPosition,
				&numwrote) ){
		dprintf("error from FillCons %s",errbuf);
	}
	else if (!FillConsoleOutputAttribute(hStdout,scrbuf.wAttributes, num,
				scrbuf.dwCursorPosition,&numwrote)) {
		dprintf("error from FillConsAttr %s",errbuf);
	}
	return 0;
}
void nt_move_next_tab(void) {

	CONSOLE_SCREEN_BUFFER_INFO scrbuf;
	HANDLE hStdout = ghstdout;
	int where;

	if(!GetConsoleScreenBufferInfo(hStdout, &scrbuf) ) {
		;
	}
	where = 8 - (scrbuf.dwCursorPosition.X+1)%8;
	scrbuf.dwCursorPosition.X += where;
	if (!SetConsoleCursorPosition(hStdout, scrbuf.dwCursorPosition) ) {
		;
	}

}
void NT_VisibleBell(void) {

	if(ghReverse != INVALID_HANDLE_VALUE) {
		SetConsoleActiveScreenBuffer(ghReverse);
		Sleep(100);
		SetConsoleActiveScreenBuffer(ghstdout);

	}

}
void NT_WrapHorizontal(void) {
	SMALL_RECT wnd;
	CONSOLE_SCREEN_BUFFER_INFO scrbuf;


	if (ghstdout == INVALID_HANDLE_VALUE){
		return;
	}
	if(!GetConsoleScreenBufferInfo(ghstdout, &scrbuf) ) {
		return;
	}
	//absolute movement
	wnd.Left =  0;//scrbuf.srWindow.Left ;
	wnd.Right = scrbuf.srWindow.Right- scrbuf.srWindow.Left + 1;
	wnd.Top =  scrbuf.srWindow.Top;
	wnd.Bottom = scrbuf.srWindow.Bottom;

	SetConsoleWindowInfo(ghstdout,TRUE,&wnd);
}
void ScrollBufHorizontal(HANDLE hOut, CONSOLE_SCREEN_BUFFER_INFO *scrbuf,
		int where) {
	SMALL_RECT wnd;
	int diff;
	CHAR_INFO chr;


	//absolute movement
	wnd.Left = (where - scrbuf->srWindow.Right) + scrbuf->srWindow.Left ;
	wnd.Right = where;
	wnd.Top =  scrbuf->srWindow.Top;
	wnd.Bottom = scrbuf->srWindow.Bottom;

	//diff = scrbuf->srWindow.Right - where;
	//dprintf("\tdiff1 %d\n",diff);

	diff = scrbuf->dwSize.X - where -1;

	if (diff < 0) { //would scroll past console buffer

		chr.Char.AsciiChar = ' ';
		chr.Attributes = scrbuf->wAttributes;

		scrbuf->dwCursorPosition.Y  = scrbuf->srWindow.Top ;
		scrbuf->dwCursorPosition.X = scrbuf->srWindow.Right+ diff;

		dprintf("scroll diff %d\n",diff);
		if (!ScrollConsoleScreenBuffer(hOut,&(scrbuf->srWindow),
					NULL,
					scrbuf->dwCursorPosition,&chr)) 
			;

		return;
	}

	SetConsoleWindowInfo(hOut,TRUE,&wnd);
}
// relative movement of "where".  line is 1 if we want to move to a line,
// or 0 if the movement is horizontal
void NT_MoveToLineOrChar(int where,int line) {

	CONSOLE_SCREEN_BUFFER_INFO scrbuf;
	HANDLE hStdout = ghstdout;


	if (hStdout == INVALID_HANDLE_VALUE){
		return;
	}
	if(!GetConsoleScreenBufferInfo(hStdout, &scrbuf) ) {
		return;
	}

	if (line){
		if ( ((scrbuf.dwCursorPosition.Y+where)> (scrbuf.srWindow.Bottom-1))
				&&( where >0)){
			ScrollBuf(hStdout,&scrbuf,where);
			scrbuf.dwCursorPosition.Y += where;
		}
		else
			scrbuf.dwCursorPosition.Y += where;
	}
	else{
		if ( (where> (scrbuf.srWindow.Right)) &&( where >0)){
			ScrollBufHorizontal(hStdout,&scrbuf,where);
		}
		scrbuf.dwCursorPosition.X = where;
	}
	if (scrbuf.dwCursorPosition.X < 0 || scrbuf.dwCursorPosition.Y <0)
		return;
	if (!SetConsoleCursorPosition(hStdout, scrbuf.dwCursorPosition) ) {
		return;
	}

}
void ScrollBuf(HANDLE hOut, CONSOLE_SCREEN_BUFFER_INFO *scrbuf,int where) {
	SMALL_RECT wnd;
	int diff;
	CHAR_INFO chr;
	COORD newpos;


	wnd.Left = 0;
	wnd.Right = 0;
	wnd.Top =  where;
	wnd.Bottom = where;

	//dwSize is not 0-based, so add 1 to proposed location
	diff = scrbuf->srWindow.Bottom + where + 1; 

	diff = scrbuf->dwSize.Y - diff;

	if (diff < 0) { //would scroll past console buffer

		chr.Char.AsciiChar = ' ';
		chr.Attributes = scrbuf->wAttributes;

		newpos.Y  = scrbuf->srWindow.Top + diff;
		newpos.X = scrbuf->srWindow.Left;

		dprintf("scroll diff %d\n",diff);
		if (!ScrollConsoleScreenBuffer(hOut,&(scrbuf->srWindow),
					NULL,
					newpos,&chr)) 
			;

		// need this to be in sync with tcsh
		scrbuf->dwCursorPosition.Y += diff; 
		return;
	}

	SetConsoleWindowInfo(hOut,FALSE,&wnd);
}
BOOL ConsolePageUpOrDown(BOOL Up) {

	HANDLE hStdout = ghstdout;
	CONSOLE_SCREEN_BUFFER_INFO scrbuf;
	SMALL_RECT srect;
	short diff;

	if(!GetConsoleScreenBufferInfo(hStdout, &scrbuf) ) {
		return FALSE;
	}
	diff = scrbuf.srWindow.Bottom -scrbuf.srWindow.Top+1 ;


	if (Up)
		diff = -diff;

	if ((scrbuf.srWindow.Top + diff > 0) &&
			(scrbuf.srWindow.Bottom + diff < scrbuf.dwSize.Y)) { 
		srect.Top = diff;  
		srect.Bottom = diff;
		srect.Left = 0;    
		srect.Right = 0;  

		if (! SetConsoleWindowInfo( hStdout, FALSE, &srect)) {
			return FALSE;
		}
	} 

	return TRUE;
}
int nt_getsize(int * lins, int * cols, int *visiblecols) {
	CONSOLE_SCREEN_BUFFER_INFO scrbuf;
	HANDLE hStdout = ghstdout;

	if(!GetConsoleScreenBufferInfo(hStdout, &scrbuf) ) {
		;
	}
	*lins = scrbuf.srWindow.Bottom -scrbuf.srWindow.Top+1 ;

	if(visiblecols)
		*visiblecols = scrbuf.srWindow.Right -scrbuf.srWindow.Left +1;

	*cols = scrbuf.dwSize.X;
	return 1;
}
void nt_set_size(int lins, int cols) {
	SMALL_RECT srect;
	CONSOLE_SCREEN_BUFFER_INFO scrbuf;
	int expand;

	/* The screen buffer visible window is specified as co-ordinates
	 * not size. Therefore, it must be zero-based
	 */
	cols--;
	lins--;

	srect.Left = srect.Top = 0;
	srect.Right = cols;
	srect.Bottom = lins;

	if(!GetConsoleScreenBufferInfo(ghstdout, &scrbuf) ) 
		return;

	expand = 0;
	if (scrbuf.dwSize.X < cols){
		expand = 1;
		scrbuf.dwSize.X = cols+1;
	}
	if (scrbuf.dwSize.Y < lins){
		expand = 1;
		scrbuf.dwSize.Y = lins+1;
	}

	if (expand && !SetConsoleScreenBufferSize(ghstdout,scrbuf.dwSize))
		return;

	if(!SetConsoleWindowInfo(ghstdout,TRUE,&srect)){
		int err;
		err=GetLastError();
		dprintf("error %d\n",err);
	}
}
void NT_ClearEOD(void) {
	CONSOLE_SCREEN_BUFFER_INFO scrbuf;
	DWORD numwrote;
	COORD origin;
	int ht,wt;
	HANDLE hStdout = ghstdout;//GetStdHandle(STD_OUTPUT_HANDLE);

	if (hStdout == INVALID_HANDLE_VALUE){
		return ;
	}
	if(!GetConsoleScreenBufferInfo(hStdout, &scrbuf) ) {
		return ;
	}
	origin = scrbuf.dwCursorPosition;
	ht = scrbuf.dwSize.Y - origin.Y;
	wt = scrbuf.dwSize.X - origin.X;
	if(!FillConsoleOutputCharacter(hStdout,' ',ht*wt,origin,&numwrote) ) {
		return ;
	}
	if (!FillConsoleOutputAttribute(hStdout,scrbuf.wAttributes, ht*wt,
				scrbuf.dwCursorPosition,&numwrote)) {
		return;
	}
	return;
}
void NT_ClearScreen(void) {
	CONSOLE_SCREEN_BUFFER_INFO scrbuf;
	DWORD numwrote;
	COORD origin={0,0};
	HANDLE hStdout = ghstdout;//GetStdHandle(STD_OUTPUT_HANDLE);

	if (hStdout == INVALID_HANDLE_VALUE){
		;
	}
	if(!GetConsoleScreenBufferInfo(hStdout, &scrbuf) ) {
		;
	}
	origin.X = scrbuf.srWindow.Left;
	origin.Y = scrbuf.srWindow.Top;
	if(!FillConsoleOutputCharacter(hStdout,' ',scrbuf.dwSize.X*scrbuf.dwSize.Y,
				origin,&numwrote) ) {
		;
	}
	if (!FillConsoleOutputAttribute(hStdout,scrbuf.wAttributes,
				scrbuf.dwSize.X*scrbuf.dwSize.Y,origin,&numwrote)) {
		;
	}
	if (!SetConsoleCursorPosition(hStdout, origin) ) { // home cursor
		;
	}
	return;
}
void NT_ClearScreen_WholeBuffer(void) {
	CONSOLE_SCREEN_BUFFER_INFO scrbuf;
	DWORD numwrote;
	COORD origin={0,0};
	HANDLE hStdout = ghstdout;

	if (hStdout == INVALID_HANDLE_VALUE){
		;
	}
	if(!GetConsoleScreenBufferInfo(hStdout, &scrbuf) ) {
		;
	}
	if(!FillConsoleOutputCharacter(hStdout,' ',scrbuf.dwSize.X*scrbuf.dwSize.Y,
				origin,&numwrote) ) {
		;
	}
	if (!FillConsoleOutputAttribute(hStdout,scrbuf.wAttributes,
				scrbuf.dwSize.X*scrbuf.dwSize.Y,origin,&numwrote)) {
		;
	}
	if (!SetConsoleCursorPosition(hStdout, origin) ) { // home cursor
		;
	}
	return;
}

#ifndef COLOR_LS_F
void set_cons_attr(char *attr2) {
	char cp[3];
	USHORT attr;
	HANDLE outhandle = (HANDLE)_get_osfhandle(FSHOUT);
	static WORD old_attribs;
	CONSOLE_SCREEN_BUFFER_INFO scrbuf;

	if (!old_attribs) {
		if(!GetConsoleScreenBufferInfo(outhandle, &scrbuf) ) {
			return;
		}
		old_attribs = scrbuf.wAttributes;
	}
	cp[0] = (unsigned char)(attr2[0]);
	cp[1] = (unsigned char)(attr2[1]);
	cp[2] = 0;
	if (cp[0] != 'g' || cp[1] != 'g')
		attr = (USHORT)strtol(cp,NULL,16);
	else{
		attr = old_attribs;
		old_attribs=0;
	}

	SetConsoleTextAttribute(outhandle, attr );
}
#endif /* !COLOR_LS_F */


/*
   color escape sequences (ISO 6429, aixterm)
   - nayuta
 */


WORD get_attributes() {
	CONSOLE_SCREEN_BUFFER_INFO scrbuf;
	if (!GetConsoleScreenBufferInfo(ghstdout, &scrbuf))
		return 0x70; // ERROR: return white background, black text
	return scrbuf.wAttributes;
}


#ifndef COMMON_LVB_REVERSE_VIDEO
#define COMMON_LVB_REVERSE_VIDEO   0x4000
#define COMMON_LVB_UNDERSCORE      0x8000
#endif


void set_attributes(const unsigned char *color) {

	static const int colors[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
	WORD wAttributes;
	const char *t;

	if (color[0] == '\x1b' && color[1] == '[')
		color += 2;

	if (!('0' <= color[0] && color[0] <= '9')) {
		SetConsoleTextAttribute(ghstdout, wNormalAttributes);
		return;
	}

	wAttributes = get_attributes();
	t = (char*)color;

	while (t) {
		int n = atoi(t);

		if ((t = strchr(t, ';')) != NULL)
			t++;

		if      (n == 0)				// Normal (default)
			wAttributes = wNormalAttributes;
		else if (n == 1)				// Bold
			wAttributes |= FOREGROUND_INTENSITY;
		else if (n == 4)				// Underlined
			wAttributes |= COMMON_LVB_UNDERSCORE;
		else if (n == 5)				// Blink (appears as BACKGROUND_INTENSITY)
			wAttributes |= BACKGROUND_INTENSITY;
		else if (n == 7)				// Inverse
			wAttributes |= COMMON_LVB_REVERSE_VIDEO;
		else if (n == 21)				// Not bold
			wAttributes &= ~FOREGROUND_INTENSITY;
		else if (n == 24)				// Not underlined
			wAttributes &= ~COMMON_LVB_UNDERSCORE;
		else if (n == 25)				// Steady (not blinking)
			wAttributes &= ~BACKGROUND_INTENSITY;
		else if (n == 27)				// Positive (not inverse)
			wAttributes &= ~COMMON_LVB_REVERSE_VIDEO;
		else if (30 <= n && n <= 37)	// Set foreground color
			wAttributes = (wAttributes & ~0x0007) | colors[n - 30];
		else if (n == 39)				// Set foreground color to default
			wAttributes = (wAttributes & ~0x0007) | (wNormalAttributes & 0x0007);
		else if (40 <= n && n <= 47)	// Set background color
			wAttributes = (wAttributes & ~0x0070) | (colors[n - 40] << 4);
		else if (n == 49)				// Set background color to default
			wAttributes = (wAttributes & ~0x0070) | (wNormalAttributes & 0x0070);
		else if (90 <= n && n <= 97)	// Set foreground color (bright)
			wAttributes = (wAttributes & ~0x0007) | colors[n - 90]
				| FOREGROUND_INTENSITY;
		else if (100 <= n && n <= 107)	// Set background color (bright)
			wAttributes = (wAttributes & ~0x0070) | (colors[n - 100] << 4)
				| BACKGROUND_INTENSITY;
		else							// (default)
			wAttributes = wNormalAttributes;
	}

	// Though Windows' console supports COMMON_LVB_REVERSE_VIDEO,
	// it seems to be buggy.  So we must simulate it.
	if (wAttributes & COMMON_LVB_REVERSE_VIDEO)
		wAttributes = (wAttributes & COMMON_LVB_UNDERSCORE)
			| ((wAttributes & 0x00f0) >> 4) | ((wAttributes & 0x000f) << 4);
	SetConsoleTextAttribute(ghstdout, wAttributes);
}
void StartHighlight(void)
{
}
void StopHighlight(void)
{
}
