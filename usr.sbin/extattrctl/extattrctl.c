/*-
 * Copyright (c) 1999, 2000 Robert N. M. Watson
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
 *	$FreeBSD$
 */
/*
 * TrustedBSD Project - extended attribute support for UFS-like file systems
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/extattr.h>
#include <ufs/ufs/extattr.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void
usage(void)
{

	fprintf(stderr,
	    "usage:\n"
	    "  extattrctl start [path]\n"
	    "  extattrctl stop [path]\n"
	    "  extattrctl initattr [attrsize] [attrfile]\n"
	    "  extattrctl enable [path] [attrname] [attrfile]\n"
	    "  extattrctl disable [path] [attrname]\n");
}

int
main(int argc, char *argv[])
{
	struct ufs_extattr_fileheader	uef;
	int	error = 0, i;

	if (argc < 2) {
		usage();
		return(-1);
	}

	if (!strcmp(argv[1], "start")) {
		if (argc != 3) {
			usage();
			return(-1);
		}
		error = extattrctl(argv[2], UFS_EXTATTR_CMD_START, 0, 0);
	} else if (!strcmp(argv[1], "stop")) {
		if (argc != 3) {
			usage();
			return(-1);
		}
		error = extattrctl(argv[2], UFS_EXTATTR_CMD_STOP, 0, 0);
	} else if (!strcmp(argv[1], "enable")) {
		if (argc != 5) {
			usage();
			return(-1);
		}
		error = extattrctl(argv[2], UFS_EXTATTR_CMD_ENABLE, argv[3],
		    argv[4]);
	} else if (!strcmp(argv[1], "disable")) {
		if (argc != 4) {
			usage();
			return(-1);
		}
		error = extattrctl(argv[2], UFS_EXTATTR_CMD_DISABLE, argv[3],
		    NULL);
	} else if (!strcmp(argv[1], "initattr")) {
		if (argc != 4) {
			usage();
			return(-1);
		}
		if ((i = open(argv[3], O_CREAT | O_EXCL | O_WRONLY, 0600)) !=
		    -1) {
			uef.uef_write_perm = UFS_EXTATTR_PERM_OWNER;
			uef.uef_read_perm = UFS_EXTATTR_PERM_ANYONE;
			uef.uef_size = atoi(argv[2]);
			if (write(i, &uef, sizeof(uef)) == -1) {
				error = -1;
			} else
				error = close(i);
		} else 
			error = -1;
	} else {
		usage();
		return(-1);
	}

	if (error)
		perror(argv[1]);

	return(error);
}
