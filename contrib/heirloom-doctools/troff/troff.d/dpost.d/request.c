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


/*	from OpenSolaris "request.c	1.5	05/06/08 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)request.c	1.3 (gritter) 8/9/05
 */

/*
 *
 * Things used to handle special requests (eg. manual feed) globally or on a per
 * page basis. Requests are passed through to the translator using the -R option.
 * The argument to -R can be "request", "request:page", or "request:page:file".
 * If page is omitted (as in the first form) or set to 0 request will be applied
 * to the global environment. In all other cases it applies only to the selected
 * page. If a file is given, page must be supplied, and the lookup is in that file
 * rather than *requestfile.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen.h"			/* general purpose definitions */
#include "request.h"			/* a few special definitions */
#include "path.h"			/* for the default request file */


Request	request[MAXREQUEST];		/* next page or global request */
int	nextreq = 0;			/* goes in request[nextreq] */
char	*requestfile = REQUESTFILE;	/* default lookup file */


/*****************************************************************************/


void
saverequest (
    char *want			/* grab code for this stuff */
)


{


    char	*page;			/* and save it for this page */


/*
 *
 * Save the request until we get to appropriate page - don't even bother with
 * the lookup right now. Format of *want string is "request", "request:page", or
 * "request:page:file", and we assume we can change the string here as needed.
 * If page is omitted or given as 0 the request will be done globally. If *want
 * includes a file, request and page must also be given, and in that case *file
 * will be used for the lookup.
 *
 */


    if ( nextreq < MAXREQUEST )  {
	request[nextreq].want = strtok(want, ": ");
	if ( (page = strtok(NULL, ": ")) == NULL )
	    request[nextreq].page = 0;
	else request[nextreq].page = atoi(page);
	if ( (request[nextreq].file = strtok(NULL, ": ")) == NULL )
	    request[nextreq].file = requestfile;
	nextreq++;
    } else error(NON_FATAL, "too many requests - ignoring %s", want);

}   /* End of saverequest */


/*****************************************************************************/


void
writerequest(


    int		page,			/* write everything for this page */
    FILE	*fp_out			/* to this file */
)


{

    int		i;			/* loop index */


/*
 *
 * Writes out all the requests that have been saved for page. Page 0 refers to
 * the global environment and is done during initial setup.
 *
 */


    for ( i = 0; i < nextreq; i++ )
	if ( request[i].page == page )
	    dumprequest(request[i].want, request[i].file, fp_out);

}   /* End of writerequest */


/*****************************************************************************/


void
dumprequest(


    char	*want,			/* look for this string */
    char	*file,			/* in this file */
    FILE	*fp_out			/* and write the value out here */
)


{


    char	buf[100];		/* line buffer for reading *file */
    FILE	*fp_in;


/*
 *
 * Looks for *want in the request file and if it's found the associated value
 * is copied to the output file. Keywords (ie. the *want strings) begin an @ in
 * the first column of file, while the values (ie. the stuff that's copied to
 * the output file) starts on the next line and extends to the next keyword or
 * to the end of file.
 *
 */


    if ( (fp_in = fopen(file, "r")) != NULL )  {
	while ( fgets(buf, sizeof(buf), fp_in) != NULL )
	    if ( buf[0] == '@' && strncmp(want, &buf[1], strlen(want)) == 0 )
		while ( fgets(buf, sizeof(buf), fp_in) != NULL )
		{
		    if ( buf[0] == '#' || buf[0] == '%' )
			continue;
		    else if ( buf[0] != '@' )
			fprintf(fp_out, "%s", buf);
		    else break;
		}
	fclose(fp_in);
    }	/* End if */

}   /* End of dumprequest */


/*****************************************************************************/

