/* 
 * bp.c --
 *
 *	This file contains the "bp" ("binary patch") program.  It is used
 *	to replace configuration strings in Tcl/Tk binaries as part of
 *	installation.
 *
 *	Usage:  bp file search replace
 *
 *	This program searches file bp for the first occurrence of the
 *	character string given by "search".  If it is found, then the
 *	first characters of that string get replaced by the string
 *	given by "replace".  The replacement string is NULL-terminated.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * All rights reserved.
 * This file is NOT subject to the terms described in "license.terms".
 *
 * SCCS: @(#) bp.c 1.2 96/03/12 09:08:26
 */

#include <stdio.h>
#include <string.h>

extern int errno;

/*
 * The array below saves the last few bytes read from the file, so that
 * they can be compared against a particular string that we're looking
 * for.
 */

#define BUFFER_SIZE 200
char buffer[BUFFER_SIZE];

int
main(argc, argv)
    int argc;			/* Number of command-line arguments. */
    char **argv;		/* Values of command-line arguments. */
{
    int length, matchChar, fileChar, cur, fileIndex, stringIndex;
    char *s;
    FILE *f;

    if (argc != 4) {
	fprintf(stderr,
		"Wrong # args: should be \"%s fileName string replace\"\n",
		argv[0]);
	exit(1);
    }
    f = fopen(argv[1], "r+");
    if (f == NULL) {
	fprintf(stderr,
		"Couldn't open \"%s\" for writing: %s\n",
		argv[1], strerror(errno));
	exit(1);
    }

    for (cur = 0; cur < BUFFER_SIZE; cur++) {
	buffer[cur] = 0;
    }
    s = argv[2];
    length = strlen(s);
    if (length > BUFFER_SIZE) {
	fprintf(stderr,
	    "String \"%s\" too long;  must be %d or fewer chars.\n",
	    s, BUFFER_SIZE);
	exit(1);
    }
    matchChar = s[length-1];

    while (1) {
	fileChar = getc(f);
	if (fileChar == EOF) {
	    if (ferror(f)) {
		goto ioError;
	    }
	    fprintf(stderr, "Couldn't find string \"%s\"\n", argv[2]);
	    exit(1);
	}
	buffer[cur] = fileChar;
	if (fileChar == matchChar) {
	    /*
	     * Last character of the string matches the current character
	     * from the file.  Search backwards through the buffer to
	     * see if the preceding characters from the file match the
	     * characters from the string.
	     */
	    for (fileIndex = cur-1, stringIndex = length-2;
		    stringIndex >= 0; fileIndex--, stringIndex--) {
		if (fileIndex < 0) {
		    fileIndex = BUFFER_SIZE-1;
		}
		if (buffer[fileIndex] != s[stringIndex]) {
		    goto noMatch;
		}
	    }

	    /*
	     * Matched!  Backup to the start of the string, then
	     * overwrite it with the replacement value.
	     */

	    if (fseek(f, -length, SEEK_CUR) == -1) {
		goto ioError;
	    }
	    if (fwrite(argv[3], strlen(argv[3])+1, 1, f) == 0) {
		goto ioError;
	    }
	    exit(0);
	}

	/*
	 * No match;  go on to next character of file.
	 */

	noMatch:
	cur++;
	if (cur >= BUFFER_SIZE) {
	    cur = 0;
	}
    }

    ioError:
    fprintf(stderr, "I/O error: %s\n", strerror(errno));
    exit(1);
}
