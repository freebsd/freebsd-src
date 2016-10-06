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


/*	from OpenSolaris "request.h	1.5	05/06/08 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)request.h	1.3 (gritter) 8/9/05
 */

/*
 *
 * Things used to handle special PostScript requests (like manual feed) globally
 * or on a per page basis. All the translators I've supplied accept the -R option
 * that can be used to insert special PostScript code before the global setup is
 * done, or at the start of named pages. The argument to the -R option is a string
 * that can be "request", "request:page", or "request:page:file". If page isn't
 * given (as in the first form) or if it's 0 in the last two, the request applies
 * to the global environment, otherwise request holds only for the named page.
 * If a file name is given a page number must be supplied, and in that case the
 * request will be looked up in that file.
 *
 */

#define MAXREQUEST	30

typedef struct {

	char	*want;
	int	page;
	char	*file;

} Request;


