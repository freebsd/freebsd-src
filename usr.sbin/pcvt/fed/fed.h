/*
 * Copyright (c) 1992, 2000 Hellmuth Michaelis
 *
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * 	last edit-date: [Mon Mar 27 16:37:27 2000]
 *
 * $FreeBSD$
 */

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef FED

int ch_height;
int ch_width;

int curchar;

WINDOW *ch_win;
WINDOW *set_win;
WINDOW *cmd_win;

#else

extern int ch_height;		/* current fontfile character dimensions */
extern int ch_width;

extern int curchar;		/* character being edited */

extern WINDOW *ch_win;		/* windows */
extern WINDOW *set_win;
extern WINDOW *cmd_win;

#endif

#define FONTCHARS	256	/* no of chars in a fontfile */

#define WHITE ('.')
#define BLACK ('*')

#define K_UP	0x10	/* ^P */
#define K_DOWN	0x0e	/* ^N */
#define K_RIGHT	0x06	/* ^F */
#define K_LEFT	0x02	/* ^B */

#define WINROW	3
#define CMDCOL	3
#define CHCOL	20
#define SETCOL	41
#define WSIZE	16
#define CMDSIZE	12
#define WBORDER	1

/* fonts */

#define WIDTH8		8	/* 8 bits wide font		      */
#define WIDTH16		16	/* 16 bits wide font		      */

#define FONT8X8		2048	/* filesize for 8x8 font              */
#define HEIGHT8X8	8	/* 8 scan lines char cell height      */

#define FONT8X10	2560	/* filesize for 8x10 font             */
#define HEIGHT8X10	10	/* 10 scan lines char cell height     */

#define FONT8X14	3584	/* filesize for 8x14 font             */
#define HEIGHT8X14	14	/* 14 scan lines char cell height     */
#define WIDTH8X14	8	/* 8 bits wide font		      */

#define FONT8X16	4096	/* filesize for 8x16 font             */
#define HEIGHT8X16	16	/* 16 scan lines char cell height     */

#define FONT16X16	8192	/* filesize for 16x16 font            */
#define HEIGHT16X16	16	/* 16 scan lines char cell height     */


void edit_mode ( void );
int edit ( void );
void normal_ch ( int r, int c );
void chg_pt ( int r, int c );
void invert ( void );
void setchr ( char type );
void setrow ( char type );
void setcol ( char type );
void main ( int argc, char *argv[] );
void readfont ( char *filename );
void dis_cmd ( char *strg );
void clr_cmd ( void );
void save_ch ( void );
void move_ch ( int src, int dest );
void xchg_ch ( int src, int dest );
void display ( int no );
void sel_mode ( void );
int selectc ( void );
void normal_set ( int r, int c );
int sel_dest ( void );
void normal_uset ( int r, int c );
void writefont( void );

/* ------------------------------ EOF ----------------------------------- */
