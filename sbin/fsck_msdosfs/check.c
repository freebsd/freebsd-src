/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 1995, 1996, 1997 Wolfgang Solfrank
 * Copyright (c) 1995 Martin Husemann
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: check.c,v 1.14 2006/06/05 16:51:18 christos Exp $");
#endif /* not lint */

#ifdef HAVE_LIBUTIL_H
#include <libutil.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "ext.h"
#include "fsutil.h"

int
checkfilesys(const char *fname)
{
	int dosfs;
	struct bootblock boot;
	struct fat_descriptor *fat = NULL;
	int finish_dosdirsection=0;
	int mod = 0;
	int ret = 8;
	int64_t freebytes;
	int64_t badbytes;

	rdonly = alwaysno;
	if (!preen)
		printf("** %s", fname);

	dosfs = open(fname, rdonly ? O_RDONLY : O_RDWR, 0);
	if (dosfs < 0 && !rdonly) {
		dosfs = open(fname, O_RDONLY, 0);
		if (dosfs >= 0)
			pwarn(" (NO WRITE)\n");
		else if (!preen)
			printf("\n");
		rdonly = 1;
	} else if (!preen)
		printf("\n");

	if (dosfs < 0) {
		perr("Can't open `%s'", fname);
		printf("\n");
		return 8;
	}

	if (readboot(dosfs, &boot) == FSFATAL) {
		close(dosfs);
		printf("\n");
		return 8;
	}

	if (skipclean && preen && checkdirty(dosfs, &boot)) {
		printf("%s: ", fname);
		printf("FILESYSTEM CLEAN; SKIPPING CHECKS\n");
		ret = 0;
		goto out;
	}

	if (!preen)  {
		printf("** Phase 1 - Read FAT and checking connectivity\n");
	}

	mod |= readfat(dosfs, &boot, &fat);
	if (mod & FSFATAL) {
		close(dosfs);
		return 8;
	}

	if (!preen)
		printf("** Phase 2 - Checking Directories\n");

	mod |= resetDosDirSection(fat);
	finish_dosdirsection = 1;
	if (mod & FSFATAL)
		goto out;
	/* delay writing FATs */

	mod |= handleDirTree(fat);
	if (mod & FSFATAL)
		goto out;

	if (!preen)
		printf("** Phase 3 - Checking for Lost Files\n");

	mod |= checklost(fat);
	if (mod & FSFATAL)
		goto out;

	/* now write the FATs */
	if (mod & FSFATMOD) {
		if (ask(1, "Update FATs")) {
			mod |= writefat(fat);
			if (mod & FSFATAL)
				goto out;
		} else
			mod |= FSERROR;
	}

	freebytes = (int64_t)boot.NumFree * boot.ClusterSize;
	badbytes = (int64_t)boot.NumBad * boot.ClusterSize;

#ifdef HAVE_LIBUTIL_H
	char freestr[7], badstr[7];

	humanize_number(freestr, sizeof(freestr), freebytes, "",
	    HN_AUTOSCALE, HN_DECIMAL | HN_IEC_PREFIXES);
	if (boot.NumBad) {
		humanize_number(badstr, sizeof(badstr), badbytes, "",
		    HN_AUTOSCALE, HN_B | HN_DECIMAL | HN_IEC_PREFIXES);

		pwarn("%d files, %sB free (%d clusters), %sB bad (%d clusters)\n",
		      boot.NumFiles, freestr, boot.NumFree,
		      badstr, boot.NumBad);
	} else {
		pwarn("%d files, %sB free (%d clusters)\n",
		      boot.NumFiles, freestr, boot.NumFree);
	}
#else
	if (boot.NumBad)
		pwarn("%d files, %jd KiB free (%d clusters), %jd KiB bad (%d clusters)\n",
		      boot.NumFiles, (intmax_t)freebytes / 1024, boot.NumFree,
		      (intmax_t)badbytes / 1024, boot.NumBad);
	else
		pwarn("%d files, %jd KiB free (%d clusters)\n",
		      boot.NumFiles, (intmax_t)freebytes / 1024, boot.NumFree);
#endif

	if (mod && (mod & FSERROR) == 0) {
		if (mod & FSDIRTY) {
			if (ask(1, "MARK FILE SYSTEM CLEAN") == 0)
				mod &= ~FSDIRTY;

			if (mod & FSDIRTY) {
				pwarn("MARKING FILE SYSTEM CLEAN\n");
				mod |= cleardirty(fat);
			} else {
				pwarn("\n***** FILE SYSTEM IS LEFT MARKED AS DIRTY *****\n");
				mod |= FSERROR; /* file system not clean */
			}
		}
	}

	if (mod & (FSFATAL | FSERROR))
		goto out;

	ret = 0;

    out:
	if (finish_dosdirsection)
		finishDosDirSection();
	free(fat);
	close(dosfs);

	if (mod & (FSFATMOD|FSDIRMOD))
		pwarn("\n***** FILE SYSTEM WAS MODIFIED *****\n");

	return ret;
}
