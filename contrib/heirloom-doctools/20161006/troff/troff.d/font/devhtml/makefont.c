/*
 * Copyright (c) 2015, Carsten Kunze
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

static int find_font(char **, ssize_t *);
static char *get_word(char **, ssize_t *, int *);
static void next_line(char **, ssize_t *);
static void out_font(char *, ssize_t, int);
static ssize_t read_file(char **);
static int usage(void);

static char *progname;
static char *fontname;
static char *charfile = "charset";

int
main(int argc, char **argv) {
	int i;
	char *b, *b0;
	ssize_t s;
	progname = argv[0];
	if (argc != 2) return usage();
	fontname = argv[1];
	s = read_file(&b0);
	b = b0;
	if (!(i = find_font(&b, &s))) {
		fprintf(stderr, "%s: Font %s not found in %s.\n",
		    progname, fontname, charfile);
		exit(EXIT_FAILURE);
	}
	out_font(b, s, i);
	free(b0);
	return EXIT_SUCCESS;
}

static ssize_t
read_file(char **b) {
	int f;
	struct stat s;
	ssize_t n;
	if ((f = open(charfile, O_RDONLY)) == -1) {
		fprintf(stderr, "%s: open(%s) failed: ", progname,
		    charfile);
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	if (fstat(f, &s) == -1) {
		fprintf(stderr, "%s: fstat(%s) failed: ", progname,
		    charfile);
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	if (!(*b = malloc(s.st_size + 1))) exit(EXIT_FAILURE);
	if ((n = read(f, *b, s.st_size)) == -1) {
		fprintf(stderr, "%s: read(%s) failed: ", progname,
		    charfile);
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	close(f);
	return n;
}

static int
find_font(char **b, ssize_t *s) {
	int i;
	int f;
	char *w;
	for (i = 1; ; i++) {
		w = get_word(b, s, &f);
		if (!w) return 0;
		if (!strcmp(w, fontname)) break;
		if (f || !*s) return 0;
	}
	if (!f) next_line(b, s);
	return i;
}

static void
out_font(char *b, ssize_t s, int i) {
	int l;
	for (l = 0; s;) {
		int j;
		char *w;
		int f;
		w = get_word(&b, &s, &f);
		if (!w) exit(EXIT_SUCCESS);
		fputs(w, stdout);
		if (f || !s) {
			fprintf(stderr,
			    "%s: Unexpected end of line or file.\n",
			    progname);
			exit(EXIT_FAILURE);
		}
		for (j = 0; j < i; j++) {
			w = get_word(&b, &s, &f);
			if (!w) {
				fprintf(stderr,
				    "%s: Unexpected end of line or file.\n",
				    progname);
				exit(EXIT_FAILURE);
			}
		}
		printf("\t%s%s\n", w, l && strcmp(w, "\"") ? "\t0\t0" : "");
		if (!f) next_line(&b, &s);
		if (!l) {
			l++;
			puts("charset");
		}
	}
}

static char *
get_word(char **b, ssize_t *s, int *f) { /* buffer, size, flags */
	char *w = NULL;
	char *p = *b;
	ssize_t n = *s;
	unsigned t = 0;
	int c;
	while (n && (!(c = *p) || c == ' ' || c == '\t')) {
		n--;
		p++;
	}
	if (c == '\n') {
		t = 1;
		goto r;
	}
	if (!n) goto r;
	w = p;
	while (n && (c = *p) && c != ' ' && c != '\t' && c != '\n') {
		n--;
		p++;
	}
	if (c == '\n') t = 1;
	*p = 0;
r:
	*b = p;
	*s = n;
	if (f) *f = t;
	return w;
}

static void
next_line(char **b, ssize_t *s) {
	char *p = *b;
	ssize_t n = *s;
	while (n && *p != '\n') {
		n--;
		p++;
	}
	if (n) {
		n--;
		p++;
	}
	*b = p;
	*s = n;
}

static int
usage(void) {
	fprintf(stderr,
"Usage: ./%s <font_name> >> <font_name>\n"
	    , progname);
	return EXIT_FAILURE;
}
