/*-
 * Copyright (c) 2017 Rick Macklem
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/extattr.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fs/nfs/nfsrvstate.h>

static void usage(void);
static void nfsrv_putfhname(fhandle_t *fhp, char *bufp);

/*
 * This program displays the location information of a data storage file
 * for a given file on a MetaData Server (MDS) in a pNFS service.  This program
 * must be run on the MDS and the file argument must be a file in a local
 * file system that has been exported for the pNFS service.
 */
int
main(int argc, char *argv[])
{
	fhandle_t fh;
	char buf[sizeof(fh) * 2 + 1], hostn[NI_MAXHOST + 1];
	struct pnfsdsfile dsfile;

	if (argc != 2)
		usage();

	/*
	 * The file's name is a hexadecimal representation of the MetaData
	 * Server's file handle.
	 */
	if (getfh(argv[1], &fh) < 0)
		err(1, "Getfh of %s failed", argv[1]);
	nfsrv_putfhname(&fh, buf);

	/*
	 * The host address and directory where the data storage file is
	 * located is in the extended attribute "pnfsd.dsfile".
	 */
	if (extattr_get_file(argv[1], EXTATTR_NAMESPACE_SYSTEM, "pnfsd.dsfile",
	    &dsfile, sizeof(dsfile)) != sizeof(dsfile))
		err(1, "Can't get extattr pnfsd.dsfile\n");

	/* Translate the IP address to a hostname. */
	if (getnameinfo((struct sockaddr *)&dsfile.dsf_sin,
	    dsfile.dsf_sin.sin_len, hostn, sizeof(hostn), NULL, 0, 0) < 0)
		err(1, "Can't get hostname\n");

	printf("%s\tds%d/%s\n", hostn, dsfile.dsf_dir, buf);
}

/*
 * Generate a file name based on the file handle and put it in *bufp.
 */
static void
nfsrv_putfhname(fhandle_t *fhp, char *bufp)
{
	int i;
	uint8_t *cp;

	cp = (uint8_t *)fhp;
	for (i = 0; i < sizeof(*fhp); i++)
		sprintf(&bufp[2 * i], "%02x", *cp++);
}

static void
usage(void)
{

	fprintf(stderr, "pnfsdsfile [filepath]\n");
	exit(1);
}

