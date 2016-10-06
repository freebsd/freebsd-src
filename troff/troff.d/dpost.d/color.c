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


/*	from OpenSolaris "color.c	1.5	05/06/08 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)color.c	1.5 (gritter) 11/29/05
 */

/*
 *
 * Routines that handle color requests passed through as device control commands
 * in the form "x X SetColor:red". The following PostScript procedures are needed:
 *
 *	_setcolor
 *
 *	  mark /color _setcolor mark
 *	  mark /color1 /color2 _setcolor mark
 *
 *	    Called whenever we want to change PostScript's current color graphics
 *	    state parameter. One or two color arguments can be given. In each case
 *	    the colors are looked up in the PostScript colordict dictionary that's
 *	    defined in *colorfile. Two named colors implies reverse video printing
 *	    with the background given in /color2 and the text printed in /color1.
 *	    Unknown colors are mapped into defaults - black for a single color and
 *	    white on black for reverse video.
 *
 *	drawrvbox
 *
 *	  leftx rightx drawrvbox -
 *
 *	    Fills a box that extends from leftx to rightx with the background color
 *	    that was requested when _setcolor set things up for reverse video mode.
 *	    The vertical extent of the box is determined using FontBBox just before
 *	    the first string is printed, and the height remains in effect until
 *	    there's an explicit color change. In otherwords font or size changes
 *	    won't always produce correct result in reverse video mode.
 *
 *	setdecoding
 *
 *	  num setdecoding -
 *
 *	    Selects the text decoding procedure (ie. what's assigned to PostScript
 *	    procedure t) from the decodingdefs array defined in the prologue. num
 *	    should be the value assigned to variable encoding (in dpost) and will
 *	    remain constant throughout a job, unless special features, like reverse
 *	    video printing, are requested. The text encoding scheme can be set on
 *	    the command line using the -e option. Print time and the size of the
 *	    output file will usually decrease as the value assigned to encoding
 *	    increases.
 *
 *
 * The recognized collection of "x X SetColor:" commands are:
 *
 *	x X SetColor:				selects black
 *	x X SetColor:color			selects color
 *	x X SetColor:color1 on color2		reverse video
 *	x X SetColor:color1 color2		reverse video again
 *	x X SetColor:num1 num2 num3 rgb		explicit rgb color request
 *	x X SetColor:num1 num2 num3 hsb		explicit hsb color request
 *	x X SetColor:num1 num2 num3 num4 cmyk	explicit cmyk color request
 *	x X SetColor:arbitrary PostScript commands
 *
 * In the last examples num1, num2, num3, and num4 should be numbers between 0 and
 * 1 inclusive and are passed on as aguments to the approrpriate PostScript color
 * command (eg. setrgbcolor). Unknown color names (ie. the ones that _setcolor
 * doesn't find in colordict) are mapped into defaults. For one color the default
 * is black, while for reverse video it's white text on a black background.
 *
 * dpost makes sure the current color is maintained across page boundaries, which
 * may not be what you want if you're using a macro package like mm that puts out
 * page footers and headers. Adding a color request to troff and keeping track of
 * the color in each environment may be the best solution.
 *
 * To get reverse video printing follow the "x X SetColor:" command with two or
 * three arguments. "x X SetColor:white on black" or "x X SetColor:white black"
 * both produce white text on a black background. Any two colors named in colordict
 * (in file *colorfile) can be chosen so "x X SetColor:yellow on blue" also works.
 * Each reverse video mode request selects the vertical extent of the background
 * box based on the font and size in use just before the first string is printed.
 * Font and/or size changes aren't guaranteed to work properly in reverse video
 * printing.
 *
 */


#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>

#include "gen.h"			/* general purpose definitions */
#include "ext.h"			/* external variable definitions */
#include "global.h"		/* global heirloom doctools definitions */

#define DEFAULTCOLOR	"black"

static char	color[500] = DEFAULTCOLOR;	/* current color */
static int	gotcolor = FALSE;		/* TRUE after *colorfile is downloaded */
static int	wantcolor = FALSE;		/* TRUE if we really ask for a color */


/*
 *
 * All these should be defined in dpost.c.
 *
 */


extern int	lastend;
extern int	encoding;
extern int	maxencoding;
extern int	realencoding;

extern char	*colorfile;
extern FILE	*tf;


/*****************************************************************************/


void
getcolor(void)


{


/*
 *
 * Responsible for making sure the PostScript color procedures are downloaded from
 * *colorfile. Done at most once per job, and only if the job really uses color.
 * For now I've decided not to quit if we can't read the color file.
 *
 */


    if ( gotcolor == FALSE && access(colorfile, 04) == 0 )
	doglobal(colorfile);

    if ( tf == stdout )
	gotcolor = TRUE;

}   /* End of getcolor */


/*****************************************************************************/


void
newcolor (
    char *name			/* of the color */
)


{


    char	*p;			/* next character in *name */
    int		i;			/* goes in color[i] */


/*
 *
 * Converts *name to lower case and saves the result in color[] for use as the
 * current color. The first time something other than DEFAULTCOLOR is requested
 * sets wantcolor to TRUE. Characters are converted to lower case as they're put
 * in color[] and we quit when we find a newline or get to the end of *name. The
 * isupper() test is for Berkley systems.
 *
 */


    for ( p = name; *p && (*p == ' ' || *p == ':'); p++ ) ;

    for ( i = 0; i < sizeof(color) - 1 && *p != '\n' && *p; i++, p++ )
	if ( isupper((int)*p) )
	    color[i] = tolower((int)*p);
	else color[i] = *p;

    if ( i == 0 )
	n_strcpy(color, DEFAULTCOLOR, sizeof(color));
    else color[i] = '\0';

    if ( strcmp(color, DEFAULTCOLOR) != 0 )
	wantcolor = TRUE;

}   /* End of newcolor */


/*****************************************************************************/


void
setcolor(void)


{


    int		newencoding;		/* text encoding scheme that's needed */
    char	*p;			/* for converting what's in color[] */


/*
 *
 * Sets the color being used by the printer to whatever's stored as the current
 * color (ie. the string in color[]). wantcolor is only set to TRUE if we've been
 * through newcolor() and asked for something other than DEFAULTCOLOR (probably
 * black). While in reverse video mode encoding gets set to maxencoding + 1 in
 * dpost and 0 on the printer. Didn't see much point in trying to extend reverse
 * video to all the different encoding schemes. realencoding is restored when we
 * leave reverse video mode.
 *
 */


    if ( wantcolor == TRUE )  {
	endtext();
	getcolor();

	lastend = -1;
	newencoding = realencoding;

	if ( islower((int)color[0]) == 0 )		/* explicit rgb, hsb, or cmyk request */
	    fprintf(tf, "%s\n", color);
	else {
	    putc('/', tf);
	    for ( p = color; *p && *p != ' '; p++ )
		putc(*p, tf);
	    for ( ; *p && *p == ' '; p++ ) ;
	    if ( strncmp(p, "on ", 3) == 0 ) p += 3;
	    if ( *p != '\0' )  {
		fprintf(tf, " /%s", p);
		newencoding = maxencoding + 1;
	    }	/* End if */
	    fprintf(tf, " _setcolor\n");
	}   /* End else */

	if ( newencoding != encoding )  {
	    encoding = newencoding;
	    fprintf(tf, "%d setdecoding\n", encoding);
	    resetpos();
	}   /* End if */
    }	/* End if */

}   /* End of setcolor */


/*****************************************************************************/

