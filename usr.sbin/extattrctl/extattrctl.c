/*-
 * Copyright (c) 1999-2001 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
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
 * $FreeBSD$
 */
/*
 * Developed by the TrustedBSD Project.
 * Support for file system extended attribute.
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/extattr.h>
#include <sys/param.h>
#include <sys/mount.h>

#include <ufs/ufs/extattr.h>

#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int initattr(int argc, char *argv[]);
long num_inodes_by_path(char *path);
void usage(void);

void
usage()
{

	fprintf(stderr,
	    "usage:\n"
	    "  extattrctl start [path]\n"
	    "  extattrctl stop [path]\n"
	    "  extattrctl initattr [-f] [-p path] [attrsize] [attrfile]\n"
	    "  extattrctl enable [path] [attrnamespace] [attrname] [attrfile]\n"
	    "  extattrctl disable [path] [attrnamespace] [attrname]\n");
	exit(-1);
}

long
num_inodes_by_path(char *path)
{
	struct statfs	buf;
	int	error;

	error = statfs(path, &buf);
	if (error) {
		perror("statfs");
		return (-1);
	}

	return (buf.f_files);
}

int
initattr(int argc, char *argv[])
{
	struct ufs_extattr_fileheader	uef;
	char	*fs_path = NULL;
	char	*zero_buf = NULL;
	long	loop, num_inodes;
	int	ch, i, error, chunksize, overwrite = 0, flags;

	optind = 0;
	while ((ch = getopt(argc, argv, "fp:r:w:")) != -1)
		switch (ch) {
		case 'f':
			overwrite = 1;
			break;
		case 'p':
			if ((fs_path = strdup(optarg)) == NULL) {
				perror("strdup");
				return(-1);
			}
			break;
		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	if (overwrite)
		flags = O_CREAT | O_WRONLY;
	else
		flags = O_CREAT | O_EXCL | O_WRONLY;

	error = 0;
	if ((i = open(argv[1], flags, 0600)) != -1) {
		uef.uef_magic = UFS_EXTATTR_MAGIC;
		uef.uef_version = UFS_EXTATTR_VERSION;
		uef.uef_size = atoi(argv[0]);
		if (write(i, &uef, sizeof(uef)) == -1)
			error = -1;
		else if (fs_path) {
			chunksize = sizeof(struct ufs_extattr_header) +
			    uef.uef_size;
			zero_buf = (char *) (malloc(chunksize));
			if (zero_buf == NULL) {
				perror("malloc");
				unlink(argv[1]);
				return (-1);
			}
			memset(zero_buf, 0, chunksize);
			num_inodes = num_inodes_by_path(fs_path);
			for (loop = 0; loop < num_inodes; loop++) {
				error = write(i, zero_buf, chunksize);
				if (error != chunksize) {
					perror("write");
					unlink(argv[1]);
					return (-1);
				}
			}
		}
	}
	if (i == -1) {
		/* unable to open file */
		perror(argv[1]);
		return (-1);
	}
	if (error == -1) {
		perror(argv[1]);
		unlink(argv[1]);
		return (-1);
	}

	return (0);
}

int
main(int argc, char *argv[])
{
	int	error = 0, attrnamespace;

	if (argc < 2)
		usage();

	if (!strcmp(argv[1], "start")) {
		if (argc != 3)
			usage();
		error = extattrctl(argv[2], UFS_EXTATTR_CMD_START, NULL, 0,
		    NULL);
		if (error) {
			perror("extattrctl start");
			return (-1);
		}
	} else if (!strcmp(argv[1], "stop")) {
		if (argc != 3)
			usage();
		error = extattrctl(argv[2], UFS_EXTATTR_CMD_STOP, NULL, 0,
		   NULL);
		if (error) {
			perror("extattrctl stop");
			return (-1);
		}
	} else if (!strcmp(argv[1], "enable")) {
		if (argc != 6)
			usage();
		error = extattr_string_to_namespace(argv[3], &attrnamespace);
		if (error) {
			perror("extattrctl enable");
			return (-1);
		}
		error = extattrctl(argv[2], UFS_EXTATTR_CMD_ENABLE, argv[5],
		    attrnamespace, argv[4]);
		if (error) {
			perror("extattrctl enable");
			return (-1);
		}
	} else if (!strcmp(argv[1], "disable")) {
		if (argc != 5)
			usage();
		error = extattr_string_to_namespace(argv[3], &attrnamespace);
		if (error) {
			perror("extattrctl disable");
			return (-1);
		}
		error = extattrctl(argv[2], UFS_EXTATTR_CMD_DISABLE, NULL,
		    attrnamespace, argv[4]);
		if (error) {
			perror("extattrctl disable");
			return (-1);
		}
	} else if (!strcmp(argv[1], "initattr")) {
		argc -= 2;
		argv += 2;
		error = initattr(argc, argv);
		if (error)
			return (-1);
	} else
		usage();

	return (0);
}
