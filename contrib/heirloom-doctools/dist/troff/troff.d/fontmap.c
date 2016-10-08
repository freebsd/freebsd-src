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
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fontmap.h"
#include "bst.h"

extern char *progname;

static void inibuf(char *, size_t);
static char *getword(int);
static int mapcmp(union bst_val, union bst_val);

static char *filepath;
static char *bufp;
static char *bufe;

struct bst map = {
	NULL,
	mapcmp
};

void
rdftmap(char *path) {
	int fd;
	struct stat sb;
	char *buf;
	ssize_t size;
	char *key;
	char *val;
	if (map.root) return;
	filepath = path;
	if ((fd = open(path, O_RDONLY)) == -1) {
		if (errno == ENOENT) return;
		fprintf(stderr, "%s: Can't open font map file %s: ",
		    progname, path);
		perror(NULL);
		exit(1);
	}
	if (fstat(fd, &sb) == -1) {
		fprintf(stderr, "%s: Can't get font map file %s status: ",
		    progname, path);
		perror(NULL);
		exit(1);
	}
	buf = malloc(sb.st_size);
	if ((size = read(fd, buf, sb.st_size)) == -1) {
		fprintf(stderr, "%s: Can't read font map file %s: ",
		    progname, path);
		perror(NULL);
		exit(1);
	}
	close(fd);
	inibuf(buf, size);
	while ((key = getword(0)) && (val = getword(1))) {
		if (avl_add(&map, (union bst_val)(void *)key,
		    (union bst_val)(void *)val)) {
			fprintf(stderr, "%s: Error while adding font "
			    "mapping %s -> %s\n", progname, key, val);
			exit(1);
		}
	}
	free(buf);
}

char *
mapft(char *name) {
	struct bst_node *n;
	if (map.root && !bst_srch(&map, (union bst_val)(void *)name, &n))
		name = n->data.p;
	return name;
}

static void
inibuf(char *buf, size_t size) {
	bufp = buf;
	bufe = buf + size;
}

/* 
 * type:
 *   0  Accect \s*((#.*)?$|\S+) or EOF
 *   1  Accept \s*\S+
 */

static char *
getword(int type) {
	char *word = bufp;
	char c;
nl:
	while (word < bufe && ((c = *word) == ' ' || c == '\t')) word++;
	if (word == bufe) goto eof;
	if (c == '#' || c == '\n') {
		if (type == 1) {
			fprintf(stderr, "%s: Syntax error in font map file "
			    "%s\n", progname, filepath);
			exit(1);
		}
		while (word < bufe && *word++ != '\n');
		if (word == bufe) goto eof;
		goto nl;
	}
	bufp = word;
	while (bufp < bufe && ((c = *bufp) != ' ' && c != '\t' && c != '\n'
	    && c != '#')) bufp++;
	if (type == 0 && (c == '#' || c == '\n')) {
		fprintf(stderr, "%s: Syntax error in font map file %s\n",
		   progname, filepath);
		exit(1);
	}
	*bufp = 0;
	word = strdup(word);
	*bufp = c;
	return word;
eof:
	if (type == 1) {
		fprintf(stderr, "%s: Unexpected end of font map file %s\n",
		    progname, filepath);
		exit(1);
	}
	return NULL;
}

static int
mapcmp(union bst_val a, union bst_val b) {
	return strcmp(a.p, b.p);
}
