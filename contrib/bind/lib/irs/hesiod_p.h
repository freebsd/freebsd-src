/*
 * Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * This file is primarily maintained by <tytso@mit.edu> and <ghudson@mit.edu>.
 */

/*
 * $Id: hesiod_p.h,v 1.6 1996/11/18 09:09:32 vixie Exp $
 */

/*
 * hesiod_p.h -- private definitions for the hesiod library
 */

#ifndef _HESIOD_P_H_INCLUDED
#define _HESIOD_P_H_INCLUDED

#define DEF_RHS		".Athena.MIT.EDU"	/* Defaults if HESIOD_CONF */
#define DEF_LHS		".ns"			/*    file is not */
						/*    present. */
struct hesiod_p {
	char *		LHS;		/* normally ".ns" */
	char *		RHS;		/* AKA the default hesiod domain */
};

#define MAX_HESRESP	1024

#endif /*_HESIOD_P_H_INCLUDED*/
