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


/*	from OpenSolaris "dpost.h	1.7	05/06/08 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)dpost.h	1.11 (gritter) 9/22/06
 */

/*
 *
 * Definitions used by the troff post-processor for PostScript printers.
 *
 * DEVNAME should be the name of a device whose font files accurately describe
 * what's available on the target printer. It's a string that's combined with
 * "/usr/lib/font/dev" to locate the final font directory. It can be changed
 * using the -T option, but you may end up getting garbage - the character code
 * field must agree with PostScript's character encoding scheme for each font and
 * troff's one or two character font names must be mapped into the appropriate
 * PostScript font names (typically in the prologue)
 *
 *
 */

#define	DEVNAME		"post"		/* name of the target printer */

/*
 *
 * NFONT is the most font positions we'll allow. It's set ridiculously high for no
 * good reason.
 *
 */

#define NFONT		300		/* max number of font positions */

/*
 *
 * SLOP controls how much horizontal positioning error we'll accept and primarily
 * helps when we're emulating another device. It's used when we output characters
 * in oput() to check if troff and the printer have gotten too far out of sync.
 * Given in units of points and can be changed using the -S option. Converted to
 * machine units in t_init() after the resolution is known.
 *
 */

#define SLOP		.2		/* horizontal error - in points */

/*
 *
 * Fonts are assigned unique internal numbers (positive integers) in their ASCII
 * font files. MAXINTERNAL is the largest internal font number that lets the host
 * resident and DOCUMENTFONTS stuff work. Used to allocate space for an array that
 * keeps track of what fonts we've seen and perhaps downloaded - could be better!
 *
 */

#define MAXINTERNAL	1536

/*
 *
 * Several different text line encoding schemes are supported. Print time should
 * decrease as the value assigned to encoding (in dpost.c) increases, although the
 * only encoding that's well tested is the lowest level one, which produces output
 * essentially identical to the original version of dpost. Setting DFLTENCODING to
 * 0 will give you the most stable (but slowest) encoding. The encoding scheme can
 * also be set on the command line using the -e option. Faster methods are based
 * on widthshow and may not place words exactly where troff wanted, but errors will
 * usually not be noticeable.
 *
 */

#define MAXENCODING	5

#ifndef DFLTENCODING
#define DFLTENCODING	0
#endif

/*
 *
 * The encoding scheme controls how lines of text are output. In the lower level
 * schemes words and horizontal positions are put on the stack as they're read and
 * when they're printed it's done in reverse order - the first string printed is
 * the one on top of the stack and it's the last one on the line. Faster methods
 * may be forced to reverse the order of strings on the stack, making the top one
 * the first string on the line. STRINGSPACE sets the size of a character array
 * that's used to save the strings that make up  a line of text so they can be
 * output in reverse order or perhaps combined in groups for widthshow.
 *
 * MAXSTACK controls how far we let PostScript's operand stack grow and determines
 * the number of strings we'll save before printing all or part of a line of text.
 * The internal limit in PostScript printers built by Adobe is 500, so MAXSTACK
 * should never be bigger than about 240!
 *
 * Line is a structure used to keep track of the words (or rather strings) on the
 * current line that have been read but not printed. dx is the width troff wants
 * to use for a space in the current string. start is where the string began, width
 * is the total width of the string, and spaces is the number of space characters
 * in the current string. *str points to the start of the string in the strings[]
 * array. The Line structure is only used in the higher level encoding schemes.
 * 
 */

#define	MAXSTACK	50		/* most strings we'll save at once */
#define	STRINGSPACE	2000		/* bytes available for string storage */

typedef struct {

	char	*str;			/* where the string is stored */
	int	dx;			/* width of a space */
	int	spaces;			/* number of space characters */
	int	start;			/* horizontal starting position */
	int	width;			/* and its total width */

} Line;

/*
 *
 * Simple stuff used to map unrecognized font names into something reasonable. The
 * mapping array is initialized using FONTMAP and used in loadfont() whenever the
 * job tries to use a font that we don't recognize. Normally only needed when we're
 * emulating another device.
 *
 */

typedef struct {

	char	*name;			/* font name we're looking for */
	char	*use;			/* and this is what we should use */

} Fontmap;

#define	FONTMAP			\
				\
	{			\
	    { "G" , "H"  },	\
	    { "LO", "S"  },	\
	    { "S2", "S"  },	\
	    { "GI", "HI" },	\
	    { "HM", "H"  },	\
	    { "HK", "H"  },	\
	    { "HL", "H"  },	\
	    { "PA", "R"  },	\
	    { "PI", "I"  },	\
	    { "PB", "B"  },	\
	    { "PX", "BI" },	\
	    { NULL, NULL }	\
	}

/*
 *
 * The Fontmap stuff isn't quite enough if we expect to do a good job emulating
 * other devices. A recognized font in *realdev's tables may be have a different
 * name in *devname's tables, and using the *realdev font may not be the best
 * choice. The fix is to use an optional lookup table for *devname that's used to
 * map font names into something else before anything else is done. The table we
 * use is /usr/lib/font/dev*realdev/fontmaps/devname and if it exists getdevmap()
 * uses the file to fill in a Devfontmap array. Then whenever an "x font pos name"
 * command is read mapdevfont() uses the lookup table to map name into something
 * else before loadfont() is called.
 *
 */

typedef struct {

	char	name[3];		/* map this font name */
	char	use[3];			/* into this one */

} Devfontmap;
