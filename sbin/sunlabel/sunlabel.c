/*-
 * Copyright (c) 2003 Jake Burkholder.
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
/*
 * Copyright (c) 1994, 1995 Gordon W. Ross
 * Copyright (c) 1994 Theo de Raadt
 * All rights reserved.
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Symmetric Computer Systems.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *      This product includes software developed by Theo de Raadt.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: $NetBSD: disksubr.c,v 1.13 2000/12/17 22:39:18 pk $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sun_disklabel.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	_PATH_TMPFILE	"/tmp/EdDk.XXXXXXXXXX"
#define	_PATH_BOOT	"/boot/boot1"

static int bflag;
static int Bflag;
static int eflag;
static int nflag;
static int rflag = 1;
static int Rflag;
static int wflag;

static int check_label(struct sun_disklabel *sl);
static void read_label(struct sun_disklabel *sl, const char *disk);
static void write_label(struct sun_disklabel *sl, const char *disk,
    const char *bootpath);
static int edit_label(struct sun_disklabel *sl, const char *disk,
    const char *bootpath);
static int parse_label(struct sun_disklabel *sl, const char *file);
static void print_label(struct sun_disklabel *sl, const char *disk, FILE *out);

static int parse_size(struct sun_disklabel *sl, int part, char *size);
static int parse_offset(struct sun_disklabel *sl, int part, char *offset);

static void usage(void);

extern char *__progname;

/*
 * Disk label editor for sun disklabels.
 */
int
main(int ac, char **av)
{
	struct sun_disklabel sl;
	const char *bootpath;
	const char *proto;
	const char *disk;
	int ch;

	bootpath = _PATH_BOOT; 
	while ((ch = getopt(ac, av, "b:BenrRw")) != -1)
		switch (ch) {
		case 'b':
			bflag = 1;
			bootpath = optarg;
			break;
		case 'B':
			Bflag = 1;
			break;
		case 'e':
			eflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'w':
			wflag = 1;
			break;
		default:
			usage();
			break;
		}
	if (bflag && !Bflag)
		usage();
	if (nflag && !(Bflag || eflag || Rflag || wflag))
		usage();
	if (eflag && (Rflag || wflag))
		usage();
	ac -= optind;
	av += optind;
	if (ac == 0)
		usage();
	bzero(&sl, sizeof(sl));
	disk = av[0];
	if (wflag) {
		if (ac != 2 || strcmp(av[1], "auto") != 0)
			usage();
		read_label(&sl, disk);
		bzero(sl.sl_part, sizeof(sl.sl_part));
		sl.sl_part[2].sdkp_cyloffset = 0;
		sl.sl_part[2].sdkp_nsectors = sl.sl_ncylinders *
		    sl.sl_ntracks * sl.sl_nsectors;
		write_label(&sl, disk, bootpath);
	} else if (eflag) {
		if (ac != 1)
			usage();
		read_label(&sl, disk);
		if (sl.sl_magic != SUN_DKMAGIC)
			errx(1, "%s%s has no sun disklabel", _PATH_DEV, disk);
		while (edit_label(&sl, disk, bootpath) != 0)
			;
	} else if (Rflag) {
		if (ac != 2)
			usage();
		proto = av[1];
		read_label(&sl, disk);
		write_label(&sl, disk, bootpath);
	} else if (Bflag) {
		read_label(&sl, disk);
		if (sl.sl_magic != SUN_DKMAGIC)
			errx(1, "%s%s has no sun disklabel", _PATH_DEV, disk);
		write_label(&sl, disk, bootpath);
	} else {
		read_label(&sl, disk);
		if (sl.sl_magic != SUN_DKMAGIC)
			errx(1, "%s%s has no sun disklabel", _PATH_DEV, disk);
		print_label(&sl, disk, stdout);
	}
	return (0);
}

static int
check_label(struct sun_disklabel *sl)
{
	uint64_t nsectors;
	uint64_t ostart;
	uint64_t start;
	uint64_t oend;
	uint64_t end;
	int i;
	int j;

	nsectors = sl->sl_ncylinders * sl->sl_ntracks * sl->sl_nsectors;
	for (i = 0; i < 8; i++) {
		if (i == 2 || sl->sl_part[i].sdkp_nsectors == 0)
			continue;
		start = (uint64_t)sl->sl_part[i].sdkp_cyloffset *
		    sl->sl_ntracks * sl->sl_nsectors;
		end = start + sl->sl_part[i].sdkp_nsectors;
		if (end > nsectors) {
			warnx("partition %c extends past end of disk",
			    'a' + i);
			return (1);
		}
		for (j = 0; j < 8; j++) {
			if (j == 2 || j == i ||
			    sl->sl_part[j].sdkp_nsectors == 0)
				continue;
			ostart = (uint64_t)sl->sl_part[j].sdkp_cyloffset *
			    sl->sl_ntracks * sl->sl_nsectors;
			oend = ostart + sl->sl_part[j].sdkp_nsectors;
			if ((start <= ostart && end >= oend) ||
			    (start > ostart && start < oend) ||
			    (end > ostart && end < oend)) {
				warnx("partition %c overlaps partition %c",
				    'a' + i, 'a' + j);
				return (1);
			}
		}
	}
	return (0);
}

static void
read_label(struct sun_disklabel *sl, const char *disk)
{
	uint32_t bytepercyl;
	uint32_t bytepersec;
	uint32_t acylinders;
	uint32_t nsectors;
	uint32_t ntracks;
	uintmax_t offset;
	uintmax_t size;
	char name[64];
	char *text;
	char *p, *s;
	size_t len;
	int i;
	int n;

	if (sysctlbyname("kern.geom.conftxt", NULL, &len, NULL, 0) < 0)
		err(1, "sysctlbyname");
	if ((text = malloc(len)) == NULL)
		err(1, "malloc");
	if (sysctlbyname("kern.geom.conftxt", text, &len, NULL, 0) < 0)
		err(1, "sysctlbyname");
	bytepercyl = 0;
	for (p = text; (s = strsep(&p, "\n")) != NULL;) {
		n = sscanf(s, "%*u DISK %s %ju %u hd %u sc %u\n", name, &size,
		    &bytepersec, &ntracks, &nsectors);
		if (n == 5) {
			if (strncmp(name, disk, strlen(disk)) != 0)
				continue;
			bytepercyl = ntracks * nsectors * bytepersec;
			sl->sl_pcylinders = size / bytepercyl;
			sl->sl_acylinders = 2;
			sl->sl_nsectors = nsectors;
			sl->sl_ntracks = ntracks;
			continue;
		}
		n = sscanf(s,
		    "%*u SUN %s %ju %*u i %u o %ju sc %*u hd %*u alt %u",
		    name, &size, &i, &offset, &acylinders);
		if (n == 5) {
			if (strncmp(name, disk, strlen(disk)) != 0)
				continue;
			sl->sl_acylinders = acylinders;
			sl->sl_part[i].sdkp_cyloffset = offset / bytepercyl;
			sl->sl_part[i].sdkp_nsectors = size / bytepersec;
			sl->sl_magic = SUN_DKMAGIC;
			continue;
		}
	}
	if (bytepercyl == 0)
		errx(1, "disk %s not found", disk);
	sl->sl_ncylinders = sl->sl_pcylinders -
	    sl->sl_acylinders;
	sl->sl_interleave = 1;
	sl->sl_sparespercyl = 0;
	sl->sl_rpm = 3600;
	size = (uint64_t)sl->sl_ncylinders * sl->sl_ntracks * sl->sl_nsectors;
	if (size > 4999 * 1024 * 2) {
		sprintf(sl->sl_text, "FreeBSD%luG cyl %u alt %u hd %u sec %u",
		    (size + 1024 * 1024) / (2 * 1024 * 1024),
		    sl->sl_ncylinders, sl->sl_acylinders,
		    sl->sl_ntracks, sl->sl_nsectors);
	} else {
		sprintf(sl->sl_text, "FreeBSD%luM cyl %u alt %u hd %u sec %u",
		    (size + 1024) / (2 * 1024),
		    sl->sl_ncylinders, sl->sl_acylinders,
		    sl->sl_ntracks, sl->sl_nsectors);
	}
}

static void
write_label(struct sun_disklabel *sl, const char *disk, const char *bootpath)
{
	char path[MAXPATHLEN];
	char boot[16 * 512];
	uint16_t cksum;
	uint16_t *sp1;
	uint16_t *sp2;
	off_t off;
	int bfd;
	int fd;
	int i;

	sl->sl_magic = SUN_DKMAGIC;

	sp1 = (u_short *)sl;
	sp2 = (u_short *)(sl + 1);
	cksum = 0;
	while (sp1 < sp2)
		cksum ^= *sp1++;
	sl->sl_cksum = cksum;

	if (check_label(sl) != 0)
		errx(1, "invalid label");

	if (nflag) {
		print_label(sl, disk, stdout);
	} else if (rflag) {
		snprintf(path, sizeof(path), "%s%s", _PATH_DEV, disk);
		if ((fd = open(path, O_RDWR)) < 0)
			err(1, "open %s", path);
		if (Bflag) {
			if ((bfd = open(bootpath, O_RDONLY)) < 0)
				err(1, "open %s", bootpath);
			if (read(bfd, boot, sizeof(boot)) != sizeof(boot))
				err(1, "read");
			close(bfd);
			for (i = 0; i < 8; i++) {
				if (sl->sl_part[i].sdkp_nsectors == 0)
					continue;
				off = sl->sl_part[i].sdkp_cyloffset *
				    sl->sl_ntracks * sl->sl_nsectors * 512;
				if (lseek(fd, off, SEEK_SET) < 0)
					err(1, "lseek");
				if (write(fd, boot, sizeof(boot)) !=
				    sizeof(boot))
					err(1, "write");
			}
		}
		if (lseek(fd, 0, SEEK_SET) < 0)
			err(1, "lseek");
		if (write(fd, sl, sizeof(*sl)) != sizeof(*sl))
			err(1, "write");
		close(fd);
	} else
		err(1, "implement!");
}

static int
edit_label(struct sun_disklabel *sl, const char *disk, const char *bootpath)
{
	char tmpfil[] = _PATH_TMPFILE;
	const char *editor;
	int status;
	FILE *fp;
	pid_t pid;
	pid_t r;
	int fd;
	int c;

	if ((fd = mkstemp(tmpfil)) < 0)
		err(1, "mkstemp");
	if ((fp = fdopen(fd, "w")) == NULL)
		err(1, "fdopen");
	print_label(sl, disk, fp);
	fflush(fp);
	if ((pid = fork()) < 0)
		err(1, "fork");
	if (pid == 0) {
		if ((editor = getenv("EDITOR")) == NULL)
			editor = _PATH_VI;
		execlp(editor, editor, tmpfil, NULL);
		err(1, "execlp %s", editor);
	}
	status = 0;
	while ((r = wait(&status)) > 0 && r != pid)
		;
	if (WIFEXITED(status)) {
		if (parse_label(sl, tmpfil) == 0) {
			fclose(fp);
			unlink(tmpfil);
			write_label(sl, disk, bootpath);
			return (0);
		}
		printf("re-edit the label? [y]: ");
		fflush(stdout);
		c = getchar();
		if (c != EOF && c != '\n')
			while (getchar() != '\n')
				;
		if  (c == 'n') {
			fclose(fp);
			unlink(tmpfil);
			return (0);
		}
	}
	fclose(fp);
	unlink(tmpfil);
	return (1);
}

static int
parse_label(struct sun_disklabel *sl, const char *file)
{
	char offset[32];
	char size[32];
	char buf[128];
	uint8_t part;
	FILE *fp;
	int line;

	line = 0;
	if ((fp = fopen(file, "r")) == NULL)
		err(1, "fopen");
	bzero(sl->sl_part, sizeof(sl->sl_part));
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (buf[0] != ' ' || buf[1] != ' ')
			continue;
		if (sscanf(buf, "  %c: %s %s\n", &part, size, offset) != 3 ||
		    parse_size(sl, part - 'a', size) ||
		    parse_offset(sl, part - 'a', offset)) {
			warnx("%s: syntex error on line %d",
			    file, line);
			fclose(fp);
			return (1);
		}
		line++;
	}
	fclose(fp);
	sl->sl_part[2].sdkp_cyloffset = 0;
	sl->sl_part[2].sdkp_nsectors = sl->sl_ncylinders *
	    sl->sl_ntracks * sl->sl_nsectors;
	return (check_label(sl));
}

static int
parse_size(struct sun_disklabel *sl, int part, char *size)
{
	uintmax_t nsectors;
	uintmax_t total;
	uintmax_t n;
	char *p;
	int i;

	nsectors = 0;
	n = strtoumax(size, &p, 10);
	if (*p != '\0') {
		if (strcmp(size, "*") == 0) {
			total = sl->sl_ncylinders * sl->sl_ntracks *
			    sl->sl_nsectors;
			for (i = 0; i < part; i++) {
				if (i == 2)
					continue;
				nsectors += sl->sl_part[i].sdkp_nsectors;
			}
			n = total - nsectors;
		} else if (p[1] == '\0' && (p[0] == 'K' || p[0] == 'k')) {
			n = roundup((n * 1024) / 512,
			    sl->sl_ntracks * sl->sl_nsectors);
		} else if (p[1] == '\0' && (p[0] == 'M' || p[0] == 'm')) {
			n = roundup((n * 1024 * 1024) / 512,
			    sl->sl_ntracks * sl->sl_nsectors);
		} else if (p[1] == '\0' && (p[0] == 'G' || p[0] == 'g')) {
			n = roundup((n * 1024 * 1024 * 1024) / 512,
			    sl->sl_ntracks * sl->sl_nsectors);
		} else
			return (-1);
	}
	sl->sl_part[part].sdkp_nsectors = n;
	return (0);
}

static int
parse_offset(struct sun_disklabel *sl, int part, char *offset)
{
	uintmax_t nsectors;
	uintmax_t n;
	char *p;
	int i;

	nsectors = 0;
	n = strtoumax(offset, &p, 10);
	if (*p != '\0') {
		if (strcmp(offset, "*") == 0) {
			for (i = 0; i < part; i++) {
				if (i == 2)
					continue;
				nsectors += sl->sl_part[i].sdkp_nsectors;
			}
			n = nsectors / (sl->sl_nsectors * sl->sl_ntracks);
		} else
			return (-1);
	}
	sl->sl_part[part].sdkp_cyloffset = n;
	return (0);
}

static void
print_label(struct sun_disklabel *sl, const char *disk, FILE *out)
{
	int i;

	fprintf(out,
"# /dev/%s:\n"
"text: %s\n"
"bytes/sectors: 512\n"
"sectors/cylinder: %d\n"
"sectors/unit: %d\n"
"\n"
"8 partitions:\n"
"#\n"
"# Size is in sectors, use %%dK, %%dM or %%dG to specify in kilobytes,\n"
"# megabytes or gigabytes respectively, or '*' to specify rest of disk.\n"
"# Offset is in cylinders, use '*' to calculate offsets automatically.\n"
"#\n"
"#    size       offset\n"
"#    ---------- ----------\n",
	    disk,
	    sl->sl_text,
	    sl->sl_nsectors * sl->sl_ntracks,
	    sl->sl_nsectors * sl->sl_ntracks * sl->sl_ncylinders);
	for (i = 0; i < 8; i++) {
		if (sl->sl_part[i].sdkp_nsectors == 0)
			continue;
		fprintf(out, "  %c: %10u %10u\n",
		    'a' + i,	
		    sl->sl_part[i].sdkp_nsectors,
		    sl->sl_part[i].sdkp_cyloffset);
	}
}

static void
usage(void)
{

	fprintf(stderr, "usage:"
"\t%s [-r] disk\n"
"\t\t(to read label)\n"
"\t%s -B [-b boot1] [-n] disk\n"
"\t\t(to install boot program only)\n"
"\t%s -R [-B [-b boot1]] [-r] [-n] disk protofile\n"
"\t\t(to restore label)\n"
"\t%s -e [-B [-b boot1]] [-r] [-n] disk\n"
"\t\t(to edit label)\n"
"\t%s -w [-B [-b boot1]] [-r] [-n] disk type\n"
"\t\t(to write default label)\n",
	     __progname,
	     __progname,
	     __progname,
	     __progname,
	     __progname);
	exit(1);
}
