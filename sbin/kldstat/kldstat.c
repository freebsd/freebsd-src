/*-
 * Copyright (c) 1997 Doug Rabson
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/sbin/kldstat/kldstat.c,v 1.7.2.2 2000/07/01 07:56:00 ps Exp $";
#endif /* not lint */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/module.h>
#include <sys/linker.h>

#if defined(__alpha__)
#define	POINTER_WIDTH	18
#else
#define	POINTER_WIDTH	10
#endif

static void
printmod(int modid)
{
    struct module_stat stat;

    stat.version = sizeof(struct module_stat);
    if (modstat(modid, &stat) < 0)
	warn("can't stat module id %d", modid);
    else
	printf("\t\t%2d %s\n", stat.id, stat.name);
}

static void printfile(int fileid, int verbose)
{
    struct kld_file_stat stat;
    int modid;

    stat.version = sizeof(struct kld_file_stat);
    if (kldstat(fileid, &stat) < 0)
	warn("can't stat file id %d", fileid);
    else
	printf("%2d %4d %p %-8x %s\n",
	       stat.id, stat.refs, stat.address, stat.size, stat.name);

    if (verbose) {
	printf("\tContains modules:\n");
	printf("\t\tId Name\n");
	for (modid = kldfirstmod(fileid); modid > 0;
	     modid = modfnext(modid))
	    printmod(modid);
    }
}

static void
usage(void)
{
    fprintf(stderr, "usage: kldstat [-v] [-i id] [-n name]\n");
    exit(1);
}

int
main(int argc, char** argv)
{
    int c;
    int verbose = 0;
    int fileid = 0;
    char* filename = 0;

    while ((c = getopt(argc, argv, "i:n:v")) != -1)
	switch (c) {
	case 'i':
	    fileid = atoi(optarg);
	    break;
	case 'n':
	    filename = optarg;
	    break;
	case 'v':
	    verbose = 1;
	    break;
	default:
	    usage();
	}
    argc -= optind;
    argv += optind;

    if (argc != 0)
	usage();

    if (filename) {
	if ((fileid = kldfind(filename)) < 0)
	    err(1, "can't find file %s", filename);
    }

    printf("Id Refs Address%*c Size     Name\n", POINTER_WIDTH - 7, ' ');
    if (fileid)
	printfile(fileid, verbose);
    else
	for (fileid = kldnext(0); fileid > 0; fileid = kldnext(fileid))
	    printfile(fileid, verbose);

    return 0;
}
