/*-
 * Copyright (c) 2004 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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

#include <geom/geom.h>
#include <geom/label/g_label.h>

#define G_LABEL_MSDOSFS_DIR	"msdosfs"

#define	FAT12	"FAT12   "
#define	FAT16	"FAT16   "
#define	FAT32	"FAT32   "
#define	VOLUME_LEN	11
#define NO_NAME "NO NAME    "


static void
g_label_msdosfs_taste(struct g_consumer *cp, char *label, size_t size)
{
	struct g_provider *pp;
	char *sector, *volume;
	int i, error;

	g_topology_assert_not();
	pp = cp->provider;
	label[0] = '\0';

	sector = (char *)g_read_data(cp, 0, pp->sectorsize, &error);
	if (sector == NULL || error != 0)
		return;
	if (strncmp(sector + 0x36, FAT12, strlen(FAT12)) == 0) {
		G_LABEL_DEBUG(1, "MSDOS (FAT12) file system detected on %s.",
		    pp->name);
		volume = sector + 0x2b;
	} else if (strncmp(sector + 0x36, FAT16, strlen(FAT16)) == 0) {
		G_LABEL_DEBUG(1, "MSDOS (FAT16) file system detected on %s.",
		    pp->name);
		volume = sector + 0x2b;
	} else if (strncmp(sector + 0x52, FAT32, strlen(FAT32)) == 0) {
		G_LABEL_DEBUG(1, "MSDOS (FAT32) file system detected on %s.",
		    pp->name);
		volume = sector + 0x47;
	} else {
		g_free(sector);
		return;
	}
	if (strncmp(volume, NO_NAME, VOLUME_LEN) == 0) {
		g_free(sector);
		return;
	}
	if (volume[0] == '\0') {
		g_free(sector);
		return;
	}
	bzero(label, size);
	strlcpy(label, volume, MIN(size, VOLUME_LEN));
	g_free(sector);
	for (i = size - 1; i > 0; i--) {
		if (label[i] == '\0')
			continue;
		else if (label[i] == ' ')
			label[i] = '\0';
		else
			break;
	}
}

const struct g_label_desc g_label_msdosfs = {
	.ld_taste = g_label_msdosfs_taste,
	.ld_dir = G_LABEL_MSDOSFS_DIR
};
