/*
 * Copyright (c) 2000 Andrew Gallatin and David E. O'Brien
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <machine/sysarch.h>
#include <machine/proc.h>

#include <stdio.h>
#include <unistd.h>
#include <err.h>

extern int sysarch(int, char *);
static void usage ();

struct parms {
	u_int64_t uac;
};

int
alpha_setuac(u_int64_t uac)
{
	struct parms p;

	p.uac = uac;
	return (sysarch(ALPHA_SET_UAC, (char *)&p));
}

int
alpha_getuac(u_int64_t *uac)
{
	struct parms p;
	int error;

	error = sysarch(ALPHA_GET_UAC, (char *)&p);
	*uac = p.uac;
	return (error);
}

static void
print_uac(u_int64_t uac)
{

	printf("parent printing is ");
	if (uac & MDP_UAC_NOPRINT)
		printf("off\n");
	else 
		printf("on\n");

	printf("parent fixup is ");
	if (uac & MDP_UAC_NOFIX)
		printf("off\n");
	else 
		printf("on\n");

	printf("parent sigbus is ");
	if (uac & MDP_UAC_SIGBUS)
		printf("on \n");
	else 
		printf("off\n");
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	int c;
	u_int64_t uac;

	if (alpha_getuac(&uac) != 0)
		err(1, NULL);

	while ((c = getopt(argc, argv, "fpsr")) != -1) {
		switch (c) {
		case 'f':
			uac |= MDP_UAC_NOFIX;
			break;
		case 'p':
			uac |= MDP_UAC_NOPRINT;
			break;
		case 's':
			uac |= MDP_UAC_SIGBUS;
			break;
		case 'r':
			uac = 0;
			break;
		default:
			usage();
			/* NOTREACHED */   
		}
	}

	if (argc != 1) {	
		if (alpha_setuac(uac) != 0)
			err(1, NULL);
	}

	print_uac(uac);
	return 0;
}

static void
usage ()
{

	fprintf(stderr, "usage: uac [-fprs]\n");
}
