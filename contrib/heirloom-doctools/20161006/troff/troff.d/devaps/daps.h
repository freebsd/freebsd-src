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
/*
 * Copyright 1989 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*	from OpenSolaris "daps.h	1.5	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)daps.h	1.3 (gritter) 8/9/05
 */

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */




	/********************************************************************
	 *																	*
	 *		The following defined constants and macros must be properly	*
	 *	set up for your APS-5 photo-typesetter.							*
	 *																	*
	 ********************************************************************/



#define	ADJUST			1			/* conditional compilation flag */

#define	LENS3			1			/* used for conditional compilation */
#define	MAX_RANGE		3			/* largest master range on this APS-5 */



#ifdef LENS1						/* standard 2.22X lens */

#define	RANGE1_SIZE		10			/* used in macro BASE_SIZE */
#define	SCALEFACTOR		1.25		/* can scale lower ranges up this far */


#define	BASE_SIZE(Range)			/* base size for master range Range */	\
																			\
		(RANGE1_SIZE * (1 << (Range - 1)))


#define	SCALE_UP(Bsize)				/* max size for lower ranges */			\
																			\
		(Bsize * SCALEFACTOR)

#endif



#ifdef LENS2						/* standard 2.667X lens */

#define	RANGE1_SIZE		12			/* master range 1 base size */
#define	SCALEFACTOR		1.25		/* can scale lower ranges up this far */


#define	BASE_SIZE(Range)			/* base size for master range Range */	\
																			\
		(RANGE1_SIZE * (1 << (Range - 1)))


#define	SCALE_UP(Bsize)				/* max size for lower ranges */			\
																			\
		(Bsize * SCALEFACTOR)

#endif


#ifdef LENS3						/* 2.667X lens - special case */

#define	RANGE1_SIZE		12			/* master range 1 base size */
#define	SCALEFACTOR		1.50		/* can scale lower ranges up this far */


#define	BASE_SIZE(Range)			/* base size for master range Range */	\
																			\
		(RANGE1_SIZE * (1 << (Range - 1)))


#define	SCALE_UP(Bsize)				/* max size for lower ranges */			\
																			\
		(Bsize * SCALEFACTOR - 1)

#endif




	/********************************************************************
	 *																	*
	 *				POST-PROCESSOR DEFINED CONSTANTS					*
	 *																	*
	 ********************************************************************/



#define	NON_FATAL		0				/* don't abort the job */
#define	FATAL			1				/* fatal error - abort the job */
#define	OFF				0				/* debug mode is off */
#define ON				1				/* debug mode is on */
#define	NO				0				/* don't ignore fatal errors - debug */
#define	YES				1				/* ignore fatal errors - debug only! */
#define	SAME_STR		0				/* strings are the same */

#define	RES				723.0			/* use this constant for accounting! */
#define	PAGE_LENGTH		11.0			/* inches per page for accounting */

#define	CUTOFF			10.0			/* beam cutoff position (inches) */
#define	HCUTOFF			7230			/* beam cutoff in device units */


#define	BMASK			0377			/* used for character mask */

#define MAX_INT			30000			/* used in t_reset() */
#define	MAX_DEBUG		20				/* number of debug states */

#define	MAX_OUTLIST		30				/* number of page pairs olist array */

#define	NAME_LENGTH		10				/* max length of typesetter name */

#define	FSIZE			200				/* size of a physical font */

#define	SLOP			2				/* error factor used in hflush() */

#define	NFONT			10				/* number of font positions available */

#define	POS_SLANT		14				/* angle used for positive slants */
#define	NEG_SLANT		-14				/* angle used for negative slants */

#define	TWO_BITS		3				/* mask for two rightmost bits */

#define	BIT0			1				/* mask for bit 0 */
#define	BIT1			2				/* mask for bit 1 */
#define	BIT2			4				/* mask for bit 2 */
#define	BIT3			8				/* mask for bit 3 */

#define	SLANT_BIT		BIT0			/* slant bit is bit 0 */
#define	FONT_BIT		BIT1			/* alternate font is bit 1 */
#define	RANGE_BIT		BIT2			/* max range is bit 2 */

#define	SLANT_VAL		3				/* slant angle starts in bit 3 */
#define	RANGE_VAL		5				/* range value starts in bit 5 */

#define	DO_ACCT			BIT0			/* still have to do accounting if on */
#define	FILE_STARTED	BIT1			/* file started but not completed */
#define	NO_OUTFILE		BIT2			/* no output file if on */
#define	NO_ACCTFILE		BIT3			/* no accounting file if on */

#define	BAN_LENGTH		130				/* max length of banner string */
#define	BAN_SIZE		10				/* point size to use in banner */

#define	VSPACE0			6				/* space down for first cut marks */
#define	VSPACE1			180				/* space at start of banner */
#define	VSPACE2			180				/* space before printing user string */
#define	VSPACE3			180				/* space before printing separator */
#define	VSPACE4			180				/* space before starting user job */

#define	HSPACE0			5346			/* space right for second cut marks */
#define	HSPACE1			50				/* indent before first separator */
#define	HSPACE2			100				/* indent before user string */
#define	HSPACE3			50				/* indent before second separator */
#define	HSPACE4			0				/* start cut marks at this position */




	/********************************************************************
	 *																	*
	 *						POST-PROCESSOR MACROS						*
	 *																	*
	 ********************************************************************/




#define	SET_ARGS(save)			/* set up internal argc and argv */			\
																			\
		save = argc;			/* save current value of argc */			\
		argc -= arg_index;		/* count arguments read already */			\
		argv += arg_index




#define	COUNT_ARGS(save)		/* count the arguments processed */			\
																			\
		arg_index = save - argc		/* arg_index counts the arguments */




#define	STR_CONVERT(s,i)		/* convert string s to integer i */			\
																			\
		do  {																\
			i = 0;															\
			do																\
				i = 10 * i + *s++ - '0';									\
			while ( isdigit(*s) );											\
		}	while ( 0 )	




#define	TOGGLE(a)				/* toggle the value in argument a */		\
																			\
		a = (a + 1) % 2




#define GET_DIG(fp,c)			/* get a single ASCII digit */				\
																			\
		c = getc(fp);														\
		if ( !isdigit(c)  ||  c == EOF )	/* illegal character */			\
			error(FATAL, "internal error - digit not found in GET_DIG")




#define GET_CHAR(fp,c)			/* get a single ASCII character */			\
																			\
		if ( (c = getc(fp)) == EOF )		/* end of file - abort */		\
			error(FATAL, "internal error - char not found in GET_CHAR")




#define GET_INT(fp,n)			/* get an integer from file fp */			\
																			\
		if ( fscanf(fp, "%d", &n) != 1 )	/* end of file - abort */		\
			error(FATAL,"internal error - integer not found in GET_INT")




#define GET_STR(fp,str)			/* get a string from file fp */				\
																			\
		if ( fscanf(fp, "%s", str) != 1 )		/* end of file - abort */	\
			error(FATAL, "internal error - string not found in GET_STR")




#define GET_LINE(fp,buf)		/* read a line from file fp into buf */		\
																			\
		if ( fgets(buf, sizeof(buf), fp) == NULL )							\
			error(FATAL, "internal error - line not found in GET_LINE")




#define SKIP_LINE(fp,c)			/* skip to the next line of the file fp */	\
																			\
		while ( ((c = getc(fp)) != '\n')  &&  c != EOF )					\
			;




#define SCAN1(buf,n1)			/* read one number from buf */				\
																			\
		if ( sscanf(buf, "%d", &n1) != 1 )									\
			error(FATAL, "internal error - integer not found in SCAN1")




#define SCAN2(buf,n1,n2)		/* read two numbers from buf */				\
																			\
		if ( sscanf(buf, "%d %d", &n1, &n2) != 2 )							\
			error(FATAL, "internal error - integers not found in SCAN2")




#define SCAN3(buf,n1,n2,n3)		/* read three numbers from buf */			\
																			\
		if ( sscanf(buf, "%d %d %d", &n1, &n2, &n3) != 3 )					\
			error(FATAL, "internal error - integers not found in SCAN3")




#define SCAN4(buf,n1,n2,n3,n4)	/* read four numbers from buf */			\
																			\
		if ( sscanf(buf, "%d %d %d %d", &n1, &n2, &n3, &n4) != 4 )			\
			error(FATAL, "internal error - integers not found in SCAN4")




#define	SCAN_STR(buf,str)		/* read a string from buf */				\
																			\
		sscanf(buf, "%s", str)




#ifdef pdp11					/* sign extension problems on pdp11s */

#define	PUTC(ch, fp)			/* don't check for error if ch = 0377 */	\
																			\
		if ( (putc(ch, fp) == EOF) && ((ch & BMASK) != 0377) )				\
			error(FATAL, "internal error - PUTC can't output character")

#else							/* don't worry about char 0377 */

#define	PUTC(ch, fp)			/* write character ch to file fp */			\
																			\
        if ( putc(ch, fp) == EOF )      /* error in writing to fp */        \
            error(FATAL, "internal error - PUTC can't output character")

#endif




#define	READ(fd, buf, n)		/* read n chars from file fd into buf */	\
																			\
		if ( read(fd, buf, n) < 0 )		/* error in reading from fd */		\
			error(FATAL, "internal error - READ can't read input file")




#define	DECODE(value, start, bitmask)										\
																			\
		(value >> start) & bitmask							




#define	CHANGE_FONT(newfont, oldrange)		/* set font to newfont */		\
																			\
		do {																\
			change_font(newfont);											\
			if ( oldrange != range )  {		/* range has changed */			\
				PUTC(HVSIZE, tf);			/* so set the point size */		\
				putint(10*pstab[size-1]);	/* to current size */			\
				oldrange = range;											\
			}	/* End if */												\
		}	while ( 0 )




#define	SETSLANT(code, angle)	/* set slant to value encoded in code */	\
																			\
		do {																\
			angle = DECODE(code, SLANT_VAL, TWO_BITS);						\
			if ( angle & BIT0 )			/* have positive APS-5 slant */		\
				angle = POS_SLANT;											\
			else if ( angle & BIT1 )	/* have negative slant */			\
				angle = NEG_SLANT;											\
			t_slant(angle + last_req_slant);	/* set the correct slant */	\
		} while ( 0 )




/* build.c */
int newfile(char *, int);
FILE *charfile(char *, int);
void save_env(void);
void restore_env(void);
void nconv(FILE *);
/* daps.c */
void get_options(int, char *[]);
void process_input(int, char *[]);
void init_signals(void);
void debug_select(char *);
void debug_file(char *);
void log_file(char *);
void acct_file(void);
void ban_file(char *);
void out_file(void);
void outlist(char *);
void error(int, char *, ...);
int done(void);
void float_err(int);
void wrap_up(int);
void conv(register FILE *);
void drawfunct(char [], FILE *);
void devcntrl(FILE *);
void t_init(void);
void t_banner(void);
void t_page(int);
void t_newline(void);
int t_size(int);
void t_charht(int);
int upper_limit(int);
void t_slant(int);
int t_font(char *);
void t_text(char *);
void t_reset(int);
void hflush(void);
void hmot(int);
void hgoto(int);
void vgoto(int);
void vmot(int);
void put1s(char *);
void t_adjust(char *);
void put1(int);
void putint(int);
void setsize(int);
void setfont(int);
void change_font(int);
void t_fp(int, char *, char *);
void account(void);
int special_case(int, int);
int get_range(int);
void fileinit(void);
void fontprint(int);
void loadfont(int, char *, char *);
void load_alt(int);
/* ../draw.c */
void drawline(int, int, char *);
void drawwig(char *);
char *getstr(char *, char *);
void drawcirc(int);
int dist(int, int, int, int);
void drawarc(int, int, int, int);
void drawellip(int, int);
void conicarc(int, int, int, int, int, int, int, int);
void putdot(int, int);
