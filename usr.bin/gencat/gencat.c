
/***********************************************************
Copyright 1990, by Alfalfa Software Incorporated, Cambridge, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that Alfalfa's name not be used in
advertising or publicity pertaining to distribution of the software
without specific, written prior permission.

ALPHALPHA DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
ALPHALPHA BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

If you make any modifications, bugfixes or other changes to this software
we'd appreciate it if you could send a copy to us so we can keep things
up-to-date.  Many thanks.
				Kee Hinckley
				Alfalfa Software, Inc.
				267 Allston St., #3
				Cambridge, MA 02139  USA
				nazgul@alfalfa.com

******************************************************************/

/* Edit History

01/18/91   3 hamilton	#if not reparsed
01/12/91   2 schulert	conditionally use prototypes
12/23/90   2 hamilton	Fix fd == NULL to fd < 0
11/03/90   1 hamilton	Alphalpha->Alfalfa & OmegaMail->Poste
08/13/90   1 schulert	move from ua to omu
*/

#include <err.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef SYSV
#include <sys/fcntl.h>
#define L_SET SEEK_SET
#define L_INCR SEEK_CUR
#endif
#include <sys/file.h>
#include <sys/stat.h>
#include "gencat.h"

/*
 * The spec says the syntax is "gencat catfile msgfile...".
 * We extend it to:
 * 	gencat [-lang C|C++|ANSIC] catfile msgfile [-h <header-file>]...
 * Flags are order dependant, we'll take whatever lang was most recently chosen
 * and use it to generate the next header file.  The header files are generated
 * at the point in the command line they are listed.  Thus the sequence:
 *	gencat -lang C foo.cat foo.mcs -h foo.h -lang C++ bar.mcs -h bar.H
 * will put constants from foo.mcs into foo.h and constants from bar.mcs into
 * bar.h.  Constants are not saved in the catalog file, so nothing will come
 * from that, even if things have been defined before.  The constants in foo.h
 * will be in C syntax, in bar.H in C++ syntax.
 */

#if ANSI_C || defined(__cplusplus)
# define P_(x) x
#else
# define P_(x) /**/
#endif

static void writeIfChanged P_((char *fname, int lang, int orConsts));

#undef P_

static void
usage()
{
    fprintf(stderr, "usage: gencat [-new] [-or] [-lang C|C++|ANSIC]\n");
    fprintf(stderr, "              catfile msgfile [-h <header-file>]...\n");
	exit(1);
}

int main(
#if ANSI_C || defined(__cplusplus)
		int argc, char *argv[])
#else
		argc, argv)
int argc;
char *argv[];
#endif
{
    int		ofd, ifd, i;
    char	*catfile = NULL;
    char	*input = NULL;
    int		lang = MCLangC;
    int		new = False;
    int		orConsts = False;

    for (i = 1; i < argc; ++i) {
	if (argv[i][0] == '-') {
	    if (strcmp(argv[i], "-lang") == 0) {
		++i;
		if (strcmp(argv[i], "C") == 0) lang = MCLangC;
		else if (strcmp(argv[i], "C++") == 0) lang = MCLangCPlusPlus;
		else if (strcmp(argv[i], "ANSIC") == 0) lang = MCLangANSIC;
		else {
		    errx(1, "unrecognized language: %s", argv[i]);
		}
	    } else if (strcmp(argv[i], "-h") == 0) {
		if (!input)
		    errx(1, "can't write to a header before reading something");
		++i;
		writeIfChanged(argv[i], lang, orConsts);
	    } else if (strcmp(argv[i], "-new") == 0) {
		if (catfile)
		    errx(1, "you must specify -new before the catalog file name");
		new = True;
	    } else if (strcmp(argv[i], "-or") == 0) {
		orConsts = ~orConsts;
	    } else {
		usage();
	    }
        } else {
	    if (!catfile) {
		catfile = argv[i];
		if (new) {
		    if ((ofd = open(catfile, O_WRONLY|O_TRUNC|O_CREAT, 0666)) < 0)
				errx(1, "unable to create a new %s", catfile);
		} else if ((ofd = open(catfile, O_RDONLY)) < 0) {
		    if ((ofd = open(catfile, O_WRONLY|O_CREAT, 0666)) < 0)
				errx(1, "unable to create %s", catfile);
		} else {
		    MCReadCat(ofd);
		    close(ofd);
		    if ((ofd = open(catfile, O_WRONLY|O_TRUNC)) < 0)
				errx(1, "unable to truncate %s", catfile);
		}
	    } else {
		input = argv[i];
		if ((ifd = open(input, O_RDONLY)) < 0)
		    errx(1, "unable to read %s", input);
		MCParse(ifd);
		close(ifd);
	    }
	}
    }
    if (catfile) {
	MCWriteCat(ofd);
	exit(0);
    } else {
	usage();
    }
    return 0;
}

static void writeIfChanged(
#if ANSI_C || defined(__cplusplus)
		char *fname, int lang, int orConsts)
#else
		fname, lang, orConsts)
char *fname;
int lang;
int orConsts;
#endif
{
    char	tmpname[32];
    char	buf[BUFSIZ], tbuf[BUFSIZ], *cptr, *tptr;
    int		fd, tfd;
    int		diff = False;
    int		len, tlen;
    struct stat	sbuf;

    /* If it doesn't exist, just create it */
    if (stat(fname, &sbuf)) {
	if ((fd = open(fname, O_WRONLY|O_CREAT, 0666)) < 0)
	    errx(1, "unable to create header file %s", fname);
	MCWriteConst(fd, lang, orConsts);
	close(fd);
	return;
    }

    /* If it does exist, create a temp file for now */
    sprintf(tmpname, "/tmp/gencat.%d", (int) getpid());
    if ((tfd = open(tmpname, O_RDWR|O_CREAT, 0666)) < 0)
		errx(1, "unable to open temporary file: %s", tmpname);
    unlink(tmpname);

    /* Write to the temp file and rewind */
    MCWriteConst(tfd, lang, orConsts);

    /* Open the real header file */
    if ((fd = open(fname, O_RDONLY)) < 0)
		errx(1, "unable to read header file: %s", fname);

    /* Backup to the start of the temp file */
    if (lseek(tfd, 0L, L_SET) < 0)
		errx(1, "unable to seek in tempfile: %s", tmpname);

    /* Now compare them */
    while ((tlen = read(tfd, tbuf, BUFSIZ)) > 0) {
	if ((len = read(fd, buf, BUFSIZ)) != tlen) {
	    diff = True;
	    goto done;
	}
	for (cptr = buf, tptr = tbuf; cptr < buf+len; ++cptr, ++tptr) {
	    if (*tptr != *cptr) {
		diff = True;
		goto done;
	    }
	}
    }
done:
    if (diff) {
	if (lseek(tfd, 0L, L_SET) < 0)
	    errx(1, "unable to seek in tempfile: %s", tmpname);
	close(fd);
	if ((fd = open(fname, O_WRONLY|O_TRUNC)) < 0)
	    errx(1, "unable to truncate header file: %s", fname);
	while ((len = read(tfd, buf, BUFSIZ)) > 0) {
	    if (write(fd, buf, len) != len)
			warnx("error writing to header file: %s", fname);
	}
    }
    close(fd);
    close(tfd);
}
