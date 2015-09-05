/*-
 * Copyright (c) 2015 Baptiste Daroussin <bapt@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/ioctl.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_enc.h>

static int locate(int argc, char **argv);

static struct command {
	const char *name;
	const char *desc;
	int (*exec)(int argc, char **argv);
} cmds[] = {
	{ "locate", "Change the state of the external LED associated with a"
	    " disk", locate} ,
};

static const int nbcmds = nitems(cmds);

static void
do_locate(int fd, unsigned int idx, bool onoff)
{
	encioc_elm_status_t o;

	o.elm_idx = idx;
	if (ioctl(fd, ENCIOC_GETELMSTAT, (caddr_t) &o) < 0) {
		close(fd);
		err(EXIT_FAILURE, "ENCIOC_GETELMSTAT");
	}
	o.cstat[0] |= 0x80;
	if (onoff)
		o.cstat[2] |= 0x02;
	else
		o.cstat[2] &= 0xfd;

	if (ioctl(fd, ENCIOC_SETELMSTAT, (caddr_t) &o) < 0) {
		close(fd);
		err(EXIT_FAILURE, "ENCIOC_SETELMSTAT");
	}
}

static bool
disk_match(const char *devnames, const char *disk, size_t len)
{
	const char *devname;

	devname = devnames;
	while ((devname = strstr(devname, disk)) != NULL) {
		if (devname[len] == '\0' || devname[len] == ',')
			return (true);
		devname++;
	}
	return (false);
}

static int
locate(int argc, char **argv)
{
	encioc_elm_devnames_t objdn;
	encioc_element_t *objp;
	glob_t g;
	char *disk;
	size_t len, i;
	int fd, nobj, j;
	bool all = false;
	bool onoff;

	if (argc != 2) {
		errx(EXIT_FAILURE, "usage: %s locate [disk] [on|off]",
		    getprogname());
	}

	disk = argv[0];

	if (strcmp(argv[1], "on") == 0) {
		onoff = true;
	} else if (strcmp(argv[1], "off") == 0) {
		onoff = false;
	} else {
		errx(EXIT_FAILURE, "usage: %s locate [disk] [on|off]",
		    getprogname());
	}

	if (strcmp(disk, "all") == 0) {
		all = true;
	}
	len = strlen(disk);

	/* Get the list of ses devices */
	if (glob("/dev/ses[0-9]*", 0, NULL, &g) == GLOB_NOMATCH) {
		globfree(&g);
		errx(EXIT_FAILURE, "No SES devices found");
	}
	for (i = 0; i < g.gl_pathc; i++) {
		/* ensure we only got numbers after ses */
		if (strspn(g.gl_pathv[i] + 8, "0123456789") !=
		    strlen(g.gl_pathv[i] + 8))
			continue;
		if ((fd = open(g.gl_pathv[i], O_RDWR)) < 0) {
			if (errno == EACCES)
				err(EXIT_FAILURE, "enable to access SES device");
			break;
		}

		if (ioctl(fd, ENCIOC_GETNELM, (caddr_t) &nobj) < 0)
			err(EXIT_FAILURE, "ENCIOC_GETNELM");

		objp = calloc(nobj, sizeof(encioc_element_t));
		if (objp == NULL)
			err(EXIT_FAILURE, "calloc()");

		if (ioctl(fd, ENCIOC_GETELMMAP, (caddr_t) objp) < 0)
			err(EXIT_FAILURE, "ENCIOC_GETELMMAP");

		for (j = 0; j < nobj; j++) {
			memset(&objdn, 0, sizeof(objdn));
			objdn.elm_idx = objp[j].elm_idx;
			objdn.elm_names_size = 128;
			objdn.elm_devnames = calloc(128, sizeof(char));
			if (objdn.elm_devnames == NULL)
				err(EXIT_FAILURE, "calloc()");
			if (ioctl(fd, ENCIOC_GETELMDEVNAMES,
			    (caddr_t) &objdn) <0)
				continue;
			if (objdn.elm_names_len > 0) {
				if (all) {
					do_locate(fd, objdn.elm_idx, onoff);
					continue;
				}
				if (disk_match(objdn.elm_devnames, disk, len)) {
					do_locate(fd, objdn.elm_idx, onoff);
					break;
				}
			}
		}	
		close(fd);
		i++;
	}
	globfree(&g);

	return (EXIT_SUCCESS);
}

static void
usage(FILE *out)
{
	int i;

	fprintf(out, "Usage: %s [command] [options]\n", getprogname());
	fprintf(out, "Commands supported:\n");
	for (i = 0; i < nbcmds; i++)
		fprintf(out, "\t%-15s%s\n", cmds[i].name, cmds[i].desc);
}

int
main(int argc, char **argv)
{
	int i;
	struct command *cmd = NULL;

	if (argc < 2) {
		warnx("Missing command");
		usage(stderr);
		return (EXIT_FAILURE);
	}

	for (i = 0; i < nbcmds; i++) {
		if (strcmp(argv[1], cmds[i].name) == 0) {
			cmd = &cmds[i];
			break;
		}
	}

	if (cmd == NULL) {
		warnx("unknown command %s", argv[1]);
		usage(stderr);
		return (EXIT_FAILURE);
	}

	argc-=2;
	argv+=2;

	return (cmd->exec(argc, argv));
}
