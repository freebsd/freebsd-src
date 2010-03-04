/*-
 * Copyright (c) 2005 Takanori Watanabe
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

#include <fs/ntfs/ntfs.h>

#include <geom/geom.h>
#include <geom/label/g_label.h>

#define G_LABEL_NTFS_DIR	"ntfs"


static void
g_label_ntfs_taste(struct g_consumer *cp, char *label, size_t size)
{
	struct g_provider *pp;
	struct bootfile *bf;
	struct filerec *fr;
	struct attr *atr;
	off_t voloff;
	char *filerecp, *ap;
	char mftrecsz, vnchar;
	int recsize, j;

	g_topology_assert_not();

	label[0] = '\0';
	pp = cp->provider;
	filerecp = NULL;

	bf = (struct bootfile *)g_read_data(cp, 0, pp->sectorsize, NULL);
	if (bf == NULL || strncmp(bf->bf_sysid, "NTFS    ", 8) != 0)
		goto done;

	mftrecsz = (char)bf->bf_mftrecsz;
	recsize = (mftrecsz > 0) ? (mftrecsz * bf->bf_bps * bf->bf_spc) : (1 << -mftrecsz);
	if (recsize % pp->sectorsize != 0)
		goto done;

	voloff = bf->bf_mftcn * bf->bf_spc * bf->bf_bps +
	    recsize * NTFS_VOLUMEINO;
	if (voloff % pp->sectorsize != 0)
		goto done;

	filerecp = g_read_data(cp, voloff, recsize, NULL);
	if (filerecp == NULL)
		goto done;
	fr = (struct filerec *)filerecp;

	if (fr->fr_fixup.fh_magic != NTFS_FILEMAGIC)
		goto done;

	for (ap = filerecp + fr->fr_attroff;
	    atr = (struct attr *)ap, atr->a_hdr.a_type != -1;
	    ap += atr->a_hdr.reclen) {
		if (atr->a_hdr.a_type == NTFS_A_VOLUMENAME) {
			if(atr->a_r.a_datalen >= size *2){
				label[0] = 0;
				goto done;
			}
			/*
			 *UNICODE to ASCII.
			 * Should we need to use iconv(9)?
			 */
			for (j = 0; j < atr->a_r.a_datalen; j++) {
				vnchar = *(ap + atr->a_r.a_dataoff + j);
				if (j & 1) {
					if (vnchar) {
						label[0] = 0;
						goto done;
					}
				} else {
					label[j / 2] = vnchar;
				}
			}
			label[j / 2] = 0;
			break;
		}
	}
done:
	if (bf != NULL)
		g_free(bf);
	if (filerecp != NULL)
		g_free(filerecp);
}

struct g_label_desc g_label_ntfs = {
	.ld_taste = g_label_ntfs_taste,
	.ld_dir = G_LABEL_NTFS_DIR,
	.ld_enabled = 1
};

G_LABEL_INIT(ntfs, g_label_ntfs, "Create device nodes for NTFS volumes");
