/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/*
 * Author: Thomas E. Dickey <dickey@clark.net> 1998
 *
 * $Id: ditto.c,v 1.3 1998/08/15 23:39:34 tom Exp $
 *
 * The program illustrates how to set up multiple screens from a single
 * program.  Invoke the program by specifying another terminal on the same
 * machine by specifying its device, e.g.,
 *	ditto /dev/ttyp1
 */
#include <test.priv.h>
#include <sys/stat.h>
#include <errno.h>

typedef struct {
	FILE *input;
	FILE *output;
	SCREEN *screen;
} DITTO;

static void
failed(const char *s)
{
	perror(s);
	exit(EXIT_FAILURE);
}

static void
usage(void)
{
	fprintf(stderr, "usage: ditto [terminal1 ...]\n");
	exit(EXIT_FAILURE);
}

static FILE *
open_tty(char *path)
{
	FILE *fp;
	struct stat sb;

	if (stat(path, &sb) < 0)
		failed(path);
	if ((sb.st_mode & S_IFMT) != S_IFCHR) {
		errno = ENOTTY;
		failed(path);
	}
	fp = fopen(path, "a+");
	if (fp == 0)
		failed(path);
	printf("opened %s\n", path);
	return fp;
}

int
main(
	int argc GCC_UNUSED,
	char *argv[] GCC_UNUSED)
{
	int j;
	int active_tty = 0;
	DITTO *data;

	if (argc <= 1)
		usage();

	if ((data = (DITTO *)calloc(argc, sizeof(DITTO))) == 0)
		failed("calloc data");

	data[0].input = stdin;
	data[0].output = stdout;
	for (j = 1; j < argc; j++) {
		data[j].input =
		data[j].output = open_tty(argv[j]);
	}

	/*
	 * If we got this far, we have open connection(s) to the terminal(s).
	 * Set up the screens.
	 */
	for (j = 0; j < argc; j++) {
		active_tty++;
		data[j].screen = newterm(
			(char *)0,	/* assume $TERM is the same */
			data[j].output,
			data[j].input);
		if (data[j].screen == 0)
			failed("newterm");
		cbreak();
		noecho();
		scrollok(stdscr, TRUE);
	}

	/*
	 * Loop, reading characters from any of the inputs and writing to all
	 * of the screens.
	 */
	for(;;) {
		int ch;
		set_term(data[0].screen);
		ch = getch();
		if (ch == ERR)
			continue;
		if (ch == 4)
			break;
		for (j = 0; j < argc; j++) {
			set_term(data[j].screen);
			addch(ch);
			refresh();
		}
	}

	/*
	 * Cleanup and exit
	 */
	for (j = argc-1; j >= 0; j--) {
		set_term(data[j].screen);
		endwin();
	}
	return EXIT_SUCCESS;
}
