/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/disklabel.h>
#include <sys/malloc.h>
#include <sys/vnode.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/extattr.h>
#include <ufs/ffs/ffs_extern.h>

#include <geom/geom.h>
#include <geom/geom_dbg.h>
#include <geom/label/g_label.h>

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

/*
 * For providers that look like disklabels we need to check if the file system
 * size is almost equal to the provider's size, because sysinstall(8) used to
 * bogusly put the first partition at offset 0 instead of 16, and glabel/ufs
 * would find a file system on the slice instead of the partition.
 *
 * In addition, media size can be a bit bigger than file system size. For
 * instance, mkuzip can append bytes to align data to large sector size (it
 * improves compression rates).
 */
static bool
g_label_ufs_ignore_bsdlabel_slice(struct g_consumer *cp,
    struct fs *fs)
{
	struct g_provider *pp;
	u_char *buf;
	uint32_t magic1, magic2;
	int error;

	pp = cp->provider;

	/*
	 * If the expected provider size for the filesystem matches the
	 * real provider size then don't ignore this filesystem.
	 */
	if (G_LABEL_UFS_CMP(pp, fs, fs_providersize))
		return (false);

	/*
	 * If the filesystem size matches the real provider size then
	 * don't ignore this filesystem.
	 */
	if (fs->fs_magic == FS_UFS1_MAGIC ?
	    G_LABEL_UFS_CMP(pp, fs, fs_old_size) :
	    G_LABEL_UFS_CMP(pp, fs, fs_size))
		return (false);

	/*
	 * Provider is bigger than expected; probe to see if there's a
	 * disklabel. Adapted from g_part_bsd_probe.
	 */

	/* Check if the superblock overlaps where the disklabel lives. */
	if (fs->fs_sblockloc < pp->sectorsize * 2)
		return (false);

	/* Sanity-check the provider. */
	if (pp->sectorsize < sizeof(struct disklabel) ||
	    pp->mediasize < BBSIZE)
		return (false);
	if (BBSIZE % pp->sectorsize)
		return (false);

	/* Check that there's a disklabel. */
	buf = g_read_data(cp, pp->sectorsize, pp->sectorsize, &error);
	if (buf == NULL)
		return (false);
	magic1 = le32dec(buf + 0);
	magic2 = le32dec(buf + 132);
	g_free(buf);
	if (magic1 == DISKMAGIC && magic2 == DISKMAGIC)
		return (true);

	return (false);
}

/*
 * Try to find a superblock on the provider. If successful, look for a volume
 * label and create an appropriate provider based on that.
 */
static void
g_label_ufs_taste_common(struct g_consumer *cp, char *label, size_t size, int what)
{
	struct g_provider *pp;
	struct fs *fs;

	g_topology_assert_not();
	pp = cp->provider;
	label[0] = '\0';

	fs = NULL;
	KASSERT(pp->sectorsize != 0, ("Tasting a disk with 0 sectorsize"));
	if (SBLOCKSIZE % pp->sectorsize != 0 || ffs_sbget(cp, &fs, UFS_STDSB,
	    UFS_NOHASHFAIL | UFS_NOCSUM | UFS_NOMSG, M_GEOM, g_use_g_read_data)
	    != 0) {
		KASSERT(fs == NULL,
		    ("g_label_ufs_taste_common: non-NULL fs %p\n", fs));
		return;
	}

	/* Check for magic. */
	if (fs->fs_magic == FS_UFS1_MAGIC && fs->fs_fsize > 0) {
		/* Valid UFS1. */
	} else if (fs->fs_magic == FS_UFS2_MAGIC && fs->fs_fsize > 0) {
		/* Valid UFS2. */
	} else {
		goto out;
	}
	/* Check if this should be ignored for compatibility. */
	if (g_label_ufs_ignore_bsdlabel_slice(cp, fs))
		goto out;
	G_LABEL_DEBUG(1, "%s file system detected on %s.",
	    fs->fs_magic == FS_UFS1_MAGIC ? "UFS1" : "UFS2", pp->name);
	switch (what) {
	case G_LABEL_UFS_VOLUME:
		/* Check for volume label */
		if (fs->fs_volname[0] != '\0')
			strlcpy(label, fs->fs_volname, size);
		break;
	case G_LABEL_UFS_ID:
		if (fs->fs_id[0] != 0 || fs->fs_id[1] != 0)
			snprintf(label, size, "%08x%08x", fs->fs_id[0],
			    fs->fs_id[1]);
		break;
	}
out:
	g_free(fs);
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
	.ld_dirprefix = "ufs/",
	.ld_enabled = 1
};

struct g_label_desc g_label_ufs_id = {
	.ld_taste = g_label_ufs_id_taste,
	.ld_dirprefix = "ufsid/",
	.ld_enabled = 1
};

G_LABEL_INIT(ufsid, g_label_ufs_id, "Create device nodes for UFS file system IDs");
G_LABEL_INIT(ufs, g_label_ufs_volume, "Create device nodes for UFS volume names");

MODULE_DEPEND(g_label, ufs, 1, 1, 1);
