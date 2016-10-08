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
 * Copyright 1993 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*	from OpenSolaris "pictures.c	1.7	05/06/08 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)pictures.c	1.7 (gritter) 3/27/07
 */

/*
 *
 * PostScript picture inclusion routines. Support for managing in-line pictures
 * has been added, and works in combination with the simple picpack pre-processor
 * that's supplied with this package. An in-line picture begins with a special
 * device control command that looks like,
 *
 *		x X InlinPicture name size
 *
 * where name is the pathname of the original picture file and size is the number
 * of bytes in the picture, which begins immediately on the next line. When dpost
 * encounters the InlinePicture device control command inlinepic() is called and
 * that routine appends the string name and the integer size to a temporary file
 * (fp_pic) and then adds the next size bytes read from the current input file to
 * file fp_pic. All in-line pictures are saved in fp_pic and located later using
 * the name string and picture file size that separate pictures saved in fp_pic.
 *
 * When a picture request (ie. an "x X PI" command) is encountered picopen() is
 * called and it first looks for the picture file in fp_pic. If it's found there
 * the entire picture (ie. size bytes) is copied from fp_pic to a new temp file
 * and that temp file is used as the picture file. If there's nothing in fp_pic
 * or if the lookup failed the original route is taken.
 *
 * Support for in-line pictures is an attempt to address requirements, expressed
 * by several orginazations, of being able to store a document as a single file
 * (usually troff input) that can then be sent through dpost and ultimately to
 * a PostScript printer. The mechanism may help some users, but the are obvious
 * disadvantages to this approach, and the original mechanism is the recommended
 * approach! Perhaps the most important problem is that troff output, with in-line
 * pictures included, doesn't fit the device independent language accepted by
 * important post-processors (like proff) and that means you won't be able to
 * reliably preview a packed file on your 5620 (or whatever).
 *
 */


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "comments.h"			/* PostScript file structuring comments */
#include "gen.h"			/* general purpose definitions */
#include "path.h"			/* just for TEMPDIR definition */


static FILE	*fp_pic = NULL;			/* in-line pictures go here */

extern int	res, hpos, vpos;
extern int	picflag;
extern FILE	*tf;


/*****************************************************************************/


void
picture (
    char *buf		/* stuff following 'x X PI' command */
)


{


    int		poffset;	/* page offset */
    int		indent;		/* indent */
    int		length;		/* line length  */
    int		totrap;		/* distance to next trap */
    char	name[4096];	/* picture file and page string */
    char	hwo[4096], *p;	/* height, width and offset strings */
    char	flags[4096];	/* miscellaneous stuff */
    int		page = 1;	/* page number pulled from name[] */
    double	frame[4];	/* height, width, y, and x offsets from hwo[] */
    char	units;		/* scale indicator for frame dimensions */
    int		whiteout = 0;	/* white out the box? */
    int		outline = 0;	/* draw a box around the picture? */
    int		scaleboth = 0;	/* scale both dimensions? */
    double	adjx = 0.5;	/* left-right adjustment */
    double	adjy = 0.5;	/* top-bottom adjustment */
    double	rot = 0;	/* rotation in clockwise degrees */
    FILE	*fp_in;		/* for *name */
    int		i;		/* loop index */


/*
 *
 * Called from devcntrl() after an 'x X PI' command is found. The syntax of that
 * command is:
 *
 *	x X PI:args
 *
 * with args separated by colons and given by:
 *
 *	poffset
 *	indent
 *	length
 *	totrap
 *	file[(page)]
 *	height[,width[,yoffset[,xoffset]]]
 *	[flags]
 *
 * poffset, indent, length, and totrap are given in machine units. height, width,
 * and offset refer to the picture frame in inches, unless they're followed by
 * the u scale indicator. flags is a string that provides a little bit of control
 * over the placement of the picture in the frame. Rotation of the picture, in
 * clockwise degrees, is set by the a flag. If it's not followed by an angle
 * the current rotation angle is incremented by 90 degrees, otherwise the angle
 * is set by the number that immediately follows the a.
 *
 */


    if ( picflag == OFF )		/* skip it */
	return;

    endtext();

    flags[0] = '\0';			/* just to be safe */
    if ( sscanf(buf, "%d:%d:%d:%d:%[^:]:%[^:]:%[^:]", &poffset, &indent,
		&length, &totrap, name, hwo, flags) < 6 )  {
	    error(NON_FATAL, "too few arguments to specify picture");
	    return;
    }	/* End if */

    if ( sscanf(name, "%*[^(](%d", &page) == 1 )	/* grab the page number */
	strtok(name, "(");			/* and separate it from the name */

    if ( (fp_in = picopen(name)) == NULL ) {
	error(NON_FATAL, "can't open picture file %s", name);
	return;
    }	/* End if */

    frame[0] = frame[1] = -1;		/* default frame height, width */
    frame[2] = frame[3] = 0;		/* and y and x offsets */

    for ( i = 0, p = hwo; i < 4 && p != NULL; i++, p = strchr(p, ',') ) {
    	if (i)
		p++;
	if ( sscanf(p, "%lf%c", &frame[i], &units) == 2 )
	    if ( units == 'i' || units == ',' || units == '\0' )
		frame[i] *= res;
    }

    if ( frame[0] <= 0 )		/* check what we got for height */
	frame[0] = totrap;

    if ( frame[1] <= 0 )		/* and width - check too big?? */
	frame[1] = length - indent;

    frame[3] += poffset + indent;	/* real x offset */

    for ( i = 0; flags[i]; i++ )
	switch ( flags[i] )  {
	    case 'c': adjx = adjy = 0.5; break;	/* move to the center */
	    case 'l': adjx = 0; break;		/* left */
	    case 'r': adjx = 1; break;		/* right */
	    case 't': adjy = 1; break;		/* top */
	    case 'b': adjy = 0; break;		/* or bottom justify */
	    case 'o': outline = 1; break;	/* outline the picture */
	    case 'w': whiteout = 1; break;	/* white out the box */
	    case 's': scaleboth = 1; break;	/* scale both dimensions */
	    case 'a': if ( sscanf(&flags[i+1], "%lf", &rot) != 1 )
			  rot += 90;
	}   /* End switch */

    fprintf(tf, "save mark\n");
    fprintf(tf, "[ /NamespacePush pdfmark\n");

    ps_include(name, fp_in, tf, page, whiteout, outline, scaleboth,
		frame[3]+frame[1]/2, -vpos-frame[2]-frame[0]/2, frame[1], frame[0], adjx, adjy, -rot);

    fprintf(tf, "[ /NamespacePop pdfmark\n");
    fprintf(tf, "cleartomark restore\n");
    xymove(hpos, vpos);
    t_sf(1);

    fclose(fp_in);

}   /* End of picture */


/*****************************************************************************/


FILE *picopen(


    char	*path			/* picture file pathname */
)


{


    char	name[4096];		/* pathnames */
    long	total;			/* and sizes - from *fp_pic */
    char	*tname;			/* pathname */
    FILE	*fp;			/* and pointer for the new temp file */


/*
 *
 * Responsible for finding and opening the next picture file. If we've accumulated
 * any in-line pictures fp_pic won't be NULL and we'll look there first. If *path
 * is found in *fp_pic we create another temp file, open it for update, unlink it,
 * copy in the picture, seek back to the start of the new temp file, and return
 * the file pointer to the caller. If fp_pic is NULL or the lookup fails we just
 * open file *path and return the resulting file pointer to the caller.
 *
 */


    if ( fp_pic != NULL )  {
	fseek(fp_pic, 0L, SEEK_SET);
	while ( fscanf(fp_pic, "%s %ld\n", name, &total) != EOF )  {
	    if ( strcmp(path, name) == 0 )  {
		if ( (tname = tempname("dpost")) == NULL )
		    error(FATAL, "can't generate temp file name");
		if ( (fp = fopen(tname, "w+")) == NULL )
		    error(FATAL, "can't open %s", tname);
		unlink(tname);
		free(tname);
		piccopy(fp_pic, fp, total);
		fseek(fp, 0L, SEEK_SET);
		return(fp);
	    }   /* End if */
	    fseek(fp_pic, total, SEEK_CUR);
	}   /* End while */
    }	/* End if */

    return(fopen(path, "r"));

}   /* End of picopen */


/*****************************************************************************/


void
inlinepic(


    FILE	*fp,			/* current input file */
    char	*buf			/* whatever followed "x X InlinePicture" */
)


{


    char	*tname;			/* temp file pathname - for *fp_pic */
    char	name[4096];		/* picture file pathname */
    long	total;			/* and size - both from *buf */


/*
 *
 * Adds an in-line picture file to the end of temporary file *fp_pic. All pictures
 * grabbed from the input file are saved in the same temp file. Each is preceeded
 * by a one line header that includes the original picture file pathname and the
 * size of the picture in bytes. The in-line picture file is opened for update,
 * left open, and unlinked so it disappears when we do.
 *
 */


    if ( fp_pic == NULL )  {
	if ( (tname = tempname("dpost")) == NULL )
	    error(FATAL, "can't generate in-line picture file name");
	if ( (fp_pic = fopen(tname, "w+")) == NULL )
	    error(FATAL, "can't open in-line picture file %s", tname);
	unlink(tname);
    }	/* End if */

    if ( sscanf(buf, "%s %ld", name, &total) != 2 )
	error(FATAL, "in-line picture error");

    fseek(fp_pic, 0L, SEEK_END);
    fprintf(fp_pic, "%s %ld\n", name, total);
    getc(fp);
    fflush(fp_pic);
    piccopy(fp, fp_pic, total);
    ungetc('\n', fp);

}   /* End of inlinepic */


/*****************************************************************************/


void
piccopy(


    FILE	*fp_in,			/* input */
    FILE	*fp_out,		/* and output file pointers */
    long	total 			/* number of bytes to be copied */
)


{


    long	i;			/* loop index */


/*
 *
 * Copies total bytes from file fp_in to fp_out. Used to append picture files to
 * *fp_pic and then copy them to yet another temporary file immediately before
 * they're used (in picture()).
 *
 */


    for ( i = 0; i < total; i++ )
	if ( putc(getc(fp_in), fp_out) == EOF )
	    error(FATAL, "error copying in-line picture file");
    fflush(fp_out);

}   /* End of piccopy */


/*****************************************************************************/

