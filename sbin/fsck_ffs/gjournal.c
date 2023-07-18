/*-
 * SPDX-License-Identifier: BSD-3-Clause AND BSD-2-Clause
 *
 * Copyright (c) 2006 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <string.h>
#include <sys/stat.h>
#include <ufs/ffs/fs.h>
#include "fsck.h"

void
gjournal_check(const char *filesys)
{
	struct fs *fs;
	struct inode ip;
	union dinode *dp;
	struct bufarea *cgbp;
	struct cg *cgp;
	struct inodesc idesc;
	uint8_t *inosused;
	ino_t cino, ino;
	int cg;

	fs = &sblock;
	/* Are there any unreferenced inodes in this file system? */
	if (fs->fs_unrefs == 0) {
		sbdirty();
		ckfini(1);
		return;
	}

	for (cg = 0; cg < fs->fs_ncg; cg++) {
		/* Show progress if requested. */
		if (got_siginfo) {
			printf("%s: phase j: cyl group %d of %d (%d%%)\n",
			    cdevname, cg, fs->fs_ncg, cg * 100 / fs->fs_ncg);
			got_siginfo = 0;
		}
		if (got_sigalarm) {
			setproctitle("%s pj %d%%", cdevname,
			     cg * 100 / fs->fs_ncg);
			got_sigalarm = 0;
		}
		cgbp = cglookup(cg);
		cgp = cgbp->b_un.b_cg;
		if (!check_cgmagic(cg, cgbp)) {
			rerun = 1;
			ckfini(0);
			return;
		}
		/* Are there any unreferenced inodes in this cylinder group? */
		if (cgp->cg_unrefs == 0)
			continue;
		/*
		 * Now go through the list of all inodes in this cylinder group
		 * to find unreferenced ones.
		 */
		inosused = cg_inosused(cgp);
		for (cino = 0; cino < fs->fs_ipg; cino++) {
			ino = fs->fs_ipg * cg + cino;
			/* Unallocated? Skip it. */
			if (isclr(inosused, cino))
				continue;
			ginode(ino, &ip);
			dp = ip.i_dp;
			/* Not a regular file nor directory? Skip it. */
			if (!S_ISREG(dp->dp2.di_mode) &&
			    !S_ISDIR(dp->dp2.di_mode)) {
				irelse(&ip);
				continue;
			}
			/* Has reference(s)? Skip it. */
			if (dp->dp2.di_nlink > 0) {
				irelse(&ip);
				continue;
			}
			/* printf("Clearing inode=%d (size=%jd)\n", ino,
			    (intmax_t)dp->dp2->di_size); */
			/* Deallocate it. */
			memset(&idesc, 0, sizeof(struct inodesc));
			idesc.id_type = ADDR;
			idesc.id_func = freeblock;
			idesc.id_number = ino;
			clri(&idesc, "UNREF", 1);
			clrbit(inosused, cino);
			/* Update position of last used inode. */
			if (ino < cgp->cg_irotor)
				cgp->cg_irotor = ino;
			/* Update statistics. */
			cgp->cg_unrefs--;
			fs->fs_unrefs--;
			/* Zero-fill the inode. */
			dp->dp2 = zino.dp2;
			/* Write the inode back. */
			inodirty(&ip);
			irelse(&ip);
			cgdirty(cgbp);
			if (cgp->cg_unrefs == 0)
				break;
		}
		/*
		 * If there are no more unreferenced inodes, there is no
		 * need to check other cylinder groups.
		 */
		if (fs->fs_unrefs == 0)
			break;
	}
	/* Write back updated statistics and super-block. */
	sbdirty();
	ckfini(1);
}
