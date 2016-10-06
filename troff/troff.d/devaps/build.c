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


/*	from OpenSolaris "build.c	1.5	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)build.c	1.4 (gritter) 8/13/05
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

#include <stdio.h>

#include "daps.h"

#define	LIBDIR	"charlib"				/* files found in *fontdir/LIBDIR */


/*
 *
 * These variables are used to save and later restore the post-processor's
 * environment.
 *
 */


int		ohpos;
int		ovpos;
int		ofont;
int		osize;

int		env = 0;						/* nonzero if environment is saved */


extern int		hpos;
extern int		vpos;
extern int		font;
extern int		size;

extern char		*fontdir;
#define	devname		troff_devname
extern char		devname[];
extern FILE		*tf;

#define oput(n)	putc(n, tf)


/*****************************************************************************/


int
newfile(


	char	*name,						/* start reading from this file */
	int		size						/* may use it to get the file */
)

{


	FILE	*fp;


	/********************************************************************
	 *																	*
	 *		Used when we want to take the post-processor's input from	*
	 *	a different file for a while. Added to handle problems with the	*
	 *	new logos, but it could easily be used to define other special	*
	 *	characters.														*
	 *																	*
	 ********************************************************************/


	if ( env || (fp = charfile(name, size)) == NULL )
		return(1);

	save_env();
	nconv(fp);
	restore_env();
	fclose(fp);

	return(0);

}	/* End of newfile */


/*****************************************************************************/


FILE *charfile(


	char	*name,						/* start reading from this file */
	int		size						/* size of the character to print */
)


{


	char	path[100];					/* file pathname put here */
	FILE	*fp;						/* file pointer for *path */


	/********************************************************************
	 *																	*
	 *		First tries to open file *name.size in the right directory,	*
	 *	and if it can't then it tries *name. Returns the file pointer	*
	 *	or NULL if either file can't be opened.							*
	 *																	*
	 ********************************************************************/


	sprintf(path, "%s/dev%s/%s/%s.%d", fontdir, devname, LIBDIR, name, size);

	if ( (fp = fopen(path, "r")) == NULL )  {
		sprintf(path, "%s/dev%s/%s/%s", fontdir, devname, LIBDIR, name);
		fp = fopen(path, "r");
	}	/* End if */

	return(fp);

}	/* End of charfile */


/*****************************************************************************/

void
save_env(void)


{


	/********************************************************************
	 *																	*
	 *		Before we start reading from a different file we'll want	*
	 *	to save the values of the variables that will be needed to get	*
	 *	back to where we were.											*
	 *																	*
	 ********************************************************************/


	hflush();

	ohpos = hpos;
	ovpos = vpos;
	ofont = font;
	osize = size;

	env = 1;

}	/* End of save_env */


/*****************************************************************************/


void
restore_env(void)


{


	/********************************************************************
	 *																	*
	 *		Hopefully does everything needed to get the post-processor	*
	 *	back to where it was before the input was diverted.				*
	 *																	*
	 ********************************************************************/


	hgoto(ohpos);
	vgoto(ovpos);

	setfont(ofont);
	setsize(osize);

	env = 0;

}	/* End of restore_env */


/*****************************************************************************/


void
nconv(


	FILE	*fp						/* new file we're reading from */
)


{


	int		ch;							/* first character of the command */
	int		c, n;						/* used in reading chars from *fp */
	char	str[100];					/* don't really need this much room */


	/********************************************************************
	 *																	*
	 *		A restricted and slightly modified version of the conv()	*
	 *	routine found in all of troff's post-processors. It should only	*
	 *	be used to interpret the special character building files found	*
	 *	in directory *fontdir/LIBDIR.									*
	 *																	*
	 ********************************************************************/


	while ( (ch = getc(fp)) != EOF )  {

		switch ( ch )  {

			case 'w':					/* just ignore these guys */
			case ' ':
			case '\n':
			case 0:
					break;

			case 'c':					/* single ASCII character */
					put1(getc(fp));
					break;

			case 'C':					/* special character */
					fscanf(fp, "%s", str);
					put1s(str);
					break;

			case 'h':					/* relative horizontal motion */
					fscanf(fp, "%d", &n);
					hmot(n);
					break;

			case 'v':
					fscanf(fp, "%d", &n);
					vmot(n);
					break;

			case 'x':					/* device control - font change only */
					fscanf(fp, "%s", str);
					if ( str[0] == 'f' )  {
						fscanf(fp, "%d %s", &n, str);
						loadfont(n, str, "");
					}	/* End if */
					break;

			case 's':					/* set point size */
					fscanf(fp, "%d", &n);
					setsize(t_size(n));
					break;

			case 'f':					/* use this font */
					fscanf(fp, "%s", str);
					setfont(t_font(str));
					break;

			case 'b':
					fscanf(fp, "%d", &n);
					oput(n);
					break;

			case 'W':
					fscanf(fp, "%d", &n);
					putint(n);
					break;


			case '#':					/* comment */
					while ( (c = getc(fp)) != '\n'  &&  c != EOF ) ;
					break;

		}	/* End switch */

	}	/* End while */

}	/* End of nconv */


/*****************************************************************************/


