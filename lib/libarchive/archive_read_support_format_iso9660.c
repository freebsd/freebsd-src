/*-
 * Copyright (c) 2003-2004 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"
__FBSDID("$FreeBSD$");

#include <sys/stat.h>

#include <errno.h>
/* #include <stdint.h> */ /* See archive_platform.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_string.h"

/*
 * An overview of ISO 9660 format:
 *
 * Each disk is laid out as follows:
 *   * 32k reserved for private use
 *   * Volume descriptor table.  Each volume descriptor
 *     is 2k and specifies basic format information.
 *     The "Primary Volume Descriptor" (PVD) is defined by the
 *     standard and should always be present; other volume
 *     descriptors include various vendor-specific extensions.
 *   * Files and directories.  Each file/dir is specified by
 *     an "extent" (starting sector and length in bytes).
 *     Dirs are just files with directory records packed one
 *     after another.  The PVD contains a single dir entry
 *     specifying the location of the root directory.  Everything
 *     else follows from there.
 *
 * This module works by first reading the volume descriptors, then
 * building a heap of directory entries, sorted by starting
 * sector.  At each step, I look for the earliest dir entry that
 * hasn't yet been read, seek forward to that location and read
 * that entry.  If it's a dir, I slurp in the new dir entries and
 * add them to the heap; if it's a regular file, I return the
 * corresponding archive_entry and wait for the client to request
 * the file body.  This strategy allows us to read most compliant
 * CDs with a single pass through the data, as required by libarchive.
 */

/* Structure of on-disk PVD. */
struct primary_volume_descriptor {
	unsigned char	type[1];
	char	id[5];
	unsigned char	version[1];
	char	reserved1[1];
	char	system_id[32];
	char	volume_id[32];
	char	reserved2[8];
	char	volume_space_size[8];
	char	reserved3[32];
	char	volume_set_size[4];
	char	volume_sequence_number[4];
	char	logical_block_size[4];
	char	path_table_size[8];
	char	type_1_path_table[4];
	char	opt_type_1_path_table[4];
	char	type_m_path_table[4];
	char	opt_type_m_path_table[4];
	char	root_directory_record[34];
	char	volume_set_id[128];
	char	publisher_id[128];
	char	preparer_id[128];
	char	application_id[128];
	char	copyright_file_id[37];
	char	abstract_file_id[37];
	char	bibliographic_file_id[37];
	char	creation_date[17];
	char	modification_date[17];
	char	expiration_date[17];
	char	effective_date[17];
	char	file_structure_version[1];
	char	reserved4[1];
	char	application_data[512];
};

/* Structure of an on-disk directory record. */
struct directory_record {
	unsigned char length[1];
	unsigned char ext_attr_length[1];
	unsigned char extent[8];
	unsigned char size[8];
	char date[7];
	unsigned char flags[1];
	unsigned char file_unit_size[1];
	unsigned char interleave[1];
	unsigned char volume_sequence_number[4];
	unsigned char name_len[1];
	char name[1];
};


/*
 * Our private data.
 */

/* In-memory storage for a directory record. */
struct dir_rec {
	struct dir_rec	*parent;
	int		 refcount;
	unsigned char	 flags;
	uint64_t	 offset;  /* Offset on disk. */
	uint64_t	 size;	/* File size in bytes. */
	time_t		 mtime;	/* File last modified time. */
	mode_t		 mode;
	char		 name[1]; /* Null-terminated filename. */
};


struct iso9660 {
	int	magic;
#define ISO9660_MAGIC   0x96609660
	int	bid; /* If non-zero, return this as our bid. */
	struct archive_string pathname;

	uint64_t	previous_offset;
	uint64_t	previous_size;
	struct archive_string previous_pathname;

	/* TODO: Make this a heap for fast inserts and deletions. */
	struct dir_rec **pending_files;
	int	pending_files_allocated;
	int	pending_files_used;

	uint64_t current_position;
	ssize_t	logical_block_size;

	off_t	entry_sparse_offset;
	ssize_t	entry_bytes_remaining;
};

static int	archive_read_format_iso9660_bid(struct archive *);
static int	archive_read_format_iso9660_cleanup(struct archive *);
static int	archive_read_format_iso9660_read_data(struct archive *,
		    const void **, size_t *, off_t *);
static int	archive_read_format_iso9660_read_header(struct archive *,
		    struct archive_entry *);
static const char *build_pathname(struct archive_string *, struct dir_rec *);
static void	dump_isodirent(FILE *, const struct directory_record *);
static time_t	isodate(const void *);
static int	isPVD(struct iso9660 *, const char *);
static struct dir_rec *next_entry(struct iso9660 *);
static void	release_dirrec(struct iso9660 *, struct dir_rec *);
static int	store_pending(struct iso9660 *, struct dir_rec *parent,
		    const struct directory_record *);
static int	toi(const void *p, int n);

int
archive_read_support_format_iso9660(struct archive *a)
{
	struct iso9660 *iso9660;
	int r;

	iso9660 = malloc(sizeof(*iso9660));
	memset(iso9660, 0, sizeof(*iso9660));
	iso9660->magic = ISO9660_MAGIC;
	iso9660->bid = -1; /* We haven't yet bid. */

	r = __archive_read_register_format(a,
	    iso9660,
	    archive_read_format_iso9660_bid,
	    archive_read_format_iso9660_read_header,
	    archive_read_format_iso9660_read_data,
	    archive_read_format_iso9660_cleanup);

	if (r != ARCHIVE_OK) {
		free(iso9660);
		return (r);
	}
	return (ARCHIVE_OK);
}


static int
archive_read_format_iso9660_bid(struct archive *a)
{
	struct iso9660 *iso9660;
	ssize_t bytes_read;
	const void *h;
	const char *p;

	iso9660 = *(a->pformat_data);

	if (iso9660->bid >= 0)
		return (iso9660->bid);

	/*
	 * Skip the first 32k (reserved area) and get the first
	 * 8 sectors of the volume descriptor table.  Of course,
	 * if the I/O layer gives us more, we'll take it.
	 */
	bytes_read = (a->compression_read_ahead)(a, &h, 32768 + 8*2048);
	if (bytes_read < 32768 + 8*2048)
	    return (iso9660->bid = -1);
	p = (const char *)h;

	/* Skip the reserved area. */
	bytes_read -= 32768;
	p += 32768;

	/* Check each volume descriptor to locate the PVD. */
	for (; bytes_read > 2048; bytes_read -= 2048, p += 2048) {
		iso9660->bid = isPVD(iso9660, p);
		if (iso9660->bid > 0)
			return (iso9660->bid);
		if (*p == '\xff') /* End-of-volume-descriptor marker. */
			break;
	}

	/* We didn't find a valid PVD; return a bid of zero. */
	iso9660->bid = 0;
	return (iso9660->bid);
}

static int
isPVD(struct iso9660 *iso9660, const char *h)
{
	const struct primary_volume_descriptor *voldesc;

	if (h[0] != 1)
		return (0);
	if (memcmp(h+1, "CD001", 5) != 0)
		return (0);


	voldesc = (const struct primary_volume_descriptor *)h;
	iso9660->logical_block_size = toi(&voldesc->logical_block_size, 2);

	/* Store the root directory in the pending list. */
	store_pending(iso9660, NULL,
	    (struct directory_record *)&voldesc->root_directory_record);
	return (48);
}

static int
archive_read_format_iso9660_read_header(struct archive *a,
    struct archive_entry *entry)
{
	struct stat st;
	struct iso9660 *iso9660;
	struct dir_rec *dirrec;
	ssize_t bytes_read;
	const void *buff;

	iso9660 = *(a->pformat_data);

	/* Get the next entry that appears after the current offset. */
	dirrec = next_entry(iso9660);
	if (dirrec == NULL)
		return (ARCHIVE_EOF);

	iso9660->entry_bytes_remaining = dirrec->size;
	iso9660->entry_sparse_offset = 0; /* Offset for sparse-file-aware clients. */

	/* Set up the entry structure with information about this entry. */
	memset(&st, 0, sizeof(st));
	st.st_mode = dirrec->mode;
	st.st_mtime = dirrec->mtime;
	st.st_size = iso9660->entry_bytes_remaining;
	archive_entry_copy_stat(entry, &st);
	archive_string_empty(&iso9660->pathname);
	archive_entry_set_pathname(entry,
	    build_pathname(&iso9660->pathname, dirrec));

	/* If this entry points to the same data as the previous
	 * entry, convert this into a hardlink to that entry.
	 * But don't bother for zero-length files. */
	if (dirrec->offset == iso9660->previous_offset
	    && dirrec->size == iso9660->previous_size
	    && dirrec->size > 0) {
		archive_entry_set_hardlink(entry,
		    iso9660->previous_pathname.s);
		iso9660->entry_bytes_remaining = 0;
		iso9660->entry_sparse_offset = 0;
		release_dirrec(iso9660, dirrec);
		return (ARCHIVE_OK);
	}

	/* If the offset is before our current position, we can't
	 * seek backwards to extract it, so issue a warning. */
	if (dirrec->offset < iso9660->current_position) {
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "Ignoring out-of-order file");
		iso9660->entry_bytes_remaining = 0;
		iso9660->entry_sparse_offset = 0;
		release_dirrec(iso9660, dirrec);
		return (ARCHIVE_WARN);
	}

	/* Seek forward to the start of the entry. */
	while (iso9660->current_position < dirrec->offset) {
		ssize_t step = dirrec->offset - iso9660->current_position;
		if (step > iso9660->logical_block_size)
			step = iso9660->logical_block_size;
		bytes_read = (a->compression_read_ahead)(a, &buff, step);
		if (bytes_read <= 0) {
			release_dirrec(iso9660, dirrec);
			return (ARCHIVE_FATAL);
		}
		if (bytes_read > step)
			bytes_read = step;
		iso9660->current_position += bytes_read;
		(a->compression_read_consume)(a, bytes_read);
	}

	iso9660->previous_size = dirrec->size;
	iso9660->previous_offset = dirrec->offset;
	archive_strcpy(&iso9660->previous_pathname, iso9660->pathname.s);

	/* If this is a directory, read in all of the entries right now. */
	if (S_ISDIR(st.st_mode)) {
		while(iso9660->entry_bytes_remaining > 0) {
			const void *block;
			const unsigned char *p;
			ssize_t step = iso9660->logical_block_size;
			if (step > iso9660->entry_bytes_remaining)
				step = iso9660->entry_bytes_remaining;
			bytes_read = (a->compression_read_ahead)(a, &block, step);
			if (bytes_read < step) {
				archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    "Failed to read full block when scanning ISO9660 directory list");
				release_dirrec(iso9660, dirrec);
				return (ARCHIVE_FATAL);
			}
			if (bytes_read > step)
				bytes_read = step;
			(a->compression_read_consume)(a, bytes_read);
			iso9660->current_position += bytes_read;
			iso9660->entry_bytes_remaining -= bytes_read;
			for (p = block;
			     *p != 0 && p < (const unsigned char *)block + bytes_read;
			     p += *p) {
				const struct directory_record *dr
				    = (const struct directory_record *)p;
				/* Skip '.' entry. */
				if (dr->name_len[0] == 1
				    && dr->name[0] == '\0')
					continue;
				/* Skip '..' entry. */
				if (dr->name_len[0] == 1
				    && dr->name[0] == '\001')
					continue;
				store_pending(iso9660, dirrec, dr);
			}
		}
	}

	release_dirrec(iso9660, dirrec);
	return (ARCHIVE_OK);
}

static int
archive_read_format_iso9660_read_data(struct archive *a,
    const void **buff, size_t *size, off_t *offset)
{
	ssize_t bytes_read;
	struct iso9660 *iso9660;

	iso9660 = *(a->pformat_data);
	if (iso9660->entry_bytes_remaining <= 0) {
		*buff = NULL;
		*size = 0;
		*offset = iso9660->entry_sparse_offset;
		return (ARCHIVE_EOF);
	}

	bytes_read = (a->compression_read_ahead)(a, buff, 1);
	if (bytes_read <= 0)
		return (ARCHIVE_FATAL);
	if (bytes_read > iso9660->entry_bytes_remaining)
		bytes_read = iso9660->entry_bytes_remaining;
	*size = bytes_read;
	*offset = iso9660->entry_sparse_offset;
	iso9660->entry_sparse_offset += bytes_read;
	iso9660->entry_bytes_remaining -= bytes_read;
	iso9660->current_position += bytes_read;
	(a->compression_read_consume)(a, bytes_read);
	return (ARCHIVE_OK);
}

static int
archive_read_format_iso9660_cleanup(struct archive *a)
{
	struct iso9660 *iso9660;

	iso9660 = *(a->pformat_data);
	free(iso9660);
	*(a->pformat_data) = NULL;
	return (ARCHIVE_OK);
}

static int
store_pending(struct iso9660 *iso9660, struct dir_rec *parent,
    const struct directory_record *isodirent)
{
	struct dir_rec *new_dirent;

	if (iso9660->pending_files_used >= iso9660->pending_files_allocated) {
		struct dir_rec **new_pending_files;
		int new_size = iso9660->pending_files_allocated * 2;

		if (new_size < 1024)
			new_size = 1024;
		new_pending_files = malloc(new_size * sizeof(new_pending_files[0]));
		memcpy(new_pending_files, iso9660->pending_files,
		    iso9660->pending_files_allocated * sizeof(new_pending_files[0]));
		if (iso9660->pending_files != NULL)
			free(iso9660->pending_files);
		iso9660->pending_files = new_pending_files;
		iso9660->pending_files_allocated = new_size;
	}

	new_dirent = malloc(sizeof(*new_dirent) + isodirent->name_len[0] + 1);
	new_dirent->parent = parent;
	if (parent != NULL)
		parent->refcount++;
	new_dirent->refcount = 0;
	new_dirent->flags = isodirent->flags[0];
	new_dirent->offset = toi(isodirent->extent, 4)
	    * iso9660->logical_block_size;
	new_dirent->size = toi(isodirent->size, 4);
	new_dirent->mtime = isodate(isodirent->date);
	memcpy(new_dirent->name, isodirent->name, isodirent->name_len[0]);
	new_dirent->name[(int)isodirent->name_len[0]] = '\0';

	if (isodirent->flags[0] & 0x02)
		new_dirent->mode = S_IFDIR | 0700;
	else
		new_dirent->mode = S_IFREG | 0400;

	iso9660->pending_files[iso9660->pending_files_used++] = new_dirent;


	/* DEBUGGING: Warn about attributes I don't yet fully support. */
	if ((isodirent->flags[0] & ~0x02) != 0) {
		fprintf(stderr, "\n ** Unrecognized flag: ");
		dump_isodirent(stderr, isodirent);
		fprintf(stderr, "\n");
	} else if (toi(isodirent->volume_sequence_number, 2) != 1) {
		fprintf(stderr, "\n ** Unrecognized sequence number: ");
		dump_isodirent(stderr, isodirent);
		fprintf(stderr, "\n");
	} else if (isodirent->file_unit_size[0] != 0) {
		fprintf(stderr, "\n ** Unexpected file unit size: ");
		dump_isodirent(stderr, isodirent);
		fprintf(stderr, "\n");
	} else if (isodirent->interleave[0] != 0) {
		fprintf(stderr, "\n ** Unexpected interleave: ");
		dump_isodirent(stderr, isodirent);
		fprintf(stderr, "\n");
	} else if (isodirent->ext_attr_length[0] != 0) {
		fprintf(stderr, "\n ** Unexpected extended attribute length: ");
		dump_isodirent(stderr, isodirent);
		fprintf(stderr, "\n");
	}

	return (ARCHIVE_OK);
}

static void
release_dirrec(struct iso9660 *iso9660, struct dir_rec *dr)
{
	struct dir_rec *parent;

	if (dr->refcount == 0) {
		dr->flags = 0xff;
		parent = dr->parent;
		free(dr);
		if (parent != NULL) {
			parent->refcount--;
			release_dirrec(iso9660, parent);
		}
	}
}

static struct dir_rec *
next_entry(struct iso9660 *iso9660)
{
	int least_index;
	uint64_t least_end_offset;
	int i;
	struct dir_rec *r;

	if (iso9660->pending_files_used < 1)
		return (NULL);

	/* Assume the first file in the list is the earliest on disk. */
	least_index = 0;
	least_end_offset = iso9660->pending_files[0]->offset
	    + iso9660->pending_files[0]->size;

	/* Now, try to find an earlier one. */
	for(i = 0; i < iso9660->pending_files_used; i++) {
		/* Use the position of the file *end* as our comparison. */
		uint64_t end_offset = iso9660->pending_files[i]->offset
		    + iso9660->pending_files[i]->size;
		if (least_end_offset > end_offset) {
			least_index = i;
			least_end_offset = end_offset;
		}
	}
	r = iso9660->pending_files[least_index];
	iso9660->pending_files[least_index]
	    = iso9660->pending_files[--iso9660->pending_files_used];
	return (r);
}

static int
toi(const void *p, int n)
{
	const unsigned char *v = (const unsigned char *)p;
	if (n > 1)
		return v[0] + 256 * toi(v + 1, n - 1);
	if (n == 1)
		return v[0];
	return (0);
}

static time_t
isodate(const void *p)
{
	struct tm tm;
	const unsigned char *v = (const unsigned char *)p;
	int offset;
	tm.tm_year = v[0];
	tm.tm_mon = v[1] - 1;
	tm.tm_mday = v[2];
	tm.tm_hour = v[3];
	tm.tm_min = v[4];
	tm.tm_sec = v[5];
	/* v[6] is the timezone offset, in 1/4-hour increments. */
	offset = ((const signed char *)p)[6];
	tm.tm_hour -= offset / 4;
	tm.tm_min -= (offset % 4) * 15;
	return (timegm(&tm));
}

static const char *
build_pathname(struct archive_string *as, struct dir_rec *dr)
{
	if (dr->parent != NULL && dr->parent->name[0] != '\0') {
		build_pathname(as, dr->parent);
		archive_strcat(as, "/");
	}
	archive_strcat(as, dr->name);
	return (as->s);
}

static void
dump_isodirent(FILE *out, const struct directory_record *isodirent)
{
	fprintf(out, " l %d,", isodirent->length[0]);
	fprintf(out, " a %d,", isodirent->ext_attr_length[0]);
	fprintf(out, " ext 0x%x,", toi(isodirent->extent, 4));
	fprintf(out, " s %d,", toi(isodirent->size, 4));
	fprintf(out, " f 0x%02x,", isodirent->flags[0]);
	fprintf(out, " u %d,", isodirent->file_unit_size[0]);
	fprintf(out, " ilv %d,", isodirent->interleave[0]);
	fprintf(out, " seq %d,", toi(isodirent->volume_sequence_number,2));
	fprintf(out, " nl %d:", isodirent->name_len[0]);
	fprintf(out, " `%.*s'", isodirent->name_len[0], isodirent->name);
}
