/*
 * Copyright (c) 2002, 2003 Greg Lehey
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * This software is provided by the author ``as is'' and any express
 * or implied warranties, including, but not limited to, the implied
 * warranties of merchantability and fitness for a particular purpose
 * are disclaimed.  In no event shall the author be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability,
 * whether in contract, strict liability, or tort (including
 * negligence or otherwise) arising in any way out of the use of this
 * software, even if advised of the possibility of such damage.
 */
/* $Id: asf.c,v 1.4 2003/05/04 02:55:20 grog Exp grog $ */
/* $FreeBSD$ */

#define MAXLINE 1024
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXTOKEN 10
char *token[MAXTOKEN];
char *modules_path;			/* path relative to kernel
					 * build directory */
char *outfile;				/* and where to write the output */

/*
 * Take a blank separated list of tokens and turn it into a list of
 * individual nul-delimited strings.  Build a list of pointers at
 * token, which must have enough space for the tokens.  Return the
 * number of tokens, or -1 on error (typically a missing string
 * delimiter).
 */
int
tokenize(char *cptr, char *token[], int maxtoken)
{
    char delim;				/* delimiter to search for */
    int tokennr;			/* index of this token */

    for (tokennr = 0; tokennr < maxtoken;) {
	while (isspace(*cptr))
	    cptr++;			/* skip initial white space */
	if ((*cptr == '\0') || (*cptr == '\n')
	    || (*cptr == '#'))		/* end of line */
	    return tokennr;		/* return number of tokens found */
	delim = *cptr;
	token[tokennr] = cptr;		/* point to it */
	tokennr++;			/* one more */
	if (tokennr == maxtoken)	/* run off the end? */
	    return tokennr;
	if ((delim == '\'') || (delim == '"')) { /* delimitered */
	    for (;;) {
		cptr++;
		if ((*cptr == delim)
		    && (cptr[-1] != '\\')) { /* found the partner */
		    cptr++;		/* move on past */
		    if (!isspace(*cptr)) /* no space after closing quote */
			return -1;
		    *cptr++ = '\0';	/* delimit */
		} else if ((*cptr == '\0')
		    || (*cptr == '\n'))	/* end of line */
		    return -1;
	    }
	} else {			/* not quoted */
	    while ((*cptr != '\0') && (!isspace(*cptr)) && (*cptr != '\n'))
		cptr++;
	    if (*cptr != '\0')		/* not end of the line, */
		*cptr++ = '\0';		/* delimit and move to the next */
	}
    }
    return maxtoken;			/* can't get here */
}

void
usage(char *myname)
{
    fprintf(stderr,
	"Usage:\n"
	"%s [-a] [-k] [-t] [modules-path [outfile]]\n\n"
	"\t-a\tappend to outfile)\n"
	"\t-k\ttake input from kldstat(8)\n",
	"\t-x\tdon't append \".debug\" to module name\n",
	myname);
}

int
main(int argc, char *argv[])
{
    char buf[MAXLINE];
    FILE *kldstat;
    FILE *objcopy;
    FILE *out;				/* output file */
    char ocbuf[MAXLINE];
    int tokens;				/* number of tokens on line */
    char basetoken[MAXLINE];
    int i;
    char *filemode = "w";		/* mode for outfile */
    char cwd[MAXPATHLEN];		/* current directory */
    char *debugname = ".debug";		/* some file names end in this */

    getcwd(cwd, MAXPATHLEN);		/* find where we are */
    kldstat = stdin;
    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    if (strcmp(argv[i], "-k") == 0) { /* get input from kldstat(8) */
		if (!(kldstat = popen("kldstat", "r"))) {
		    perror("Can't start kldstat");
		    return 1;
		}
	    } else if (strcmp(argv[i], "-a") == 0) /* append to outfile */
		filemode = "a";
	    else if (strcmp(argv[i], "-x") == 0) /* no .debug extension */
		debugname = "";		/* nothing */
	    else {
		fprintf(stderr,
		    "Invalid option: %s, aborting\n",
		    argv[i]);
		usage(argv[0]);
		return 1;
	    }
	} else if (modules_path == NULL)
	    modules_path = argv[i];
	else if (outfile == NULL)
	    outfile = argv[i];
	else {
	    fprintf(stderr,
		"Extraneous startup information: \"%s\", aborting\n",
		argv[i]);
	    usage(argv[0]);
	    return 1;
	}
    }
    if (modules_path == NULL)
	modules_path = "modules";
    if (outfile == NULL)
	outfile = ".asf";
    if ((out = fopen(outfile, filemode)) == NULL) {
	fprintf(stderr,
	    "Can't open output file %s: %s (%d)\n",
	    outfile,
	    strerror(errno),
	    errno);
	return 1;
    }
    while (fgets(buf, MAXLINE, kldstat)) {
	if ((!(strstr(buf, "kernel")))
	    && buf[0] != 'I') {
	    quad_t base;
	    quad_t textaddr;
	    quad_t dataaddr;
	    quad_t bssaddr;

	    tokens = tokenize(buf, token, MAXTOKEN);
	    base = strtoll(token[2], NULL, 16);
	    strcpy(basetoken, token[4]);
	    basetoken[strlen(basetoken) - 3] = '\0'; /* cut off the .ko */
	    snprintf(ocbuf,
		MAXLINE,
		"/usr/bin/objdump --section-headers %s/%s/%s%s",
		modules_path,
		basetoken,
		token[4],
		debugname);
	    if (!(objcopy = popen(ocbuf, "r"))) {
		fprintf(stderr,
		    "Can't start %s: %s (%d)\n",
		    ocbuf,
		    strerror(errno),
		    errno);
		return 1;
	    }
	    while (fgets(ocbuf, MAXLINE, objcopy)) {
		int octokens;
		char *octoken[MAXTOKEN];

		octokens = tokenize(ocbuf, octoken, MAXTOKEN);
		if (octokens > 1) {
		    if (!strcmp(octoken[1], ".text"))
			textaddr = strtoll(octoken[3], NULL, 16) + base;
		    else if (!strcmp(octoken[1], ".data"))
			dataaddr = strtoll(octoken[3], NULL, 16) + base;
		    else if (!strcmp(octoken[1], ".bss"))
			bssaddr = strtoll(octoken[3], NULL, 16) + base;
		}
	    }
	    if (textaddr) {		/* we must have a text address */
		fprintf(out,
		    "add-symbol-file %s/%s/%s/%s%s 0x%llx",
		    cwd,
		    modules_path,
		    basetoken,
		    token[4],
		    debugname,
		    textaddr);
		if (dataaddr)
		    fprintf(out, " -s .data 0x%llx", dataaddr);
		if (bssaddr)
		    fprintf(out, " -s .bss 0x%llx", bssaddr);
		fprintf(out, "\n");
	    }
	}
    }
    return 0;
}
