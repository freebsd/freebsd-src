/*-
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fs/cd9660/iso.h>
#include <sys/fs/cd9660/cd9660_rrip.h>

#include <err.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* cd9660_vfsops.c scans descriptors starting at block 16. */
static const size_t pvd_block = 16;

/*
 * ISO_DEFAULT_BLOCK_SIZE is 2048 bytes, so the PVD byte offset is the
 * logical block number multiplied by the logical block size.
 */
static const off_t pvd_offset = (off_t)ISO_DEFAULT_BLOCK_SIZE * pvd_block;

/*
 * Corrupt a Rock Ridge ISO to demonstrate PR296853.
 *
 * The code is a condensed form of code from two files:
 * - sys/fs/cd9660/cd9660_vfsops.c
 * - sys/fs/cd9660/cd9660_rrip.c
 *
 * It finds the primary volume descriptor (PVD), then finds the root
 * directory in the PVD, and finally walks the SUSP entries until it
 * finds the Extensions Reference (ER). If the ER is Rock Ridge, it
 * corrupts the length.
 *
 * The corrupted length triggers the bug in PR296853.
 */
static int
corrupt_rock_ridge(const char *path)
{
	const struct iso_directory_record *root, *root_entry;
	const struct iso_primary_descriptor *pvd;
	const struct iso_volume_descriptor *vdp;
	uint8_t pvd_raw[ISO_DEFAULT_BLOCK_SIZE];
	uint8_t root_block[ISO_DEFAULT_BLOCK_SIZE];
	struct stat st;
	off_t root_offset;
	size_t name_len, record_len, su_len, su_offset;
	ssize_t nread;
	int fd;
	uint32_t root_extent;

	int error = -1;

	fd = open(path, O_CLOEXEC | O_NOFOLLOW | O_RDWR);
	if (fd == -1) {
		warn("open %s", path);
		goto out;
	}

	/* Sanity check to ensure that the ISO is large enough to have a PVD. */
	if (fstat(fd, &st) == -1) {
		warn("fstat");
		goto out;
	}
	if (st.st_size < pvd_offset + (off_t)sizeof(pvd_raw)) {
		warnx("image is too small (no primary volume descriptor)");
		goto out;
	}

	/* Read the PVD but don't retry short preads because it's a test. */
	nread = pread(fd, pvd_raw, sizeof(pvd_raw), pvd_offset);
	if (nread == -1) {
		warn("pread %s", path);
		goto out;
	}
	if (nread != (ssize_t)sizeof(pvd_raw)) {
		warnx("short read %s (%zd != %zu)", path, nread,
		    sizeof(pvd_raw));
		goto out;
	}

	/*
	 * Check that it's actually the primary descriptor.
	 * Logic is from iso_mountfs in cd9660_vfsops.c.
	 */
	vdp = (const struct iso_volume_descriptor *)pvd_raw;
	if (memcmp(vdp->id, ISO_STANDARD_ID, sizeof(vdp->id)) != 0) {
		warnx("read offset is not a volume descriptor %s", path);
		goto out;
	}
	if (isonum_711(vdp->type) != ISO_VD_PRIMARY) {
		warnx("primary volume descriptor isn't at expected location %s",
		    path);
		goto out;
	}
	pvd = (const struct iso_primary_descriptor *)pvd_raw;
	root = (const struct iso_directory_record *)pvd->root_directory_record;

	/*
	 * Now that we have the PVD, find the root directory so that we can
	 * mangle the system use area.
	 */
	root_extent = isonum_733(root->extent);
	root_offset = ((off_t)root_extent + isonum_711(root->ext_attr_length)) *
	    ISO_DEFAULT_BLOCK_SIZE;
	if (root_offset > st.st_size ||
	    (off_t)sizeof(root_block) > st.st_size - root_offset) {
		warnx("root directory block is outside of image %s", path);
		goto out;
	}

	nread = pread(fd, root_block, sizeof(root_block), root_offset);
	if (nread == -1) {
		warn("pread %s", path);
		goto out;
	}
	if (nread != (ssize_t)sizeof(root_block)) {
		warnx("short read %s (%zd != %zu)", path, nread,
		    sizeof(root_block));
		goto out;
	}

	/* Logic here is a condensed cd9660_rrip_loop */
	root_entry = (const struct iso_directory_record *)root_block;
	record_len = isonum_711(root_entry->length);
	name_len = isonum_711(root_entry->name_len);

	su_offset = ISO_DIRECTORY_RECORD_SIZE + name_len;
	/* Even name len has one byte padding; see cd9660_rrip_loop. */
	if ((name_len & 1) == 0)
		su_offset++;
	su_len = record_len - su_offset;

	for (size_t susp_offset = 0; susp_offset < su_len;) {
		const ISO_SUSP_HEADER *header;
		size_t entry_len;

		if (su_len - susp_offset < sizeof(*header)) {
			warnx("SUSP header too small %s", path);
			goto out;
		}
		header = (const ISO_SUSP_HEADER *)(root_block + su_offset +
		    susp_offset);
		entry_len = isonum_711(header->length);

		if (entry_len < sizeof(*header) ||
		    entry_len > su_len - susp_offset) {
			warnx("invalid SUSP entry length %s", path);
			goto out;
		}

		/*
		 * Corrupt ISO_RRIP_EXTREF in the Extensions Reference (ER).
		 * See: cd9660_rrip_loop and cd9660_rrip_extref.
		 */
		if (isonum_711(header->version) == 1 &&
		    memcmp(header->type, "ER", 2) == 0) {
			off_t offset;
			const ISO_RRIP_EXTREF *extref;
			uint8_t bad_len = 4;

			/* Expected Rock Ridge but got something else. */
			if (entry_len < sizeof(ISO_RRIP_EXTREF)) {
				warnx(
				    "ER too short for Rock Ridge %s (%zu < %zu)",
				    path, entry_len, sizeof(ISO_RRIP_EXTREF));
				goto out;
			}

			extref = (const ISO_RRIP_EXTREF *)header;
			if (isonum_711(extref->version) != 1) {
				susp_offset += entry_len;
				continue;
			}

			offset = root_offset + su_offset + susp_offset +
			    offsetof(ISO_SUSP_HEADER, length);

			/*
			 * Corrupt the Rock Ridge length.
			 * This triggers the bug in PR296853 because the initial
			 * parse of RR fails. At that point, the outlen field is
			 * arbitrary bytes (NULL in the fix) because that's not
			 * one of the paths that write a buffer, which would
			 * require outlen.
			 */
			if (pwrite(fd, &bad_len, sizeof(bad_len), offset) !=
			    sizeof(bad_len)) {
				warn("pwrite to corrupt ISO %s", path);
				goto out;
			}

			error = 0;
			goto out;
		}

		susp_offset += entry_len;
	}

	warnx("did not find a valid ER entry in %s", path);
out:
	if (fd != -1)
		(void)close(fd);
	return (error);
}

int
main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s image\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (corrupt_rock_ridge(argv[1]) == -1) {
		errx(EXIT_FAILURE, "failed to corrupt Rock Ridge ISO: %s",
		    argv[1]);
	}

	return (EXIT_SUCCESS);
}
