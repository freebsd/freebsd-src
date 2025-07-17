/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1980, 1986, 1993
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

#include <sys/param.h>
#include <sys/stat.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <stdint.h>
#include <string.h>

#include "fsck.h"

void
pass4(void)
{
	ino_t inumber;
	struct inode ip;
	struct inodesc idesc;
	int i, n, cg;

	memset(&idesc, 0, sizeof(struct inodesc));
	idesc.id_func = freeblock;
	for (cg = 0; cg < sblock.fs_ncg; cg++) {
		if (got_siginfo) {
			printf("%s: phase 4: cyl group %d of %d (%d%%)\n",
			    cdevname, cg, sblock.fs_ncg,
			    cg * 100 / sblock.fs_ncg);
			got_siginfo = 0;
		}
		if (got_sigalarm) {
			setproctitle("%s p4 %d%%", cdevname,
			    cg * 100 / sblock.fs_ncg);
			got_sigalarm = 0;
		}
		inumber = cg * sblock.fs_ipg;
		for (i = 0; i < inostathead[cg].il_numalloced; i++, inumber++) {
			if (inumber < UFS_ROOTINO)
				continue;
			idesc.id_number = inumber;
			idesc.id_type = inoinfo(inumber)->ino_idtype;
			switch (inoinfo(inumber)->ino_state) {

			case FZLINK:
			case DZLINK:
				if (inoinfo(inumber)->ino_linkcnt == 0) {
					clri(&idesc, "UNREF", 1);
					break;
				}
				/* fall through */

			case FSTATE:
			case DFOUND:
				n = inoinfo(inumber)->ino_linkcnt;
				if (n) {
					adjust(&idesc, (short)n);
					break;
				}
				break;

			case DSTATE:
				clri(&idesc, "UNREF", 1);
				break;

			case DCLEAR:
				/* if on snapshot, already cleared */
				if (cursnapshot != 0)
					break;
				ginode(inumber, &ip);
				if (DIP(ip.i_dp, di_size) == 0) {
					clri(&idesc, "ZERO LENGTH", 1);
					irelse(&ip);
					break;
				}
				irelse(&ip);
				/* fall through */
			case FCLEAR:
				clri(&idesc, "BAD/DUP", 1);
				break;

			case USTATE:
				break;

			default:
				errx(EEXIT, "BAD STATE %d FOR INODE I=%ju",
				    inoinfo(inumber)->ino_state,
				    (uintmax_t)inumber);
			}
		}
	}
}
