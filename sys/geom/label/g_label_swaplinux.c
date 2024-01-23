/*-
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <geom/geom.h>
#include <geom/geom_dbg.h>
#include <geom/label/g_label.h>

/*
 * Taken from
 * https://github.com/util-linux/util-linux/blob/master/include/swapheader.h
 */

#define SWAP_VERSION 1
#define SWAP_UUID_LENGTH 16
#define SWAP_LABEL_LENGTH 16
#define SWAP_SIGNATURE "SWAPSPACE2"
#define SWAP_SIGNATURE_SZ (sizeof(SWAP_SIGNATURE) - 1)

struct swap_header_v1_2 {
	char	      bootbits[1024];    /* Space for disklabel etc. */
	uint32_t      version;
	uint32_t      last_page;
	uint32_t      nr_badpages;
	unsigned char uuid[SWAP_UUID_LENGTH];
	char	      volume_name[SWAP_LABEL_LENGTH];
	uint32_t      padding[117];
	uint32_t      badpages[1];
};

typedef union {
	struct swap_header_v1_2	header;
	struct {
		uint8_t reserved[4096 - SWAP_SIGNATURE_SZ];
		char	signature[SWAP_SIGNATURE_SZ];
	} tail;
} swhdr_t;

#define sw_version	header.version
#define sw_volume_name	header.volume_name
#define sw_signature	tail.signature

static void
g_label_swaplinux_taste(struct g_consumer *cp, char *label, size_t size)
{
	struct g_provider *pp;
	swhdr_t *hdr;

	g_topology_assert_not();
	pp = cp->provider;
	label[0] = '\0';

	KASSERT(pp->sectorsize != 0, ("Tasting a disk with 0 sectorsize"));
	if ((4096 % pp->sectorsize) != 0)
		return;

	hdr = (swhdr_t *)g_read_data(cp, 0, 4096, NULL);
	if (hdr == NULL)
		return;

	/* Check version and magic string */
	if (hdr->sw_version == SWAP_VERSION &&
	    !memcmp(hdr->sw_signature, SWAP_SIGNATURE, SWAP_SIGNATURE_SZ))
		G_LABEL_DEBUG(1, "linux swap detected on %s.", pp->name);
	else
		goto exit_free;

	/* Check for volume label */
	if (hdr->sw_volume_name[0] == '\0')
		goto exit_free;

	/* Terminate label */
	hdr->sw_volume_name[sizeof(hdr->sw_volume_name) - 1] = '\0';
	strlcpy(label, hdr->sw_volume_name, size);

exit_free:
	g_free(hdr);
}

struct g_label_desc g_label_swaplinux = {
	.ld_taste = g_label_swaplinux_taste,
	.ld_dirprefix = "swaplinux/",
	.ld_enabled = 1
};

G_LABEL_INIT(swaplinux, g_label_swaplinux, "Create device nodes for Linux swap");
