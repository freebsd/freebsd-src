/*
 * mkcapsort.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/06/04 11:38:02
 *
 * mkcapsort
 *
 * make the sorted lists of pointers to strins
 *
 */

#define NOTLIB
#include "defs.h"
#include <term.h>

#ifdef USE_MYQSORT
#include "qsort.c"
#endif

const char SCCSid[] = "@(#) mytinfo mkcapsort.c 3.3 92/06/04 public domain, By Ross Ridge";

char **sboolnames[NUM_OF_BOOLS], **sboolcodes[NUM_OF_BOOLS], **sboolfnames[NUM_OF_BOOLS];
char **snumnames[NUM_OF_NUMS], **snumcodes[NUM_OF_NUMS], **snumfnames[NUM_OF_NUMS];
char **sstrnames[NUM_OF_STRS], **sstrcodes[NUM_OF_STRS], **sstrfnames[NUM_OF_STRS];


int
main() {
	register int i;

	i = NUM_OF_BOOLS;
	while(i) {
		i--;
		sboolnames[i] = &boolnames[i];
		sboolcodes[i] = &boolcodes[i];
		sboolfnames[i] = &boolfnames[i];
	}

	i = NUM_OF_NUMS;
	while(i) {
		i--;
		snumnames[i] = &numnames[i];
		snumcodes[i] = &numcodes[i];
		snumfnames[i] = &numfnames[i];
	}

	i = NUM_OF_STRS;
	while(i) {
		i--;
		sstrnames[i] = &strnames[i];
		sstrcodes[i] = &strcodes[i];
		sstrfnames[i] = &strfnames[i];
	}

	qsort((anyptr) sboolnames, NUM_OF_BOOLS, sizeof(*sboolnames), _compar);
	qsort((anyptr) sboolcodes, NUM_OF_BOOLS, sizeof(*sboolcodes), _compar);
	qsort((anyptr) sboolfnames, NUM_OF_BOOLS, sizeof(*sboolfnames),_compar);
	qsort((anyptr) snumnames, NUM_OF_NUMS, sizeof(*snumnames), _compar);
	qsort((anyptr) snumcodes, NUM_OF_NUMS, sizeof(*snumcodes), _compar);
	qsort((anyptr) snumfnames, NUM_OF_NUMS, sizeof(*snumfnames), _compar);
	qsort((anyptr) sstrnames, NUM_OF_STRS, sizeof(*sstrnames), _compar);
	qsort((anyptr) sstrcodes, NUM_OF_STRS, sizeof(*sstrcodes), _compar);
	qsort((anyptr) sstrfnames, NUM_OF_STRS, sizeof(*sstrfnames), _compar);

	printf("/*\n");
	printf(" * capsort.c\n");
	printf(" *\n");
	printf(" * This file was generated automatically.\n");
	printf(" *\n");
	printf(" */\n\n");

	puts("extern char *boolnames[], *boolcodes[], *boolfnames[];");
	puts("extern char *numnames[], *numcodes[], *numfnames[];");
	puts("extern char *strnames[], *strcodes[], *strfnames[];");
	putchar('\n');

	printf("char **_sboolnames[] = {\n");
	for(i = 0; i < NUM_OF_BOOLS; i++)
		printf("\tboolnames + %ld,\n", (long) (sboolnames[i] - boolnames));
	printf("	(char **) 0\n");
	printf("};\n\n");

	printf("char **_sboolcodes[] = {\n");
	for(i = 0; i < NUM_OF_BOOLS; i++)
		printf("\tboolcodes + %ld,\n", (long) (sboolcodes[i] - boolcodes));
	printf("	(char **) 0\n");
	printf("};\n\n");

	printf("char **_sboolfnames[] = {\n");
	for(i = 0; i < NUM_OF_BOOLS; i++)
		printf("\tboolfnames + %ld,\n", (long) (sboolfnames[i] - boolfnames));
	printf("	(char **) 0\n");
	printf("};\n\n");

	printf("char **_snumnames[] = {\n");
	for(i = 0; i < NUM_OF_NUMS; i++)
		printf("\tnumnames + %ld,\n", (long) (snumnames[i] - numnames));
	printf("	(char **) 0\n");
	printf("};\n\n");

	printf("char **_snumcodes[] = {\n");
	for(i = 0; i < NUM_OF_NUMS; i++)
		printf("\tnumcodes + %ld,\n", (long) (snumcodes[i] - numcodes));
	printf("	(char **) 0\n");
	printf("};\n\n");

	printf("char **_snumfnames[] = {\n");
	for(i = 0; i < NUM_OF_NUMS; i++)
		printf("\tnumfnames + %ld,\n", (long) (snumfnames[i] - numfnames));
	printf("	(char **) 0\n");
	printf("};\n\n");

	printf("char **_sstrnames[] = {\n");
	for(i = 0; i < NUM_OF_STRS; i++)
		printf("\tstrnames + %ld,\n", (long) (sstrnames[i] - strnames));
	printf("	(char **) 0\n");
	printf("};\n\n");

	printf("char **_sstrcodes[] = {\n");
	for(i = 0; i < NUM_OF_STRS; i++)
		printf("\tstrcodes + %ld,\n", (long) (sstrcodes[i] - strcodes));
	printf("	(char **) 0\n");
	printf("};\n\n");

	printf("char **_sstrfnames[] = {\n");
	for(i = 0; i < NUM_OF_STRS; i++)
		printf("\tstrfnames + %ld,\n", (long) (sstrfnames[i] - strfnames));
	printf("	(char **) 0\n");
	printf("};\n\n");

	return 0;
}
