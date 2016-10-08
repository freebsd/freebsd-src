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
#include <unistd.h>
#include <string.h>
#include "main.h"
#include "bst.h"

void * /* pointer to \0 terminated file contents */
file2ram(char *p, /* path to file */
    ssize_t *l) { /* pointer to buffer size variable */
	int f;
	struct stat s;
	char *b = NULL;
	if ((f = open(p, O_RDONLY)) == -1) {
		fprintf(stderr, "%s: open(%s) failed: ", progname, p);
		perror(NULL);
		goto r;
	}
	if (fstat(f, &s) == -1) {
		fprintf(stderr, "%s: fstat(%s) failed: ", progname, p);
		perror(NULL);
		goto c;
	}
	if (!(b = malloc(s.st_size + 1))) goto c;
	if ((*l = read(f, b, s.st_size)) == -1) {
		free(b);
		fprintf(stderr, "%s: read(%s) failed: ", progname, p);
		perror(NULL);
		goto c;
	}
	b[*l] = 0;
c:
	close(f);
r:
	return b;
}

ssize_t
lineskip(char **b, ssize_t n) {
	char *p = *b;
	int i = 0;
	int c;
	while (1) {
		for (; n && (!(c = *p) || c == ' ' || c == '\t' || c == '\n');
		    n--, p++) {
			if (c == '\n') i = 0;
			else i++;
		}
		if (!n || !i || c != '#') goto r;
		for (; n && (c = *p) && c != '\n'; n--, p++);
	}
r:
	*b = p;
	return n;
}

char * /* pointer to word */
get_word(char **b, /* begin of buffer, then end of buffer */
    ssize_t *l, /* buffer length */
    size_t *s, /* word length (if not NULL) */
    int *t) { /* type (if not NULL) */
	char *w = NULL;
	char *p = *b;
	ssize_t n = *l;
	int c;
	size_t i = 0;
	int f = 0;
	for (; n && (!(c = *p) || c == ' ' || c == '\t' || c == '\n');
	    n--, p++);
	if (!n) goto r;
	w = p;
	for (; n && (c = *p) && c != ' ' && c != '\t' && c != '\n';
	    i++, n--, p++) {
		if (c >= '0' && c <= '9') f |= 1;
		else f |= 2;
	}
	if (!n) goto r;
	*p = 0;
r:
	*b = p;
	*l = n;
	if (s) *s = i;
	if (t) *t = f;
	return w;
}

char * /* pointer to line */
get_line(char **b, /* begin of buffer, then end of buffer */
    ssize_t *l, /* buffer length */
    size_t *s) { /* line length (if not NULL) */
	char *w = NULL;
	char *p = *b;
	ssize_t n = *l;
	int c;
	size_t i = 0;
	for (; n && (!(c = *p) || c == ' ' || c == '\t');
	    n--, p++);
	if (!n) goto r;
	w = p;
	for (; n && (c = *p) && c != '\n'; i++, n--, p++);
	if (!n) goto r;
	*p = 0;
r:
	*b = p;
	*l = n;
	if (s) *s = i;
	return w;
}

int
bst_scmp(union bst_val a, union bst_val b) {
	return strcmp(a.p, b.p);
}
int
bst_icmp(union bst_val a, union bst_val b) {
	return a.i < b.i ? -1 :
	       a.i > b.i ?  1 :
	                    0 ;
}
