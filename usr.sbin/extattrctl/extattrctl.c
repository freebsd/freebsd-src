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
#include <sys/param.h>
#include <sys/mount.h>

#include <ufs/ufs/extattr.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char	*optarg;
extern int	optind;

void
usage(void)
{

	fprintf(stderr,
	    "usage:\n"
	    "  extattrctl start [path]\n"
	    "  extattrctl stop [path]\n"
	    "  extattrctl initattr [-p path] [-r [kroa]] [-w [kroa]] "
	    "[attrsize] [attrfile]\n"
	    "  extattrctl enable [path] [attrname] [attrfile]\n"
	    "  extattrctl disable [path] [attrname]\n");
	exit(-1);
}

/*
 * Return a level, or -1
 */
int
extattr_level_from_string(char *string)
{

	if (strlen(string) != 1)
		return (-1);

	switch(string[0]) {
	case 'k':
	case 'K':
		return (UFS_EXTATTR_PERM_KERNEL);
	case 'r':
	case 'R':
		return (UFS_EXTATTR_PERM_ROOT);
	case 'o':
	case 'O':
		return (UFS_EXTATTR_PERM_OWNER);
	case 'a':
	case 'A':
		return (UFS_EXTATTR_PERM_ANYONE);
	default:
		return (-1);
	}
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
	int	initattr_rlevel = UFS_EXTATTR_PERM_OWNER;
	int	initattr_wlevel = UFS_EXTATTR_PERM_OWNER;
	int	ch, i, error;

	optind = 0;
	while ((ch = getopt(argc, argv, "p:rw")) != -1)
		switch (ch) {
		case 'p':
			fs_path = strdup(optarg);
			break;
		case 'r':
			initattr_rlevel = extattr_level_from_string(optarg);
			if (initattr_rlevel == -1)
				usage();
			break;
		case 'w':
			initattr_wlevel = extattr_level_from_string(optarg);
			if (initattr_wlevel == -1)
				usage();
			break;
		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	error = 0;
	if ((i = open(argv[1], O_CREAT | O_EXCL | O_WRONLY, 0600)) != -1) {
		uef.uef_magic = UFS_EXTATTR_MAGIC;
		uef.uef_version = UFS_EXTATTR_VERSION;
		uef.uef_write_perm = initattr_wlevel;
		uef.uef_read_perm = initattr_rlevel;
		uef.uef_size = atoi(argv[0]);
		if (write(i, &uef, sizeof(uef)) == -1)
			error = -1;
		else if (fs_path) {
			zero_buf = (char *) (malloc(uef.uef_size));
			if (zero_buf == NULL) {
				perror("malloc");
				unlink(argv[1]);
				return (-1);
			}
			memset(zero_buf, 0, uef.uef_size);
			num_inodes = num_inodes_by_path(fs_path);
			for (loop = 0; loop < num_inodes; loop++) {
				error = write(i, zero_buf, uef.uef_size);
				if (error != uef.uef_size) {
					perror("write");
					unlink(argv[1]);
					return (-1);
				}
			}
		}
	}
	if (i == -1 || error == -1) {
		perror(argv[1]);
		unlink(argv[1]);
		return (-1);
	}

	return (0);
}

int
main(int argc, char *argv[])
{
	int	error = 0;

	if (argc < 2)
		usage();

	if (!strcmp(argv[1], "start")) {
		if (argc != 3)
			usage();
		error = extattrctl(argv[2], UFS_EXTATTR_CMD_START, 0, 0);
	} else if (!strcmp(argv[1], "stop")) {
		if (argc != 3)
			usage();
		error = extattrctl(argv[2], UFS_EXTATTR_CMD_STOP, 0, 0);
	} else if (!strcmp(argv[1], "enable")) {
		if (argc != 5)
			usage();
		error = extattrctl(argv[2], UFS_EXTATTR_CMD_ENABLE, argv[3],
		    argv[4]);
	} else if (!strcmp(argv[1], "disable")) {
		if (argc != 4)
			usage();
		error = extattrctl(argv[2], UFS_EXTATTR_CMD_DISABLE, argv[3],
		    NULL);
	} else if (!strcmp(argv[1], "initattr")) {
		argc -= 2;
		argv += 2;
		error = initattr(argc, argv);
	} else
		usage();

	if (error)
		perror(argv[1]);

	return(error);
}
