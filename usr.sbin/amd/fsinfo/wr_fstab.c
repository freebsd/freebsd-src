/*
 * Copyright (c) 1989 Jan-Simon Pendry
 * Copyright (c) 1989 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
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
 *	@(#)wr_fstab.c	8.1 (Berkeley) 6/6/93
 *
 * $FreeBSD$
 *
 */

#include "../fsinfo/fsinfo.h"

/* ---------- AIX 1 ------------------------------ */

/*
 * AIX 1 format
 */
static void write_aix1_dkfstab(ef, dp)
FILE *ef;
disk_fs *dp;
{
	char *hp = strdup(dp->d_host->h_hostname);
	char *p = strchr(hp, '.');
	if (p)
		*p = '\0';

	fprintf(ef, "\n%s:\n\tdev = %s\n\tvfs = %s\n\ttype = %s\n\tlog = %s\n\tvol = %s\n\topts = %s\n\tmount = true\n\tcheck = true\n\tfree = false\n",
		dp->d_mountpt,
		dp->d_dev,
		dp->d_fstype,
		dp->d_fstype,
		dp->d_log,
		dp->d_mountpt,
		dp->d_opts);
	free(hp);
}

static void write_aix1_dkrmount(ef, hn, fp)
FILE *ef;
char *hn;
fsmount *fp;
{
	char *h = strdup(fp->f_ref->m_dk->d_host->h_hostname);
	char *hp = strdup(h);
	char *p = strchr(hp, '.');
	if (p)
		*p = '\0';
	domain_strip(h, hn);
	fprintf(ef, "\n%s:\n\tsite = %s\n\tdev = %s:%s\n\tvfs = %s\n\ttype = %s\n\tvol = %s\n\topts = %s\n\tmount = true\n\tcheck = true\n\tfree = false\n",
		fp->f_localname,
		hp,
		h,
		fp->f_volname,
		fp->f_fstype,
		fp->f_fstype,
		fp->f_localname,
		fp->f_opts);

	free(hp);
	free(h);
}

/* ---------- AIX 3 ------------------------------ */

/*
 * AIX 3 format
 */
static void write_aix3_dkfstab(ef, dp)
FILE *ef;
disk_fs *dp;
{
	if (strcmp(dp->d_fstype, "jfs") == 0 && strncmp(dp->d_dev, "/dev/", 5) == 0 && !dp->d_log)
		error("aix 3 needs a log device for journalled filesystem (jfs) mounts");

	fprintf(ef, "\n%s:\n\tdev = %s\n\tvfs = %s\n\ttype = %s\n\tlog = %s\n\tvol = %s\n\topts = %s\n\tmount = true\n\tcheck = true\n\tfree = false\n",
		dp->d_mountpt,
		dp->d_dev,
		dp->d_fstype,
		dp->d_fstype,
		dp->d_log,
		dp->d_mountpt,
		dp->d_opts);
}

static void write_aix3_dkrmount(ef, hn, fp)
FILE *ef;
char *hn;
fsmount *fp;
{
	char *h = strdup(fp->f_ref->m_dk->d_host->h_hostname);
	domain_strip(h, hn);
	fprintf(ef, "\n%s:\n\tdev = %s:%s\n\tvfs = %s\n\ttype = %s\n\tvol = %s\n\topts = %s\n\tmount = true\n\tcheck = true\n\tfree = false\n",
		fp->f_localname,
		h,
		fp->f_volname,
		fp->f_fstype,
		fp->f_fstype,
		fp->f_localname,
		fp->f_opts);

	free(h);
}

/* ---------- Ultrix ----------------------------- */

static void write_ultrix_dkfstab(ef, dp)
FILE *ef;
disk_fs *dp;
{
	fprintf(ef, "%s:%s:%s:%s:%d:%d\n",
		dp->d_dev,
		dp->d_mountpt,
		dp->d_fstype,
		dp->d_opts,
		dp->d_freq,
		dp->d_passno);
}

static void write_ultrix_dkrmount(ef, hn, fp)
FILE *ef;
char *hn;
fsmount *fp;
{
	char *h = strdup(fp->f_ref->m_dk->d_host->h_hostname);
	domain_strip(h, hn);
	fprintf(ef, "%s@%s:%s:%s:%s:0:0\n",
		fp->f_volname,
		h,
		fp->f_localname,
		fp->f_fstype,
		fp->f_opts);
	free(h);
}

/* ---------- Generic ---------------------------- */

/*
 * Generic (BSD, SunOS, HPUX) format
 */
static void write_generic_dkfstab(ef, dp)
FILE *ef;
disk_fs *dp;
{
	fprintf(ef, "%s %s %s %s %d %d\n",
		dp->d_dev,
		dp->d_mountpt,
		dp->d_fstype,
		dp->d_opts,
		dp->d_freq,
		dp->d_passno);
}

static void write_generic_dkrmount(ef, hn, fp)
FILE *ef;
char *hn;
fsmount *fp;
{
	char *h = strdup(fp->f_ref->m_dk->d_host->h_hostname);
	domain_strip(h, hn);
	fprintf(ef, "%s:%s %s %s %s 0 0\n",
		h,
		fp->f_volname,
		fp->f_localname,
		fp->f_fstype,
		fp->f_opts);
	free(h);
}

/* ----------------------------------------------- */

static struct os_fstab_type {
	char *os_name;
	void (*op_fstab)();
	void (*op_mount)();
} os_tabs[] = {
	{ "aix1", write_aix1_dkfstab, write_aix1_dkrmount },		/* AIX 1 */
	{ "aix3", write_aix3_dkfstab, write_aix3_dkrmount },		/* AIX 3 */
	{ "generic", write_generic_dkfstab, write_generic_dkrmount },	/* Generic */
	{ "u2_0", write_ultrix_dkfstab, write_ultrix_dkrmount },	/* Ultrix */
	{ "u3_0", write_ultrix_dkfstab, write_ultrix_dkrmount },	/* Ultrix */
	{ "u4_0", write_ultrix_dkfstab, write_ultrix_dkrmount },	/* Ultrix */
	{ 0, 0, 0 }
};

#define	GENERIC_OS_NAME "generic"

static struct os_fstab_type *find_fstab_type(hp)
host *hp;
{
	struct os_fstab_type *op = 0;
	char *os_name = 0;

again:;
	if (os_name == 0) {
		if (ISSET(hp->h_mask, HF_OS))
			os_name = hp->h_os;
		else
			os_name = GENERIC_OS_NAME;
	}

	for (op = os_tabs; op->os_name; op++)
		if (strcmp(os_name, op->os_name) == 0)
			return op;

	os_name = GENERIC_OS_NAME;
	goto again;
}

static int write_dkfstab(ef, q, output)
FILE *ef;
qelem *q;
void (*output)();
{
	int errors = 0;
	disk_fs *dp;

	ITER(dp, disk_fs, q)
		if (strcmp(dp->d_fstype, "export") != 0)
			(*output)(ef, dp);

	return errors;
}

static int write_dkrmount(ef, q, hn, output)
FILE *ef;
qelem *q;
char *hn;
void (*output)();
{
	int errors = 0;
	fsmount *fp;

	ITER(fp, fsmount, q)
		(*output)(ef, hn, fp);

	return errors;
}

int write_fstab(q)
qelem *q;
{
	int errors = 0;

	if (fstab_pref) {
		host *hp;
		show_area_being_processed("write fstab", 4);
		ITER(hp, host, q) {
			if (hp->h_disk_fs || hp->h_mount) {
				FILE *ef = pref_open(fstab_pref, hp->h_hostname, gen_hdr, hp->h_hostname);
				if (ef) {
					struct os_fstab_type *op = find_fstab_type(hp);
					show_new(hp->h_hostname);
					if (hp->h_disk_fs)
						errors += write_dkfstab(ef, hp->h_disk_fs, op->op_fstab);
					else
						log("No local disk mounts on %s", hp->h_hostname);

					if (hp->h_mount)
						errors += write_dkrmount(ef, hp->h_mount, hp->h_hostname, op->op_mount);

					pref_close(ef);
				}
			} else {
				error("no disk mounts on %s", hp->h_hostname);
			}
		}
	}

	return errors;
}
