/*-
 * Copyright (c) 1998 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <err.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

static void
usage(void)
{
    fprintf(stderr, "usage: chkgrp [groupfile]\n");
    exit(EX_USAGE);
}

int
main(int argc, char *argv[])
{
    unsigned int i, len;
    int n = 0, k, e = 0;
    char *line, *f[4], *p;
    const char *gfn;
    FILE *gf;

    /* check arguments */
    switch (argc) {
    case 1:
	gfn = "/etc/group";
	break;
    case 2:
	gfn = argv[1];
	break;
    default:
	gfn = NULL; /* silence compiler */
	usage();
    }

    /* open group file */
    if ((gf = fopen(gfn, "r")) == NULL)
	err(EX_IOERR, "%s", gfn); /* XXX - is IO_ERR the correct exit code? */

    /* check line by line */
    while (++n) {
	if ((line = fgetln(gf, &len)) == NULL)
	    break;
	while (len && isspace(line[len-1]))
	    len--;

	/* ignore blank lines and comments */
	for (p = line; p < (line + len); p++)
	    if (!isspace(*p)) break;
	if (!len || (*p == '#')) {
#if 0
	    /* entry is correct, so print it */
	    printf("%*.*s\n", len, len, line);
#endif
	    continue;
	}
	
	/*
	 * A correct group entry has four colon-separated fields, the third
	 * of which must be entirely numeric and the fourth of which may
	 * be empty.
	 */
	for (i = k = 0; k < 4; k++) {
	    for (f[k] = line+i; (i < len) && (line[i] != ':'); i++)
		/* nothing */ ;
	    if ((k < 3) && (line[i] != ':'))
		break;
	    line[i++] = 0;
	}
	if (k < 4) {
	    warnx("%s: line %d: missing field(s)", gfn, n);
	    e++;
	    continue;
	}

	/* check if fourth field ended with a colon */
	if (i < len) {
	    warnx("%s: line %d: too many fields", gfn, n);
	    e++;
	    continue;
	}
	
	/* check that none of the fields contain whitespace */
	for (k = 0; k < 4; k++)
	    if (strcspn(f[k], " \t") != strlen(f[k]))
		warnx("%s: line %d: field %d contains whitespace",
		      gfn, n, k+1);

	/* check that the GID is numeric */
	if (strspn(f[2], "0123456789") != strlen(f[2])) {
	    warnx("%s: line %d: GID is not numeric", gfn, n);
	    e++;
	    continue;
	}
	
#if 0
	/* entry is correct, so print it */
	printf("%s:%s:%s:%s\n", f[0], f[1], f[2], f[3]);
#endif	
    }

    /* check what broke the loop */
    if (ferror(gf))
	err(EX_IOERR, "%s: line %d", gfn, n);

    /* done */
    fclose(gf);
    exit(e ? EX_DATAERR : EX_OK);
}
