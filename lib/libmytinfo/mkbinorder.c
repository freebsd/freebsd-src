/*
 * mkbinorder.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:30:04
 *
 */

#define NOTLIB
#include "defs.h"
#include <term.h>

const char SCCSid[] = "@(#) mytinfo mkbinorder.c 3.2 92/02/01 public domain, By Ross Ridge";

char cap[MAX_NAME];
char *p2c = cap;
char **p2p2c = &p2c;

int
main(argc, argv)
int argc;
char **argv; {
	int r;
	int ind;
	FILE *f;

	if (argc == 1)
		f = stdin;
	else if (argc == 2) {
		f = fopen(argv[1], "r");
		if (f == NULL) {
			fprintf(stderr, "can't open %s\n", argv[1]);
			exit(1);
		}
	} else {
		fprintf(stderr, "argument count\n");
		exit(1);
	}

	do {
		r = fscanf(f, "%s", cap);
	} while(r == 1 && strcmp(cap, "!") != 0);
	if (r != 1) {
		fprintf(stderr, "expected '!'\n");
		exit(1);
	}

	puts("/*");
	puts(" * binorder.c");
	puts(" *");
	puts(" * This file was generated automatically");
	puts(" *");
	puts(" */\n");

	puts("int _boolorder[] = {");

	while(1) {
		r = fscanf(f, "%s", cap);
		cap[MAX_TINFONAME] = '\0';
		if (r != 1) {
			fprintf(stderr, "unexpected EOF\n");
			exit(1);
		}
		if (strcmp(cap, "#") == 0)
			break;
		ind = _findboolname(cap);
		if (ind == -1) {
			fprintf(stderr, "unknown bool name '%s'\n", cap);
			continue;
		}
		printf("\t%d,\n", ind);
	}
	puts("\t-1");
	puts("};\n");

	puts("int _numorder[] = {");

	while(1) {
		r = fscanf(f, "%s", cap);
		cap[MAX_TINFONAME] = '\0';
		if (r != 1) {
			fprintf(stderr, "unexpected EOF\n");
			exit(1);
		}
		if (strcmp(cap, "$") == 0)
			break;
		ind = _findnumname(cap);
		if (ind == -1) {
			fprintf(stderr, "unknown num name '%s'\n", cap);
			continue;
		}
		printf("\t%d,\n", ind);
	}
	puts("\t-1");
	puts("};\n");

	puts("int _strorder[] = {");

	while(1) {
		r = fscanf(f, "%s", cap);
		cap[MAX_TINFONAME] = '\0';
		if (r != 1)
			break;
		ind = _findstrname(cap);
		if (ind == -1) {
			fprintf(stderr, "unknown str name '%s'\n", cap);
			continue;
		}
		printf("\t%d,\n", ind);
	}
	puts("\t-1");
	puts("};\n");

	return 0;
}
