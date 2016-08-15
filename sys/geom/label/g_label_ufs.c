/*-
 * Copyright (c) 2002, 2003 Gordon Tetlow
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

#define G_LABEL_UFS_VOLUME_DIR	"ufs"
#define G_LABEL_UFS_ID_DIR	"ufsid"

#define	G_LABEL_UFS_VOLUME	0
#define	G_LABEL_UFS_ID		1

/*
 * G_LABEL_UFS_CMP returns true if difference between provider mediasize
 * and filesystem size is less than G_LABEL_UFS_MAXDIFF sectors
 */
#define	G_LABEL_UFS_CMP(prov, fsys, size) 				   \
	( abs( ((fsys)->size) - ( (prov)->mediasize / (fsys)->fs_fsize ))  \
				< G_LABEL_UFS_MAXDIFF )
#define	G_LABEL_UFS_MAXDIFF	0x100

static const int superblocks[] = SBLOCKSEARCH;

static void
g_label_ufs_taste_common(struct g_consumer *cp, char *label, size_t size, int what)
{
	struct g_provider *pp;
	int sb, superblock;
	struct fs *fs;

	g_topology_assert_not();
	pp = cp->provider;
	label[0] = '\0';

	if (SBLOCKSIZE % cp->provider->sectorsize != 0)
		return;

	/*
	 * Walk through the standard places that superblocks hide and look
	 * for UFS magic. If we find magic, then check that the size in the
	 * superblock corresponds to the size of the underlying provider.
	 * Finally, look for a volume label and create an appropriate
	 * provider based on that.
	 */
	for (sb = 0; (superblock = superblocks[sb]) != -1; sb++) {
		/*
		 * Take care not to issue an invalid I/O request. The offset of
		 * the superblock candidate must be multiples of the provider's
		 * sector size, otherwise an FFS can't exist on the provider
		 * anyway.
		 */
		if (superblock % cp->provider->sectorsize != 0)
			continue;

		fs = (struct fs *)g_read_data(cp, superblock, SBLOCKSIZE, NULL);
		if (fs == NULL)
			continue;
		/*
		 * Check for magic. We also need to check if file system size
		 * is almost equal to providers size, because sysinstall(8)
		 * used to bogusly put first partition at offset 0
		 * instead of 16, and glabel/ufs would find file system on slice
		 * instead of partition.
		 *
		 * In addition, media size can be a bit bigger than file system
		 * size. For instance, mkuzip can append bytes to align data
		 * to large sector size (it improves compression rates).
		 */
		switch (fs->fs_magic){
		case FS_UFS1_MAGIC:
		case FS_UFS2_MAGIC:
			G_LABEL_DEBUG(1, "%s %s params: %jd, %d, %d, %jd\n",
				fs->fs_magic == FS_UFS1_MAGIC ? "UFS1" : "UFS2",
				pp->name, pp->mediasize, fs->fs_fsize,
				fs->fs_old_size, fs->fs_providersize);
			break;
		default:
			break;
		}

		if (fs->fs_magic == FS_UFS1_MAGIC && fs->fs_fsize > 0 &&
		    ( G_LABEL_UFS_CMP(pp, fs, fs_old_size)
			|| G_LABEL_UFS_CMP(pp, fs, fs_providersize))) {
		    	/* Valid UFS1. */
		} else if (fs->fs_magic == FS_UFS2_MAGIC && fs->fs_fsize > 0 &&
		    ( G_LABEL_UFS_CMP(pp, fs, fs_size)
			|| G_LABEL_UFS_CMP(pp, fs, fs_providersize))) {
		    	/* Valid UFS2. */
		} else {
			g_free(fs);
			continue;
		}
		if (fs->fs_sblockloc != superblock || fs->fs_ncg < 1 ||
		    fs->fs_bsize < MINBSIZE ||
		    fs->fs_bsize < sizeof(struct fs)) {
			g_free(fs);
			continue;
		}
		G_LABEL_DEBUG(1, "%s file system detected on %s.",
		    fs->fs_magic == FS_UFS1_MAGIC ? "UFS1" : "UFS2", pp->name);
		switch (what) {
		case G_LABEL_UFS_VOLUME:
			/* Check for volume label */
			if (fs->fs_volname[0] == '\0') {
				g_free(fs);
				continue;
			}
			strlcpy(label, fs->fs_volname, size);
			break;
		case G_LABEL_UFS_ID:
			if (fs->fs_id[0] == 0 && fs->fs_id[1] == 0) {
				g_free(fs);
				continue;
			}
			snprintf(label, size, "%08x%08x", fs->fs_id[0],
			    fs->fs_id[1]);
			break;
		}
		g_free(fs);
		break;
	}
}

static void
g_label_ufs_volume_taste(struct g_consumer *cp, char *label, size_t size)
{

	g_label_ufs_taste_common(cp, label, size, G_LABEL_UFS_VOLUME);
}

static void
g_label_ufs_id_taste(struct g_consumer *cp, char *label, size_t size)
{

	g_label_ufs_taste_common(cp, label, size, G_LABEL_UFS_ID);
}

struct g_label_desc g_label_ufs_volume = {
	.ld_taste = g_label_ufs_volume_taste,
	.ld_dir = G_LABEL_UFS_VOLUME_DIR,
	.ld_enabled = 1
};

struct g_label_desc g_label_ufs_id = {
	.ld_taste = g_label_ufs_id_taste,
	.ld_dir = G_LABEL_UFS_ID_DIR,
	.ld_enabled = 1
};

G_LABEL_INIT(ufsid, g_label_ufs_id, "Create device nodes for UFS file system IDs");
G_LABEL_INIT(ufs, g_label_ufs_volume, "Create device nodes for UFS volume names");
