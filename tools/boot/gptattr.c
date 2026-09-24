/*
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Set a GPT attribute on a partition of a disk image file.
 *
 * gpart(8) does the same thing, but only on a real device and only as root.
 * boot-test.sh builds every image as an unprivileged user, so it needs to
 * edit the on-disk GPT itself.  Both the primary and the secondary entry
 * table are updated: the loader only reads the primary, but the kernel
 * checks both, and a mismatch would be reported as a corrupt GPT during the
 * boot under test.
 *
 * Usage: gptattr <image> <index> <attribute-bit>
 * where <index> is 1-based, matching gpart(8) partition numbering.
 */

#include <sys/param.h>
#include <sys/disk/gpt.h>
#include <sys/endian.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#define	SECSZ	512

static uint8_t *image;
static size_t imagesz;

/*
 * Apply the attribute to one entry table and refresh the CRCs of the header
 * at hdroff that describes it.
 */
static void
patch_table(uint64_t hdroff, u_int index, uint64_t attr)
{
	struct gpt_hdr *hdr;
	struct gpt_ent *ent;
	uint64_t tbloff;
	uint32_t entries, entsz, hdrsz;

	if (hdroff + SECSZ > imagesz)
		errx(1, "GPT header at offset %ju is past end of image",
		    (uintmax_t)hdroff);

	hdr = (struct gpt_hdr *)(image + hdroff);
	if (memcmp(hdr->hdr_sig, GPT_HDR_SIG, sizeof(hdr->hdr_sig)) != 0)
		errx(1, "no GPT header at offset %ju", (uintmax_t)hdroff);

	hdrsz = le32toh(hdr->hdr_size);
	entries = le32toh(hdr->hdr_entries);
	entsz = le32toh(hdr->hdr_entsz);
	tbloff = le64toh(hdr->hdr_lba_table) * SECSZ;

	if (index < 1 || index > entries)
		errx(1, "partition index %u out of range (1..%u)", index,
		    entries);
	if (tbloff + (uint64_t)entries * entsz > imagesz)
		errx(1, "GPT entry table is past end of image");

	ent = (struct gpt_ent *)(image + tbloff + (uint64_t)(index - 1) * entsz);
	le64enc(&ent->ent_attr, le64toh(ent->ent_attr) | attr);

	/*
	 * The table CRC covers the entries, and the header CRC covers the
	 * header including that table CRC, so recompute them in that order.
	 * hdr_crc_self is zeroed while it is being computed over itself.
	 */
	le32enc(&hdr->hdr_crc_table, crc32(0, image + tbloff,
	    entries * entsz));
	hdr->hdr_crc_self = 0;
	le32enc(&hdr->hdr_crc_self, crc32(0, (const uint8_t *)hdr, hdrsz));
}

int
main(int argc, char *argv[])
{
	FILE *fp;
	char *end;
	uint64_t attr;
	u_long bit, index;

	if (argc != 4) {
		fprintf(stderr,
		    "usage: %s <image> <index> <attribute-bit>\n", argv[0]);
		return (1);
	}

	errno = 0;
	index = strtoul(argv[2], &end, 0);
	if (errno != 0 || *end != '\0' || index == 0)
		errx(1, "bad partition index: %s", argv[2]);
	bit = strtoul(argv[3], &end, 0);
	if (errno != 0 || *end != '\0' || bit > 63)
		errx(1, "bad attribute bit: %s", argv[3]);
	attr = (uint64_t)1 << bit;

	if ((fp = fopen(argv[1], "r+")) == NULL)
		err(1, "%s", argv[1]);
	if (fseek(fp, 0, SEEK_END) != 0)
		err(1, "%s", argv[1]);
	imagesz = (size_t)ftell(fp);
	rewind(fp);
	if (imagesz < 2 * SECSZ)
		errx(1, "%s is too small to hold a GPT", argv[1]);
	if ((image = malloc(imagesz)) == NULL)
		err(1, "malloc");
	if (fread(image, 1, imagesz, fp) != imagesz)
		err(1, "%s: read", argv[1]);

	/* Primary header is always at LBA 1; it names the secondary. */
	patch_table(SECSZ, index, attr);
	patch_table(le64toh(((struct gpt_hdr *)(image + SECSZ))->hdr_lba_alt) *
	    SECSZ, index, attr);

	rewind(fp);
	if (fwrite(image, 1, imagesz, fp) != imagesz)
		err(1, "%s: write", argv[1]);
	if (fclose(fp) != 0)
		err(1, "%s: close", argv[1]);
	free(image);

	return (0);
}
