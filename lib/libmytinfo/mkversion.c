/*
 * mkversion.c
 *
 * By Ross Ridge
 * Public Domain 
 * 92/02/01 07:30:09
 *
 * generates version.c
 *
 */

#define NOTLIB
#include "defs.h"

#include "version.h"

const char SCCSid[] = "@(#) mytinfo mkversion.c 3.2 92/02/01 public domain, By Ross Ridge";

int
main(argc, argv)
int argc;
char **argv; {
	puts("/*");
	puts(" * version.c ");
	puts(" *");
	puts(" * This file was generated automatically.");
	puts(" *");
	puts(" */");
	putchar('\n');

	printf("char _mytinfo_version[] = \"@(#) mytinfo: Release %d, Patchlevel %d (ache).\";\n",
	       RELEASE, PATCHLEVEL);

	return 0;
}
