/* wconfig.c */
/*
 * Copyright 1995,1996,1997,1998 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*
 * Program to take the place of the configure shell script under DOS.
 * The makefile.in files are constructed in such a way that all this
 * program needs to do is uncomment lines beginning ##DOS by removing the
 * first 5 characters of the line.  This will allow lines like:
 * ##DOS!include win-pre.in to become: !include win-pre.in
 *
 * We also turn any line beginning with '@' into a blank line.
 *
 * If a config directory is specified, then the output will be start with
 * config\pre.in, then the filtered stdin text, and will end with
 * config\post.in.
 *
 * Syntax: wconfig [options] [config_directory] <input_file >output_file
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int copy_file (char *path, char *fname);
void add_ignore_list(char *str);

int mit_specific = 0;

char *win16_flag = "WIN16##";
char *win32_flag = "WIN32##";


int main(int argc, char *argv[])
{
	char *ignore_str = "--ignore=";
	int ignore_len;
	char *cp, *tmp;
	char *win_flag;
	char wflags[1024];
	size_t wlen, alen;

#ifdef _WIN32
	win_flag = win32_flag;
#else
	win_flag = "UNIX##";
#endif

	wlen = 0;

	ignore_len = strlen(ignore_str);
	argc--; argv++;
	while (*argv && *argv[0] == '-') {
		alen = strlen(*argv);
		if (wlen + 1 + alen > sizeof (wflags) - 1) {
			fprintf (stderr,
				 "wconfig: argument list too long (internal limit %lu)",
				 (unsigned long) sizeof (wflags));
			exit (1);
		}
		if (wlen > 0)
			wflags[wlen++] = ' ';
		memcpy(&wflags[wlen], *argv, alen);
		wlen += alen;

		if (!strcmp(*argv, "--mit")) {
			mit_specific = 1;
			argc--; argv++;
			continue;
		}
		if (!strcmp(*argv, "--win16")) {
			win_flag = win16_flag;
			argc--; argv++;
			continue;
		}
		if (!strcmp(*argv, "--win32")) {
			win_flag = win32_flag;
			argc--; argv++;
			continue;
		}
		if (!strncmp(*argv, "--enable-", 9)) {
			tmp = malloc(alen - ignore_len + 3);
			if (!tmp) {
				fprintf(stderr,
					"wconfig: malloc failed!\n");
				exit(1);
			}
			memcpy(tmp, *argv + ignore_len, alen - ignore_len);
			memcpy(tmp + alen - ignore_len, "##", 3);
			for (cp = tmp; *cp; cp++) {
				if (islower(*cp))
					*cp = toupper(*cp);
			}
			add_ignore_list(tmp);
			argc--; argv++;
			continue;
		}
		if (!strncmp(*argv, ignore_str, ignore_len)) {
			add_ignore_list((*argv)+ignore_len);
			argc--; argv++;
			continue;
		}
		fprintf(stderr, "Invalid option: %s\n", *argv);
		exit(1);
	}
	wflags[wlen] = '\0';

	if (win_flag)
		add_ignore_list(win_flag);

	if (mit_specific)
		add_ignore_list("MIT##");

	if (wflags[0] && (argc > 0))
		printf("WCONFIG_FLAGS=%s\n", wflags);

	if (argc > 0)
		copy_file (*argv, "win-pre.in");

	copy_file("", "-");

	if (argc > 0)
		copy_file (*argv, "win-post.in");

	return 0;
}

char *ignore_list[64] = {
	"DOS##",
	"DOS",
	};

/*
 * Add a new item to the ignore list
 */
void add_ignore_list(char *str)
{
	char **cpp;

	for (cpp = ignore_list; *cpp; cpp++)
		;
	*cpp = str;
}


/*
 *
 * Copy_file
 *
 * Copies file 'path\fname' to stdout.
 *
 */
static int
copy_file (char *path, char *fname)
{
    FILE *fin;
    char buf[1024];
    char **cpp, *ptr;
    size_t len, plen, flen;

    if (strcmp(fname, "-") == 0) {
	    fin = stdin;
    } else {
	    plen = strlen(path);
	    flen = strlen(fname);
	    if (plen + 1 + flen > sizeof(buf) - 1) {
		    fprintf(stderr, "Name %s or %s too long", path, fname);
		    return 1;
	    }
	    memcpy(buf, path, plen);
#ifdef _WIN32
	    buf[plen] = '\\';
#else
	    buf[plen] = '/';
#endif
	    memcpy(buf + plen + 1, fname, flen);
	    buf[plen + 1 + flen] = '\0';
	    fin = fopen (buf, "r");                     /* File to read */
	    if (fin == NULL) {
		    fprintf(stderr, "wconfig: Can't open file %s\n", buf);
		    return 1;
	    }
    }


    while (fgets (buf, sizeof(buf), fin) != NULL) { /* Copy file over */
	    if (buf[0] == '@') {
		    fputs("\n", stdout);
		    continue;
	    }
	    if (buf[0] != '#' || buf[1] != '#') {
		    fputs(buf, stdout);
		    continue;
	    }
	    ptr = buf;
	    for (cpp = ignore_list; *cpp; cpp++) {
		    len = strlen(*cpp);
		    if (memcmp (*cpp, buf+2, len) == 0) {
			    ptr += 2+len;
			    break;
		    }
	    }
	    fputs(ptr, stdout);
    }

    fclose (fin);

    return 0;
}
