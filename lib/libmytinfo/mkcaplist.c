/*
 * mkcaplist.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:30:05
 *
 * mkcaplist [-n caps] [file]
 *
 * makes caplist.c from the cap_list file
 *
 */

#define NOTLIB
#include "defs.h"

const char SCCSid[] = "@(#) mytinfo mkcaplist.c 3.2 92/02/01 public domain, By Ross Ridge";

#define DEFAULT_CAPS	1000

struct caplist *list;

int
main(argc, argv)
int argc;
char **argv; {
	FILE *f;
	int caps = DEFAULT_CAPS;
	int n;
	register int i;

	if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'n') {
		caps = atoi(argv[2]);
		argv += 2;
		argc -= 2;
	}

	if (argc == 1) {
		f = stdin;
	} else if (argc == 2) {
		f = fopen(argv[1], "r");
		if (f == NULL) {
			fprintf(stderr, "%s: can't open '%s'\n", argv[0],
				argv[1]);
			return 1;
		}
	} else {
		fprintf(stderr, "%s: too many arguments\n", argv[0]);
		fprintf(stderr, "usage: %s [-n caps] [file]\n", argv[0]);
		return 1;
	}

	list = (struct caplist *) malloc(caps * sizeof(struct caplist));
	if (list == NULL) {
		fprintf(stderr, "%s: malloc failed.\n", argv[0]);
		return 1;
	}

	n = readcaps(f, list, caps);
	if (n > caps) {
		fprintf(stderr, "%s: too many caps, use -n.\n", argv[0]);
		return 1;
	}
	if (n == 0) {
		fprintf(stderr, "%s: no caps in file.\n", argv[0]);
		return 1;
	}
	if (n == -1) {
		fprintf(stderr, "%s: bad caps line.\n", argv[0]);
		return 1;
	}
	if (n == -2) {
		fprintf(stderr, "%s: unexpected EOF.\n", argv[0]);
		return 1;
	}

	puts("/*");
	puts(" * caplist.c ");
	puts(" *");
	puts(" * This file was generated automatically.");
	puts(" *");
	puts(" */");
	putchar('\n');

	puts("char *boolnames[] = {");
	for (i = 0; i < n; i++)
		if (list[i].type == '!')
			printf("\t\"%s\",\n", list[i].tinfo);
	puts("\t(char *)0");
	puts("};");
	putchar('\n');

	puts("char *boolcodes[] = {");
	for (i = 0; i < n; i++)
		if (list[i].type == '!')
			printf("\t\"%s\",\n", list[i].tcap);
	puts("\t(char *)0");
	puts("};");
	putchar('\n');

	puts("char *boolfnames[] = {");
	for (i = 0; i < n; i++)
		if (list[i].type == '!')
			printf("\t\"%s\",\n", list[i].var);
	puts("\t(char *)0");
	puts("};");
	putchar('\n');

	puts("char *numnames[] = {");
	for (i = 0; i < n; i++)
		if (list[i].type == '#')
			printf("\t\"%s\",\n", list[i].tinfo);
	puts("\t(char *)0");
	puts("};");
	putchar('\n');

	puts("char *numcodes[] = {");
	for (i = 0; i < n; i++)
		if (list[i].type == '#')
			printf("\t\"%s\",\n", list[i].tcap);
	puts("\t(char *)0");
	puts("};");
	putchar('\n');

	puts("char *numfnames[] = {");
	for (i = 0; i < n; i++)
		if (list[i].type == '#')
			printf("\t\"%s\",\n", list[i].var);
	puts("\t(char *)0");
	puts("};");
	putchar('\n');

	puts("char *strnames[] = {");
	for (i = 0; i < n; i++)
		if (list[i].type == '$')
			printf("\t\"%s\",\n", list[i].tinfo);
	puts("\t(char *)0");
	puts("};");
	putchar('\n');

	puts("char *strcodes[] = {");
	for (i = 0; i < n; i++)
		if (list[i].type == '$')
			printf("\t\"%s\",\n", list[i].tcap);
	puts("\t(char *)0");
	puts("};");
	putchar('\n');

	puts("char *strfnames[] = {");
	for (i = 0; i < n; i++)
		if (list[i].type == '$')
			printf("\t\"%s\",\n", list[i].var);
	puts("\t(char *)0");
	puts("};");
	putchar('\n');

	puts("char _strflags[] = {");
	for (i = 0; i < n; i++)
		if (list[i].type == '$')
			printf("\t'%c',\n", list[i].flag);
	puts("\t'\\0'");
	puts("};");
	putchar('\n');

	return 0;
}
