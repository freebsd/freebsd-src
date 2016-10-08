/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*	from OpenSolaris "gen.h	1.5	05/06/08 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)gen.h	1.16 (gritter) 10/15/06
 */

/*
 *
 * A few definitions that shouldn't have to change. They're used by most of the
 * programs in this package.
 *
 */


extern const char	creator[];


#define NON_FATAL	0
#define FATAL		1
#define USER_FATAL	2

#define OFF		0
#define ON		1

#define FALSE		0
#define TRUE		1

#define BYTE		8
#define BMASK		0377

#define POINTS		72.3

#ifndef PI
#define PI		3.141592654
#endif


/*
 *
 * A few simple macros.
 *
 */


#define ABS(A)		((A) >= 0 ? (A) : -(A))
#undef	MIN
#define MIN(A, B)	((A) < (B) ? (A) : (B))
#undef	MAX
#define MAX(A, B)	((A) > (B) ? (A) : (B))
 
/* color.c */
void getcolor(void);
void newcolor(char *);
void setcolor(void);
/* dpost.c */
void init_signals(void);
void header(FILE *);
void options(void);
void setpaths(char *);
void setup(void);
void arguments(void);
void done(void);
void account(void);
void conv(register FILE *);
void devcntrl(FILE *);
void fontinit(void);
void loadfont(int, char *, char *, int, int);
void loadspecial(void);
void loaddefault(void);
void fontprint(int);
char *mapfont(char *);
void getdevmap(void);
char *mapdevfont(char *);
void reset(void);
void resetpos(void);
void t_init(void);
void t_page(int);
void t_newline(void);
int t_size(int);
void setsize(int, float);
void t_fp(int, char *, char *, void *);
int t_font(char *);
void setfont(int);
void t_sf(int);
void t_charht(int, float);
void t_slant(int);
void needresource(const char *, ...);
void t_supply(char *);
void t_reset(int);
void t_trailer(void);
void hgoto(int);
void hmot(int);
void vgoto(int);
void vmot(int);
void xymove(int, int);
void put1s(register char *);
void put1(register int);
void oput(int);
void starttext(void);
void endtext(void);
void endstring(void);
void endline(void);
void addchar(int);
void addoctal(int);
void charlib(int);
int doglobal(char *);
void documentfont(const char *);
void documentfonts(void);
void redirect(int);
/* draw.c */
void getdraw(void);
void drawline(int, int);
void drawcirc(int, int);
void drawellip(int, int, int);
void drawarc(int, int, int, int, int);
void drawspline(FILE *, int);
void beginpath(char *, int);
void drawpath(char *, int);
void parsebuf(char *);
void getbaseline(void);
void newbaseline(char *);
void drawtext(char *);
void settext(char *);
/* glob.c */
/* misc.c */
void error(int, char *, ...);
void out_list(char *);
int in_olist(int);
int cat(char *, FILE *);
int str_convert(char **, int);
char *tempname(const char *);
int psskip(size_t, FILE *);
char *psgetline(char **, size_t *, size_t *, FILE *);
int sget(char *, size_t, FILE *);
/* pictures.c */
void picture(char *);
FILE *picopen(char *);
void inlinepic(FILE *, char *);
void piccopy(FILE *, FILE *, long);
/* ps_include.c */
void ps_include(const char *, FILE *, FILE *, int, int, int, int,
		double, double, double, double, double, double, double);
/* request.c */
void saverequest(char *);
void writerequest(int, FILE *);
void dumprequest(char *, char *, FILE *);
/* tempnam.c */
