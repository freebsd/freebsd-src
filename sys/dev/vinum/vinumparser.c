/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Nan Yang Computer
 *      Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *
 * $Id: vinumparser.c,v 1.21 2000/12/20 03:44:13 grog Exp grog $
 * $FreeBSD$
 */

/*
 * This file contains the parser for the configuration routines.  It's used
 * both in the kernel and in the user interface program, thus the separate file.
 */

/*
 * Go through a text and split up into text tokens.  These are either non-blank
 * sequences, or any sequence (except \0) enclosed in ' or ".  Embedded ' or
 * " characters may be escaped by \, which otherwise has no special meaning.
 *
 * Delimit by following with a \0, and return pointers to the starts at token [].
 * Return the number of tokens found as the return value.
 *
 * This method has the restriction that a closing " or ' must be followed by
 * grey space.
 *
 * Error conditions are end of line before end of quote, or no space after
 * a closing quote.  In this case, tokenize() returns -1.
 */

#include <sys/param.h>
#include <dev/vinum/vinumkw.h>
#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/conf.h>
#include <machine/setjmp.h>
/* All this mess for a single struct definition */
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/disklabel.h>
#include <sys/mount.h>

#include <dev/vinum/vinumvar.h>
#include <dev/vinum/vinumio.h>
#include <dev/vinum/vinumext.h>
#define iswhite(c) ((c == ' ') || (c == '\t'))		    /* check for white space */
#else /* userland */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#define iswhite isspace					    /* use the ctype macro */
#endif

/* enum keyword is defined in vinumvar.h */

#define keypair(x) { #x, kw_##x }			    /* create pair "foo", kw_foo */
#define flagkeypair(x) { "-"#x, kw_##x }		    /* create pair "-foo", kw_foo */
#define KEYWORDSET(x) {sizeof (x) / sizeof (struct _keywords), x}

/* Normal keywords.  These are all the words that vinum knows. */
struct _keywords keywords[] =
{keypair(drive),
    keypair(partition),
    keypair(sd),
    keypair(subdisk),
    keypair(plex),
    keypair(volume),
    keypair(vol),
    keypair(setupstate),
    keypair(readpol),
    keypair(org),
    keypair(name),
    keypair(writethrough),
    keypair(writeback),
    keypair(raw),
    keypair(device),
    keypair(concat),
    keypair(raid4),
    keypair(raid5),
    keypair(striped),
    keypair(plexoffset),
    keypair(driveoffset),
    keypair(length),
    keypair(len),
    keypair(size),
    keypair(state),
    keypair(round),
    keypair(prefer),
    keypair(rename),
    keypair(detached),
#ifndef _KERNEL						    /* for vinum(8) only */
#ifdef VINUMDEBUG
    keypair(debug),
#endif
    keypair(stripe),
    keypair(mirror),
#endif
    keypair(attach),
    keypair(detach),
    keypair(printconfig),
    keypair(saveconfig),
    keypair(replace),
    keypair(create),
    keypair(read),
    keypair(modify),
    keypair(list),
    keypair(l),
    keypair(ld),
    keypair(ls),
    keypair(lp),
    keypair(lv),
    keypair(info),
    keypair(set),
    keypair(rm),
    keypair(mv),
    keypair(move),
    keypair(init),
    keypair(label),
    keypair(resetconfig),
    keypair(start),
    keypair(stop),
    keypair(makedev),
    keypair(help),
    keypair(quit),
    keypair(setdaemon),
    keypair(getdaemon),
    keypair(max),
    keypair(replace),
    keypair(readpol),
    keypair(resetstats),
    keypair(setstate),
    keypair(checkparity),
    keypair(rebuildparity),
    keypair(dumpconfig),
    keypair(retryerrors)
};
struct keywordset keyword_set = KEYWORDSET(keywords);

#ifndef _KERNEL
struct _keywords flag_keywords[] =
{flagkeypair(f),
    flagkeypair(d),
    flagkeypair(v),
    flagkeypair(s),
    flagkeypair(r),
    flagkeypair(w)
};
struct keywordset flag_set = KEYWORDSET(flag_keywords);

#endif

/*
 * Take a blank separated list of tokens and turn it into a list of
 * individual nul-delimited strings.  Build a list of pointers at
 * token, which must have enough space for the tokens.  Return the
 * number of tokens, or -1 on error (typically a missing string
 * delimiter).
 */
int
tokenize(char *cptr, char *token[], int maxtoken)
{
    char delim;						    /* delimiter for searching for the partner */
    int tokennr;					    /* index of this token */

    for (tokennr = 0; tokennr < maxtoken;) {
	while (iswhite(*cptr))
	    cptr++;					    /* skip initial white space */
	if ((*cptr == '\0') || (*cptr == '\n') || (*cptr == '#')) /* end of line */
	    return tokennr;				    /* return number of tokens found */
	delim = *cptr;
	token[tokennr] = cptr;				    /* point to it */
	tokennr++;					    /* one more */
	if (tokennr == maxtoken)			    /* run off the end? */
	    return tokennr;
	if ((delim == '\'') || (delim == '"')) {	    /* delimitered */
	    for (;;) {
		cptr++;
		if ((*cptr == delim) && (cptr[-1] != '\\')) { /* found the partner */
		    cptr++;				    /* move on past */
		    if (!iswhite(*cptr))		    /* error, no space after closing quote */
			return -1;
		    *cptr++ = '\0';			    /* delimit */
		} else if ((*cptr == '\0') || (*cptr == '\n')) /* end of line */
		    return -1;
	    }
	} else {					    /* not quoted */
	    while ((*cptr != '\0') && (!iswhite(*cptr)) && (*cptr != '\n'))
		cptr++;
	    if (*cptr != '\0')				    /* not end of the line, */
		*cptr++ = '\0';				    /* delimit and move to the next */
	}
    }
    return maxtoken;					    /* can't get here */
}

/* Find a keyword and return an index */
enum keyword
get_keyword(char *name, struct keywordset *keywordset)
{
    int i;
    struct _keywords *keywords = keywordset->k;		    /* point to the keywords */
    if (name != NULL) {					    /* parameter exists */
	for (i = 0; i < keywordset->size; i++)
	    if (!strcmp(name, keywords[i].name))
		return (enum keyword) keywords[i].keyword;
    }
    return kw_invalid_keyword;
}
