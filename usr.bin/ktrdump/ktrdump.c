/*-
 * Copyright (c) 2002 Jake Burkholder
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/ktr.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	SBUFLEN	128
#define	USAGE \
	"usage: ktrdump [-c] [-f] [-t] [-e execfile] [-i ktrfile ] [-m corefile] [-o outfile]"

extern char *optarg;
extern int optind;

static void usage(void);

static struct nlist nl[] = {
	{ "_ktr_version" },
	{ "_ktr_entries" },
	{ "_ktr_idx" },
	{ "_ktr_buf" },
	{ NULL }
};

static int cflag;
static int eflag;
static int fflag;
static int mflag;
static int tflag;
static int iflag;

static char corefile[PATH_MAX];
static char execfile[PATH_MAX];

static char desc[SBUFLEN];
static char errbuf[_POSIX2_LINE_MAX];
static char fbuf[PATH_MAX];
static char obuf[PATH_MAX];
static char sbuf[KTR_PARMS][SBUFLEN];

/*
 * Reads the ktr trace buffer from kernel memory and prints the trace entries.
 */
int
main(int ac, char **av)
{
	u_long parms[KTR_PARMS];
	struct ktr_entry *buf;
	struct stat sb;
	kvm_t *kd;
	FILE *out;
	char *p;
	int version;
	int entries;
	int index;
	int parm;
	int in;
	int c;
	int i;
	int n;

	/*
	 * Parse commandline arguments.
	 */
	out = stdout;
	while ((c = getopt(ac, av, "cfte:i:m:o:")) != -1)
		switch (c) {
		case 'c':
			cflag = 1;
			break;
		case 'e':
			if (strlcpy(execfile, optarg, sizeof(execfile))
			    >= sizeof(execfile))
				errx(1, "%s: File name too long", optarg);
			eflag = 1;
			break;
		case 'f':
			fflag = 1;
			break;
		case 'i':
			iflag = 1;
			if ((in = open(optarg, O_RDONLY)) == -1)
				err(1, "%s", optarg);
			break;
		case 'm':
			if (strlcpy(corefile, optarg, sizeof(corefile))
			    >= sizeof(corefile))
				errx(1, "%s: File name too long", optarg);
			mflag = 1;
			break;
		case 'o':
			if ((out = fopen(optarg, "w")) == NULL)
				err(1, "%s", optarg);
			break;
		case 't':
			tflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	ac -= optind;
	av += optind;
	if (ac != 0)
		usage();

	/*
	 * Open our execfile and corefile, resolve needed symbols and read in
	 * the trace buffer.
	 */
	if ((kd = kvm_openfiles(eflag ? execfile : NULL,
	    mflag ? corefile : NULL, NULL, O_RDONLY, errbuf)) == NULL)
		errx(1, "%s", errbuf);
	if (kvm_nlist(kd, nl) != 0 ||
	    kvm_read(kd, nl[0].n_value, &version, sizeof(version)) == -1)
		errx(1, "%s", kvm_geterr(kd));
	if (version != KTR_VERSION)
		errx(1, "ktr version mismatch");
	if (iflag) {
		if (fstat(in, &sb) == -1)
			errx(1, "stat");
		entries = sb.st_size / sizeof(*buf);
		index = 0;
		buf = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, in, 0);
		if (buf == MAP_FAILED)
			errx(1, "mmap");
	} else {
		if (kvm_read(kd, nl[1].n_value, &entries, sizeof(entries))
		    == -1)
			errx(1, "%s", kvm_geterr(kd));
		if ((buf = malloc(sizeof(*buf) * entries)) == NULL)
			err(1, NULL);
		if (kvm_read(kd, nl[2].n_value, &index, sizeof(index)) == -1 ||
		    kvm_read(kd, nl[3].n_value, buf, sizeof(*buf) * entries)
		    == -1)
			errx(1, "%s", kvm_geterr(kd));
	}

	/*
	 * Print a nice header.
	 */
	fprintf(out, "%-6s ", "index");
	if (cflag)
		fprintf(out, "%-3s ", "cpu");
	if (tflag)
		fprintf(out, "%-16s ", "timestamp");
	if (fflag)
		fprintf(out, "%-32s ", "file and line");
	fprintf(out, "%s", "trace");
	fprintf(out, "\n");

	fprintf(out, "------ ");
	if (cflag)
		fprintf(out, "--- ");
	if (tflag)
		fprintf(out, "---------------- ");
	if (fflag)
		fprintf(out, "---------------------------------------- ");
	fprintf(out, "----- ");
	fprintf(out, "\n");

	/*
	 * Now tear through the trace buffer.
	 */
	if (!iflag)
		i = (index - 1) & (entries - 1);
	for (;;) {
		if (buf[i].ktr_desc == NULL)
			break;
		if (kvm_read(kd, (u_long)buf[i].ktr_desc, desc,
		    sizeof(desc)) == -1)
			errx(1, "%s", kvm_geterr(kd));
		desc[sizeof(desc) - 1] = '\0';
		parm = 0;
		for (p = desc; (c = *p++) != '\0';) {
			if (c != '%')
				continue;
			if ((c = *p++) == '\0')
				break;
			if (parm == KTR_PARMS)
				errx(1, "too many parameters");
			switch (c) {
			case 's':
				if (kvm_read(kd, (u_long)buf[i].ktr_parms[parm],
				    sbuf[parm], sizeof(sbuf[parm])) == -1)
					strcpy(sbuf[parm], "(null)");
				sbuf[parm][sizeof(sbuf[0]) - 1] = '\0';
				parms[parm] = (u_long)sbuf[parm];
				parm++;
				break;
			default:
				parms[parm] = buf[i].ktr_parms[parm];
				parm++;
				break;
			}
		}
		fprintf(out, "%6d ", i);
		if (cflag)
			fprintf(out, "%3d ", buf[i].ktr_cpu);
		if (tflag)
			fprintf(out, "%16ju ",
			    (uintmax_t)buf[i].ktr_timestamp);
		if (fflag) {
			if (kvm_read(kd, (u_long)buf[i].ktr_file, fbuf,
			    sizeof(fbuf)) == -1)
				strcpy(fbuf, "(null)");
			snprintf(obuf, sizeof(obuf), "%s:%d", fbuf,
			    buf[i].ktr_line);
			fprintf(out, "%-40s ", obuf);
		}
		fprintf(out, desc, parms[0], parms[1], parms[2], parms[3],
		    parms[4], parms[5]);
		fprintf(out, "\n");
		if (!iflag) {
			if (i == index)
				break;
			i = (i - 1) & (entries - 1);
		} else {
			if (++i == entries)
				break;
		}
	}

	return (0);
}

static void
usage(void)
{
	errx(1, USAGE);
}
