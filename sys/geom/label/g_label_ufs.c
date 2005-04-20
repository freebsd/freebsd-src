/*-
 * Copyright (c) 2002, 2003 Gordon Tetlow
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <geom/geom.h>
#include <geom/label/g_label.h>

#define G_LABEL_UFS_DIR	"ufs"

static const int superblocks[] = SBLOCKSEARCH;

static void
g_label_ufs_taste(struct g_consumer *cp, char *label, size_t size)
{
	struct g_provider *pp;
	int error, sb, superblock;
	struct fs *fs;

	g_topology_assert_not();
	pp = cp->provider;
	label[0] = '\0';
	/*
	 * Walk through the standard places that superblocks hide and look
	 * for UFS magic. If we find magic, then check that the size in the
	 * superblock corresponds to the size of the underlying provider.
	 * Finally, look for a volume label and create an appropriate 
	 * provider based on that.
	 */
	for (sb = 0; (superblock = superblocks[sb]) != -1; sb++) {
		/*
		 * Take care not to issue an invalid I/O request.  The
		 * offset and size of the superblock candidate must be
		 * multiples of the provider's sector size, otherwise an
		 * FFS can't exist on the provider anyway.
		 */
		if (superblock % cp->provider->sectorsize != 0 ||
		    SBLOCKSIZE % cp->provider->sectorsize != 0)
			continue;

		fs = (struct fs *)g_read_data(cp, superblock, SBLOCKSIZE,
		    &error);
		if (fs == NULL || error != 0)
			continue;
		/* Check for magic and make sure things are the right size */
		if (fs->fs_magic == FS_UFS1_MAGIC) {
			G_LABEL_DEBUG(1, "UFS1 file system detected on %s.",
			    pp->name);
			if (fs->fs_old_size * fs->fs_fsize !=
			    (int32_t)pp->mediasize) {
				g_free(fs);
				continue;
			}
		} else if (fs->fs_magic == FS_UFS2_MAGIC) {
			G_LABEL_DEBUG(1, "UFS2 file system detected on %s.",
			    pp->name);
			if (fs->fs_size * fs->fs_fsize !=
			    (int64_t)pp->mediasize) {
				g_free(fs);
				continue;
			}
		} else {
			g_free(fs);
			continue;
		}
		/* Check for volume label */
		if (fs->fs_volname[0] == '\0') {
			g_free(fs);
			continue;
		}
		strlcpy(label, fs->fs_volname, size);
		g_free(fs);
		break;
	}
}

const struct g_label_desc g_label_ufs = {
	.ld_taste = g_label_ufs_taste,
	.ld_dir = G_LABEL_UFS_DIR
};
