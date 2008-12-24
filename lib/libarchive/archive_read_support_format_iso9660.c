/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
/* #include <stdint.h> */ /* See archive_platform.h */
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <time.h>

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_private.h"
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
 * building a list of directory entries, sorted by starting
 * sector.  At each step, I look for the earliest dir entry that
 * hasn't yet been read, seek forward to that location and read
 * that entry.  If it's a dir, I slurp in the new dir entries and
 * add them to the heap; if it's a regular file, I return the
 * corresponding archive_entry and wait for the client to request
 * the file body.  This strategy allows us to read most compliant
 * CDs with a single pass through the data, as required by libarchive.
 */

/* Structure of on-disk primary volume descriptor. */
#define PVD_type_offset 0
#define PVD_type_size 1
#define PVD_id_offset (PVD_type_offset + PVD_type_size)
#define PVD_id_size 5
#define PVD_version_offset (PVD_id_offset + PVD_id_size)
#define PVD_version_size 1
#define PVD_reserved1_offset (PVD_version_offset + PVD_version_size)
#define PVD_reserved1_size 1
#define PVD_system_id_offset (PVD_reserved1_offset + PVD_reserved1_size)
#define PVD_system_id_size 32
#define PVD_volume_id_offset (PVD_system_id_offset + PVD_system_id_size)
#define PVD_volume_id_size 32
#define PVD_reserved2_offset (PVD_volume_id_offset + PVD_volume_id_size)
#define PVD_reserved2_size 8
#define PVD_volume_space_size_offset (PVD_reserved2_offset + PVD_reserved2_size)
#define PVD_volume_space_size_size 8
#define PVD_reserved3_offset (PVD_volume_space_size_offset + PVD_volume_space_size_size)
#define PVD_reserved3_size 32
#define PVD_volume_set_size_offset (PVD_reserved3_offset + PVD_reserved3_size)
#define PVD_volume_set_size_size 4
#define PVD_volume_sequence_number_offset (PVD_volume_set_size_offset + PVD_volume_set_size_size)
#define PVD_volume_sequence_number_size 4
#define PVD_logical_block_size_offset (PVD_volume_sequence_number_offset + PVD_volume_sequence_number_size)
#define PVD_logical_block_size_size 4
#define PVD_path_table_size_offset (PVD_logical_block_size_offset + PVD_logical_block_size_size)
#define PVD_path_table_size_size 8
#define PVD_type_1_path_table_offset (PVD_path_table_size_offset + PVD_path_table_size_size)
#define PVD_type_1_path_table_size 4
#define PVD_opt_type_1_path_table_offset (PVD_type_1_path_table_offset + PVD_type_1_path_table_size)
#define PVD_opt_type_1_path_table_size 4
#define PVD_type_m_path_table_offset (PVD_opt_type_1_path_table_offset + PVD_opt_type_1_path_table_size)
#define PVD_type_m_path_table_size 4
#define PVD_opt_type_m_path_table_offset (PVD_type_m_path_table_offset + PVD_type_m_path_table_size)
#define PVD_opt_type_m_path_table_size 4
#define PVD_root_directory_record_offset (PVD_opt_type_m_path_table_offset + PVD_opt_type_m_path_table_size)
#define PVD_root_directory_record_size 34
#define PVD_volume_set_id_offset (PVD_root_directory_record_offset + PVD_root_directory_record_size)
#define PVD_volume_set_id_size 128
#define PVD_publisher_id_offset (PVD_volume_set_id_offset + PVD_volume_set_id_size)
#define PVD_publisher_id_size 128
#define PVD_preparer_id_offset (PVD_publisher_id_offset + PVD_publisher_id_size)
#define PVD_preparer_id_size 128
#define PVD_application_id_offset (PVD_preparer_id_offset + PVD_preparer_id_size)
#define PVD_application_id_size 128
#define PVD_copyright_file_id_offset (PVD_application_id_offset + PVD_application_id_size)
#define PVD_copyright_file_id_size 37
#define PVD_abstract_file_id_offset (PVD_copyright_file_id_offset + PVD_copyright_file_id_size)
#define PVD_abstract_file_id_size 37
#define PVD_bibliographic_file_id_offset (PVD_abstract_file_id_offset + PVD_abstract_file_id_size)
#define PVD_bibliographic_file_id_size 37
#define PVD_creation_date_offset (PVD_bibliographic_file_id_offset + PVD_bibliographic_file_id_size)
#define PVD_creation_date_size 17
#define PVD_modification_date_offset (PVD_creation_date_offset + PVD_creation_date_size)
#define PVD_modification_date_size 17
#define PVD_expiration_date_offset (PVD_modification_date_offset + PVD_modification_date_size)
#define PVD_expiration_date_size 17
#define PVD_effective_date_offset (PVD_expiration_date_offset + PVD_expiration_date_size)
#define PVD_effective_date_size 17
#define PVD_file_structure_version_offset (PVD_effective_date_offset + PVD_effective_date_size)
#define PVD_file_structure_version_size 1
#define PVD_reserved4_offset (PVD_file_structure_version_offset + PVD_file_structure_version_size)
#define PVD_reserved4_size 1
#define PVD_application_data_offset (PVD_reserved4_offset + PVD_reserved4_size)
#define PVD_application_data_size 512
#define PVD_reserved5_offset (PVD_application_data_offset + PVD_application_data_size)
#define PVD_reserved5_size (2048 - PVD_reserved5_offset)

/* TODO: It would make future maintenance easier to just hardcode the
 * above values.  In particular, ECMA119 states the offsets as part of
 * the standard.  That would eliminate the need for the following check.*/
#if PVD_reserved5_offset != 1395
#error PVD offset and size definitions are wrong.
#endif

/* Structure of an on-disk directory record. */
/* Note:  ISO9660 stores each multi-byte integer twice, once in
 * each byte order.  The sizes here are the size of just one
 * of the two integers.  (This is why the offset of a field isn't
 * the same as the offset+size of the previous field.) */
#define DR_length_offset 0
#define DR_length_size 1
#define DR_ext_attr_length_offset 1
#define DR_ext_attr_length_size 1
#define DR_extent_offset 2
#define DR_extent_size 4
#define DR_size_offset 10
#define DR_size_size 4
#define DR_date_offset 18
#define DR_date_size 7
#define DR_flags_offset 25
#define DR_flags_size 1
#define DR_file_unit_size_offset 26
#define DR_file_unit_size_size 1
#define DR_interleave_offset 27
#define DR_interleave_size 1
#define DR_volume_sequence_number_offset 28
#define DR_volume_sequence_number_size 2
#define DR_name_len_offset 32
#define DR_name_len_size 1
#define DR_name_offset 33

/*
 * Our private data.
 */

/* In-memory storage for a directory record. */
struct file_info {
	struct file_info	*parent;
	int		 refcount;
	uint64_t	 offset;  /* Offset on disk. */
	uint64_t	 size;	/* File size in bytes. */
	uint64_t	 ce_offset; /* Offset of CE */
	uint64_t	 ce_size; /* Size of CE */
	time_t		 birthtime; /* File created time. */
	time_t		 mtime;	/* File last modified time. */
	time_t		 atime;	/* File last accessed time. */
	time_t		 ctime;	/* File attribute change time. */
	uint64_t	 rdev; /* Device number */
	mode_t		 mode;
	uid_t		 uid;
	gid_t		 gid;
	ino_t		 inode;
	int		 nlinks;
	struct archive_string name; /* Pathname */
	char		 name_continues; /* Non-zero if name continues */
	struct archive_string symlink;
	char		 symlink_continues; /* Non-zero if link continues */
};


struct iso9660 {
	int	magic;
#define ISO9660_MAGIC   0x96609660
	struct archive_string pathname;
	char	seenRockridge; /* Set true if RR extensions are used. */
	unsigned char	suspOffset;

	uint64_t	previous_offset;
	uint64_t	previous_size;
	struct archive_string previous_pathname;

	/* TODO: Make this a heap for fast inserts and deletions. */
	struct file_info **pending_files;
	int	pending_files_allocated;
	int	pending_files_used;

	uint64_t current_position;
	ssize_t	logical_block_size;
	uint64_t volume_size; /* Total size of volume in bytes. */

	off_t	entry_sparse_offset;
	int64_t	entry_bytes_remaining;
};

static void	add_entry(struct iso9660 *iso9660, struct file_info *file);
static int	archive_read_format_iso9660_bid(struct archive_read *);
static int	archive_read_format_iso9660_cleanup(struct archive_read *);
static int	archive_read_format_iso9660_read_data(struct archive_read *,
		    const void **, size_t *, off_t *);
static int	archive_read_format_iso9660_read_data_skip(struct archive_read *);
static int	archive_read_format_iso9660_read_header(struct archive_read *,
		    struct archive_entry *);
static const char *build_pathname(struct archive_string *, struct file_info *);
#if DEBUG
static void	dump_isodirrec(FILE *, const unsigned char *isodirrec);
#endif
static time_t	time_from_tm(struct tm *);
static time_t	isodate17(const unsigned char *);
static time_t	isodate7(const unsigned char *);
static int	isPVD(struct iso9660 *, const unsigned char *);
static struct file_info *next_entry(struct iso9660 *);
static int	next_entry_seek(struct archive_read *a, struct iso9660 *iso9660,
		    struct file_info **pfile);
static struct file_info *
		parse_file_info(struct iso9660 *iso9660,
		    struct file_info *parent, const unsigned char *isodirrec);
static void	parse_rockridge(struct iso9660 *iso9660,
		    struct file_info *file, const unsigned char *start,
		    const unsigned char *end);
static void	parse_rockridge_NM1(struct file_info *,
		    const unsigned char *, int);
static void	parse_rockridge_SL1(struct file_info *,
		    const unsigned char *, int);
static void	parse_rockridge_TF1(struct file_info *,
		    const unsigned char *, int);
static void	release_file(struct iso9660 *, struct file_info *);
static unsigned	toi(const void *p, int n);

int
archive_read_support_format_iso9660(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct iso9660 *iso9660;
	int r;

	iso9660 = (struct iso9660 *)malloc(sizeof(*iso9660));
	if (iso9660 == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Can't allocate iso9660 data");
		return (ARCHIVE_FATAL);
	}
	memset(iso9660, 0, sizeof(*iso9660));
	iso9660->magic = ISO9660_MAGIC;

	r = __archive_read_register_format(a,
	    iso9660,
	    archive_read_format_iso9660_bid,
	    archive_read_format_iso9660_read_header,
	    archive_read_format_iso9660_read_data,
	    archive_read_format_iso9660_read_data_skip,
	    archive_read_format_iso9660_cleanup);

	if (r != ARCHIVE_OK) {
		free(iso9660);
		return (r);
	}
	return (ARCHIVE_OK);
}


static int
archive_read_format_iso9660_bid(struct archive_read *a)
{
	struct iso9660 *iso9660;
	ssize_t bytes_read;
	const void *h;
	const unsigned char *p;
	int bid;

	iso9660 = (struct iso9660 *)(a->format->data);

	/*
	 * Skip the first 32k (reserved area) and get the first
	 * 8 sectors of the volume descriptor table.  Of course,
	 * if the I/O layer gives us more, we'll take it.
	 */
	h = __archive_read_ahead(a, 32768 + 8*2048, &bytes_read);
	if (h == NULL)
	    return (-1);
	p = (const unsigned char *)h;

	/* Skip the reserved area. */
	bytes_read -= 32768;
	p += 32768;

	/* Check each volume descriptor to locate the PVD. */
	for (; bytes_read > 2048; bytes_read -= 2048, p += 2048) {
		bid = isPVD(iso9660, p);
		if (bid > 0)
			return (bid);
		if (*p == '\177') /* End-of-volume-descriptor marker. */
			break;
	}

	/* We didn't find a valid PVD; return a bid of zero. */
	return (0);
}

static int
isPVD(struct iso9660 *iso9660, const unsigned char *h)
{
	struct file_info *file;
	int i;

	/* Type of the Primary Volume Descriptor must be 1. */
	if (h[PVD_type_offset] != 1)
		return (0);

	/* ID must be "CD001" */
	if (memcmp(h + PVD_id_offset, "CD001", 5) != 0)
		return (0);

	/* PVD version must be 1. */
	if (h[PVD_version_offset] != 1)
		return (0);

	/* Reserved field must be 0. */
	if (h[PVD_reserved1_offset] != 0)
		return (0);

	/* Reserved field must be 0. */
	for (i = 0; i < PVD_reserved2_size; ++i)
		if (h[PVD_reserved2_offset + i] != 0)
			return (0);

	/* Reserved field must be 0. */
	for (i = 0; i < PVD_reserved3_size; ++i)
		if (h[PVD_reserved3_offset + i] != 0)
			return (0);

	/* Logical block size must be > 0. */
	/* I've looked at Ecma 119 and can't find any stronger
	 * restriction on this field. */
	iso9660->logical_block_size = toi(h + PVD_logical_block_size_offset, 2);
	if (iso9660->logical_block_size <= 0)
		return (0);

	iso9660->volume_size = iso9660->logical_block_size
	    * (uint64_t)toi(h + PVD_volume_space_size_offset, 4);

	/* File structure version must be 1 for ISO9660/ECMA119. */
	if (h[PVD_file_structure_version_offset] != 1)
		return (0);


	/* Reserved field must be 0. */
	for (i = 0; i < PVD_reserved4_size; ++i)
		if (h[PVD_reserved4_offset + i] != 0)
			return (0);

	/* Reserved field must be 0. */
	for (i = 0; i < PVD_reserved5_size; ++i)
		if (h[PVD_reserved5_offset + i] != 0)
			return (0);

	/* XXX TODO: Check other values for sanity; reject more
	 * malformed PVDs. XXX */

	/* Store the root directory in the pending list. */
	file = parse_file_info(iso9660, NULL, h + PVD_root_directory_record_offset);
	add_entry(iso9660, file);
	return (48);
}

static int
archive_read_format_iso9660_read_header(struct archive_read *a,
    struct archive_entry *entry)
{
	struct iso9660 *iso9660;
	struct file_info *file;
	int r;

	iso9660 = (struct iso9660 *)(a->format->data);

	if (!a->archive.archive_format) {
		a->archive.archive_format = ARCHIVE_FORMAT_ISO9660;
		a->archive.archive_format_name = "ISO9660";
	}

	/* Get the next entry that appears after the current offset. */
	r = next_entry_seek(a, iso9660, &file);
	if (r != ARCHIVE_OK)
		return (r);

	iso9660->entry_bytes_remaining = file->size;
	iso9660->entry_sparse_offset = 0; /* Offset for sparse-file-aware clients. */

	if (file->offset + file->size > iso9660->volume_size) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "File is beyond end-of-media: %s", file->name);
		iso9660->entry_bytes_remaining = 0;
		iso9660->entry_sparse_offset = 0;
		release_file(iso9660, file);
		return (ARCHIVE_WARN);
	}

	/* Set up the entry structure with information about this entry. */
	archive_entry_set_mode(entry, file->mode);
	archive_entry_set_uid(entry, file->uid);
	archive_entry_set_gid(entry, file->gid);
	archive_entry_set_nlink(entry, file->nlinks);
	archive_entry_set_ino(entry, file->inode);
	archive_entry_set_birthtime(entry, file->birthtime, 0);
	archive_entry_set_mtime(entry, file->mtime, 0);
	archive_entry_set_ctime(entry, file->ctime, 0);
	archive_entry_set_atime(entry, file->atime, 0);
	/* N.B.: Rock Ridge supports 64-bit device numbers. */
	archive_entry_set_rdev(entry, (dev_t)file->rdev);
	archive_entry_set_size(entry, iso9660->entry_bytes_remaining);
	archive_string_empty(&iso9660->pathname);
	archive_entry_set_pathname(entry,
	    build_pathname(&iso9660->pathname, file));
	if (file->symlink.s != NULL)
		archive_entry_copy_symlink(entry, file->symlink.s);

	/* If this entry points to the same data as the previous
	 * entry, convert this into a hardlink to that entry.
	 * But don't bother for zero-length files. */
	if (file->offset == iso9660->previous_offset
	    && file->size == iso9660->previous_size
	    && file->size > 0) {
		archive_entry_set_hardlink(entry,
		    iso9660->previous_pathname.s);
		iso9660->entry_bytes_remaining = 0;
		iso9660->entry_sparse_offset = 0;
		release_file(iso9660, file);
		return (ARCHIVE_OK);
	}

	/* If the offset is before our current position, we can't
	 * seek backwards to extract it, so issue a warning. */
	if (file->offset < iso9660->current_position) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Ignoring out-of-order file");
		iso9660->entry_bytes_remaining = 0;
		iso9660->entry_sparse_offset = 0;
		release_file(iso9660, file);
		return (ARCHIVE_WARN);
	}

	iso9660->previous_size = file->size;
	iso9660->previous_offset = file->offset;
	archive_strcpy(&iso9660->previous_pathname, iso9660->pathname.s);

	/* If this is a directory, read in all of the entries right now. */
	if (archive_entry_filetype(entry) == AE_IFDIR) {
		while (iso9660->entry_bytes_remaining > 0) {
			const void *block;
			const unsigned char *p;
			ssize_t step = iso9660->logical_block_size;
			if (step > iso9660->entry_bytes_remaining)
				step = iso9660->entry_bytes_remaining;
			block = __archive_read_ahead(a, step, NULL);
			if (block == NULL) {
				archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
	    "Failed to read full block when scanning ISO9660 directory list");
				release_file(iso9660, file);
				return (ARCHIVE_FATAL);
			}
			__archive_read_consume(a, step);
			iso9660->current_position += step;
			iso9660->entry_bytes_remaining -= step;
			for (p = (const unsigned char *)block;
			     *p != 0 && p < (const unsigned char *)block + step;
			     p += *p) {
				struct file_info *child;

				/* Skip '.' entry. */
				if (*(p + DR_name_len_offset) == 1
				    && *(p + DR_name_offset) == '\0')
					continue;
				/* Skip '..' entry. */
				if (*(p + DR_name_len_offset) == 1
				    && *(p + DR_name_offset) == '\001')
					continue;
				child = parse_file_info(iso9660, file, p);
				add_entry(iso9660, child);
				if (iso9660->seenRockridge) {
					a->archive.archive_format =
					    ARCHIVE_FORMAT_ISO9660_ROCKRIDGE;
					a->archive.archive_format_name =
					    "ISO9660 with Rockridge extensions";
				}
			}
		}
	}

	release_file(iso9660, file);
	return (ARCHIVE_OK);
}

static int
archive_read_format_iso9660_read_data_skip(struct archive_read *a)
{
	/* Because read_next_header always does an explicit skip
	 * to the next entry, we don't need to do anything here. */
	(void)a; /* UNUSED */
	return (ARCHIVE_OK);
}

static int
archive_read_format_iso9660_read_data(struct archive_read *a,
    const void **buff, size_t *size, off_t *offset)
{
	ssize_t bytes_read;
	struct iso9660 *iso9660;

	iso9660 = (struct iso9660 *)(a->format->data);
	if (iso9660->entry_bytes_remaining <= 0) {
		*buff = NULL;
		*size = 0;
		*offset = iso9660->entry_sparse_offset;
		return (ARCHIVE_EOF);
	}

	*buff = __archive_read_ahead(a, 1, &bytes_read);
	if (bytes_read == 0)
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Truncated input file");
	if (buff == NULL)
		return (ARCHIVE_FATAL);
	if (bytes_read > iso9660->entry_bytes_remaining)
		bytes_read = iso9660->entry_bytes_remaining;
	*size = bytes_read;
	*offset = iso9660->entry_sparse_offset;
	iso9660->entry_sparse_offset += bytes_read;
	iso9660->entry_bytes_remaining -= bytes_read;
	iso9660->current_position += bytes_read;
	__archive_read_consume(a, bytes_read);
	return (ARCHIVE_OK);
}

static int
archive_read_format_iso9660_cleanup(struct archive_read *a)
{
	struct iso9660 *iso9660;
	struct file_info *file;

	iso9660 = (struct iso9660 *)(a->format->data);
	while ((file = next_entry(iso9660)) != NULL)
		release_file(iso9660, file);
	archive_string_free(&iso9660->pathname);
	archive_string_free(&iso9660->previous_pathname);
	if (iso9660->pending_files)
		free(iso9660->pending_files);
	free(iso9660);
	(a->format->data) = NULL;
	return (ARCHIVE_OK);
}

/*
 * This routine parses a single ISO directory record, makes sense
 * of any extensions, and stores the result in memory.
 */
static struct file_info *
parse_file_info(struct iso9660 *iso9660, struct file_info *parent,
    const unsigned char *isodirrec)
{
	struct file_info *file;
	size_t name_len;
	const unsigned char *rr_start, *rr_end;
	const char *p;
	int flags;

	/* TODO: Sanity check that name_len doesn't exceed length, etc. */

	/* Create a new file entry and copy data from the ISO dir record. */
	file = (struct file_info *)malloc(sizeof(*file));
	if (file == NULL)
		return (NULL);
	memset(file, 0, sizeof(*file));
	file->parent = parent;
	if (parent != NULL)
		parent->refcount++;
	file->offset = toi(isodirrec + DR_extent_offset, DR_extent_size)
	    * iso9660->logical_block_size;
	file->size = toi(isodirrec + DR_size_offset, DR_size_size);
	file->mtime = isodate7(isodirrec + DR_date_offset);
	file->ctime = file->atime = file->mtime;

	name_len = (size_t)*(const unsigned char *)(isodirrec + DR_name_len_offset);
	p = isodirrec + DR_name_offset;
	/* Rockridge extensions (if any) follow name.  Compute this
	 * before fidgeting the name_len below. */
	rr_start = p + name_len + (name_len & 1 ? 0 : 1) + iso9660->suspOffset;
	rr_end = (const unsigned char *)isodirrec
	    + *(isodirrec + DR_length_offset);

	/* Chop off trailing ';1' from files. */
	if (name_len > 2 && p[name_len - 1] == '1' && p[name_len - 2] == ';')
		name_len -= 2;
	/* Chop off trailing '.' from filenames. */
	if (name_len > 1 && p[name_len - 1] == '.')
		--name_len;
	archive_strncpy(&file->name, p, name_len);

	flags = *(isodirrec + DR_flags_offset);
	if (flags & 0x02)
		file->mode = AE_IFDIR | 0700;
	else
		file->mode = AE_IFREG | 0400;

	/* Rockridge extensions overwrite information from above. */
	parse_rockridge(iso9660, file, rr_start, rr_end);

#if DEBUG
	/* DEBUGGING: Warn about attributes I don't yet fully support. */
	if ((flags & ~0x02) != 0) {
		fprintf(stderr, "\n ** Unrecognized flag: ");
		dump_isodirrec(stderr, isodirrec);
		fprintf(stderr, "\n");
	} else if (toi(isodirrec + DR_volume_sequence_number_offset, 2) != 1) {
		fprintf(stderr, "\n ** Unrecognized sequence number: ");
		dump_isodirrec(stderr, isodirrec);
		fprintf(stderr, "\n");
	} else if (*(isodirrec + DR_file_unit_size_offset) != 0) {
		fprintf(stderr, "\n ** Unexpected file unit size: ");
		dump_isodirrec(stderr, isodirrec);
		fprintf(stderr, "\n");
	} else if (*(isodirrec + DR_interleave_offset) != 0) {
		fprintf(stderr, "\n ** Unexpected interleave: ");
		dump_isodirrec(stderr, isodirrec);
		fprintf(stderr, "\n");
	} else if (*(isodirrec + DR_ext_attr_length_offset) != 0) {
		fprintf(stderr, "\n ** Unexpected extended attribute length: ");
		dump_isodirrec(stderr, isodirrec);
		fprintf(stderr, "\n");
	}
#endif
	return (file);
}

static void
add_entry(struct iso9660 *iso9660, struct file_info *file)
{
	/* Expand our pending files list as necessary. */
	if (iso9660->pending_files_used >= iso9660->pending_files_allocated) {
		struct file_info **new_pending_files;
		int new_size = iso9660->pending_files_allocated * 2;

		if (iso9660->pending_files_allocated < 1024)
			new_size = 1024;
		/* Overflow might keep us from growing the list. */
		if (new_size <= iso9660->pending_files_allocated)
			__archive_errx(1, "Out of memory");
		new_pending_files = (struct file_info **)malloc(new_size * sizeof(new_pending_files[0]));
		if (new_pending_files == NULL)
			__archive_errx(1, "Out of memory");
		memcpy(new_pending_files, iso9660->pending_files,
		    iso9660->pending_files_allocated * sizeof(new_pending_files[0]));
		if (iso9660->pending_files != NULL)
			free(iso9660->pending_files);
		iso9660->pending_files = new_pending_files;
		iso9660->pending_files_allocated = new_size;
	}

	iso9660->pending_files[iso9660->pending_files_used++] = file;
}

static void
parse_rockridge(struct iso9660 *iso9660, struct file_info *file,
    const unsigned char *p, const unsigned char *end)
{
	(void)iso9660; /* UNUSED */
	file->name_continues = 0;
	file->symlink_continues = 0;

	while (p + 4 < end  /* Enough space for another entry. */
	    && p[0] >= 'A' && p[0] <= 'Z' /* Sanity-check 1st char of name. */
	    && p[1] >= 'A' && p[1] <= 'Z' /* Sanity-check 2nd char of name. */
	    && p[2] >= 4 /* Sanity-check length. */
	    && p + p[2] <= end) { /* Sanity-check length. */
		const unsigned char *data = p + 4;
		int data_length = p[2] - 4;
		int version = p[3];

		/*
		 * Yes, each 'if' here does test p[0] again.
		 * Otherwise, the fall-through handling to catch
		 * unsupported extensions doesn't work.
		 */
		switch(p[0]) {
		case 'C':
			if (p[0] == 'C' && p[1] == 'E') {
				if (version == 1 && data_length == 24) {
					/*
					 * CE extension comprises:
					 *   8 byte sector containing extension
					 *   8 byte offset w/in above sector
					 *   8 byte length of continuation
					 */
					file->ce_offset = (uint64_t)toi(data, 4)
					    * iso9660->logical_block_size
					    + toi(data + 8, 4);
					file->ce_size = toi(data + 16, 4);
					/* If the result is rediculous,
					 * ignore it. */
					if (file->ce_offset + file->ce_size
					    > iso9660->volume_size) {
						file->ce_offset = 0;
						file->ce_size = 0;
					}
				}
				break;
			}
			/* FALLTHROUGH */
		case 'N':
			if (p[0] == 'N' && p[1] == 'M') {
				if (version == 1)
					parse_rockridge_NM1(file,
					    data, data_length);
				break;
			}
			/* FALLTHROUGH */
		case 'P':
			if (p[0] == 'P' && p[1] == 'D') {
				/*
				 * PD extension is padding;
				 * contents are always ignored.
				 */
				break;
			}
			if (p[0] == 'P' && p[1] == 'N') {
				if (version == 1 && data_length == 16) {
					file->rdev = toi(data,4);
					file->rdev <<= 32;
					file->rdev |= toi(data + 8, 4);
				}
				break;
			}
			if (p[0] == 'P' && p[1] == 'X') {
				/*
				 * PX extension comprises:
				 *   8 bytes for mode,
				 *   8 bytes for nlinks,
				 *   8 bytes for uid,
				 *   8 bytes for gid,
				 *   8 bytes for inode.
				 */
				if (version == 1) {
					if (data_length >= 8)
						file->mode
						    = toi(data, 4);
					if (data_length >= 16)
						file->nlinks
						    = toi(data + 8, 4);
					if (data_length >= 24)
						file->uid
						    = toi(data + 16, 4);
					if (data_length >= 32)
						file->gid
						    = toi(data + 24, 4);
					if (data_length >= 40)
						file->inode
						    = toi(data + 32, 4);
				}
				break;
			}
			/* FALLTHROUGH */
		case 'R':
			if (p[0] == 'R' && p[1] == 'R' && version == 1) {
				iso9660->seenRockridge = 1;
				/*
				 * RR extension comprises:
				 *    one byte flag value
				 */
				/* TODO: Handle RR extension. */
				break;
			}
			/* FALLTHROUGH */
		case 'S':
			if (p[0] == 'S' && p[1] == 'L') {
				if (version == 1)
					parse_rockridge_SL1(file,
					    data, data_length);
				break;
			}
			if (p[0] == 'S' && p[1] == 'P'
			    && version == 1 && data_length == 3
			    && data[0] == (unsigned char)'\xbe'
			    && data[1] == (unsigned char)'\xef') {
				/*
				 * SP extension stores the suspOffset
				 * (Number of bytes to skip between
				 * filename and SUSP records.)
				 * It is mandatory by the SUSP standard
				 * (IEEE 1281).
				 *
				 * It allows SUSP to coexist with
				 * non-SUSP uses of the System
				 * Use Area by placing non-SUSP data
				 * before SUSP data.
				 *
				 * TODO: Add a check for 'SP' in
				 * first directory entry, disable all SUSP
				 * processing if not found.
				 */
				iso9660->suspOffset = data[2];
				break;
			}
			if (p[0] == 'S' && p[1] == 'T'
			    && data_length == 0 && version == 1) {
				/*
				 * ST extension marks end of this
				 * block of SUSP entries.
				 *
				 * It allows SUSP to coexist with
				 * non-SUSP uses of the System
				 * Use Area by placing non-SUSP data
				 * after SUSP data.
				 */
				return;
			}
		case 'T':
			if (p[0] == 'T' && p[1] == 'F') {
				if (version == 1)
					parse_rockridge_TF1(file,
					    data, data_length);
				break;
			}
			/* FALLTHROUGH */
		default:
			/* The FALLTHROUGHs above leave us here for
			 * any unsupported extension. */
#if DEBUG
			{
				const unsigned char *t;
				fprintf(stderr, "\nUnsupported RRIP extension for %s\n", file->name.s);
				fprintf(stderr, " %c%c(%d):", p[0], p[1], data_length);
				for (t = data; t < data + data_length && t < data + 16; t++)
					fprintf(stderr, " %02x", *t);
				fprintf(stderr, "\n");
			}
#endif
			break;
		}



		p += p[2];
	}
}

static void
parse_rockridge_NM1(struct file_info *file, const unsigned char *data,
    int data_length)
{
	if (!file->name_continues)
		archive_string_empty(&file->name);
	file->name_continues = 0;
	if (data_length < 1)
		return;
	/*
	 * NM version 1 extension comprises:
	 *   1 byte flag, value is one of:
	 *     = 0: remainder is name
	 *     = 1: remainder is name, next NM entry continues name
	 *     = 2: "."
	 *     = 4: ".."
	 *     = 32: Implementation specific
	 *     All other values are reserved.
	 */
	switch(data[0]) {
	case 0:
		if (data_length < 2)
			return;
		archive_strncat(&file->name, data + 1, data_length - 1);
		break;
	case 1:
		if (data_length < 2)
			return;
		archive_strncat(&file->name, data + 1, data_length - 1);
		file->name_continues = 1;
		break;
	case 2:
		archive_strcat(&file->name, ".");
		break;
	case 4:
		archive_strcat(&file->name, "..");
		break;
	default:
		return;
	}

}

static void
parse_rockridge_TF1(struct file_info *file, const unsigned char *data,
    int data_length)
{
	char flag;
	/*
	 * TF extension comprises:
	 *   one byte flag
	 *   create time (optional)
	 *   modify time (optional)
	 *   access time (optional)
	 *   attribute time (optional)
	 *  Time format and presence of fields
	 *  is controlled by flag bits.
	 */
	if (data_length < 1)
		return;
	flag = data[0];
	++data;
	--data_length;
	if (flag & 0x80) {
		/* Use 17-byte time format. */
		if ((flag & 1) && data_length >= 17) {
			/* Create time. */
			file->birthtime = isodate17(data);
			data += 17;
			data_length -= 17;
		}
		if ((flag & 2) && data_length >= 17) {
			/* Modify time. */
			file->mtime = isodate17(data);
			data += 17;
			data_length -= 17;
		}
		if ((flag & 4) && data_length >= 17) {
			/* Access time. */
			file->atime = isodate17(data);
			data += 17;
			data_length -= 17;
		}
		if ((flag & 8) && data_length >= 17) {
			/* Attribute change time. */
			file->ctime = isodate17(data);
			data += 17;
			data_length -= 17;
		}
	} else {
		/* Use 7-byte time format. */
		if ((flag & 1) && data_length >= 7) {
			/* Create time. */
			file->birthtime = isodate17(data);
			data += 7;
			data_length -= 7;
		}
		if ((flag & 2) && data_length >= 7) {
			/* Modify time. */
			file->mtime = isodate7(data);
			data += 7;
			data_length -= 7;
		}
		if ((flag & 4) && data_length >= 7) {
			/* Access time. */
			file->atime = isodate7(data);
			data += 7;
			data_length -= 7;
		}
		if ((flag & 8) && data_length >= 7) {
			/* Attribute change time. */
			file->ctime = isodate7(data);
			data += 7;
			data_length -= 7;
		}
	}
}

static void
parse_rockridge_SL1(struct file_info *file, const unsigned char *data,
    int data_length)
{
	int component_continues = 1;

	if (!file->symlink_continues)
		archive_string_empty(&file->symlink);
	else
		archive_strcat(&file->symlink, "/");
	file->symlink_continues = 0;

	/*
	 * Defined flag values:
	 *  0: This is the last SL record for this symbolic link
	 *  1: this symbolic link field continues in next SL entry
	 *  All other values are reserved.
	 */
	if (data_length < 1)
		return;
	switch(*data) {
	case 0:
		break;
	case 1:
		file->symlink_continues = 1;
		break;
	default:
		return;
	}
	++data;  /* Skip flag byte. */
	--data_length;

	/*
	 * SL extension body stores "components".
	 * Basically, this is a complicated way of storing
	 * a POSIX path.  It also interferes with using
	 * symlinks for storing non-path data. <sigh>
	 *
	 * Each component is 2 bytes (flag and length)
	 * possibly followed by name data.
	 */
	while (data_length >= 2) {
		unsigned char flag = *data++;
		unsigned char nlen = *data++;
		data_length -= 2;

		if (!component_continues)
			archive_strcat(&file->symlink, "/");
		component_continues = 0;

		switch(flag) {
		case 0: /* Usual case, this is text. */
			if (data_length < nlen)
				return;
			archive_strncat(&file->symlink,
			    (const char *)data, nlen);
			break;
		case 0x01: /* Text continues in next component. */
			if (data_length < nlen)
				return;
			archive_strncat(&file->symlink,
			    (const char *)data, nlen);
			component_continues = 1;
			break;
		case 0x02: /* Current dir. */
			archive_strcat(&file->symlink, ".");
			break;
		case 0x04: /* Parent dir. */
			archive_strcat(&file->symlink, "..");
			break;
		case 0x08: /* Root of filesystem. */
			archive_string_empty(&file->symlink);
			archive_strcat(&file->symlink, "/");
			break;
		case 0x10: /* Undefined (historically "volume root" */
			archive_string_empty(&file->symlink);
			archive_strcat(&file->symlink, "ROOT");
			break;
		case 0x20: /* Undefined (historically "hostname") */
			archive_strcat(&file->symlink, "hostname");
			break;
		default:
			/* TODO: issue a warning ? */
			return;
		}
		data += nlen;
		data_length -= nlen;
	}
}


static void
release_file(struct iso9660 *iso9660, struct file_info *file)
{
	struct file_info *parent;

	if (file->refcount == 0) {
		parent = file->parent;
		archive_string_free(&file->name);
		archive_string_free(&file->symlink);
		free(file);
		if (parent != NULL) {
			parent->refcount--;
			release_file(iso9660, parent);
		}
	}
}

static int
next_entry_seek(struct archive_read *a, struct iso9660 *iso9660,
    struct file_info **pfile)
{
	struct file_info *file;
	uint64_t offset;

	*pfile = NULL;
	for (;;) {
		*pfile = file = next_entry(iso9660);
		if (file == NULL)
			return (ARCHIVE_EOF);

		/* CE area precedes actual file data? Ignore it. */
		if (file->ce_offset > file->offset) {
			/* fprintf(stderr, " *** Discarding CE data.\n"); */
			file->ce_offset = 0;
			file->ce_size = 0;
		}

		/* Don't waste time seeking for zero-length bodies. */
		if (file->size == 0) {
			file->offset = iso9660->current_position;
		}

		/* If CE exists, find and read it now. */
		if (file->ce_offset > 0)
			offset = file->ce_offset;
		else
			offset = file->offset;

		/* Seek forward to the start of the entry. */
		if (iso9660->current_position < offset) {
			off_t step = offset - iso9660->current_position;
			off_t bytes_read;
			bytes_read = __archive_read_skip(a, step);
			if (bytes_read < 0)
				return (bytes_read);
			iso9660->current_position = offset;
		}

		/* We found body of file; handle it now. */
		if (offset == file->offset)
			return (ARCHIVE_OK);

		/* Found CE?  Process it and push the file back onto list. */
		if (offset == file->ce_offset) {
			const void *p;
			ssize_t size = file->ce_size;
			const unsigned char *rr_start;

			file->ce_offset = 0;
			file->ce_size = 0;
			p = __archive_read_ahead(a, size, NULL);
			if (p == NULL)
				return (ARCHIVE_FATAL);
			rr_start = (const unsigned char *)p;
			parse_rockridge(iso9660, file, rr_start,
			    rr_start + size);
			__archive_read_consume(a, size);
			iso9660->current_position += size;
			add_entry(iso9660, file);
		}
	}
}

static struct file_info *
next_entry(struct iso9660 *iso9660)
{
	int least_index;
	uint64_t least_end_offset;
	int i;
	struct file_info *r;

	if (iso9660->pending_files_used < 1)
		return (NULL);

	/* Assume the first file in the list is the earliest on disk. */
	least_index = 0;
	least_end_offset = iso9660->pending_files[0]->offset
	    + iso9660->pending_files[0]->size;

	/* Now, try to find an earlier one. */
	for (i = 0; i < iso9660->pending_files_used; i++) {
		/* Use the position of the file *end* as our comparison. */
		uint64_t end_offset = iso9660->pending_files[i]->offset
		    + iso9660->pending_files[i]->size;
		if (iso9660->pending_files[i]->ce_offset > 0
		    && iso9660->pending_files[i]->ce_offset < iso9660->pending_files[i]->offset)
			end_offset = iso9660->pending_files[i]->ce_offset
		    + iso9660->pending_files[i]->ce_size;
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

static unsigned int
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
isodate7(const unsigned char *v)
{
	struct tm tm;
	int offset;
	memset(&tm, 0, sizeof(tm));
	tm.tm_year = v[0];
	tm.tm_mon = v[1] - 1;
	tm.tm_mday = v[2];
	tm.tm_hour = v[3];
	tm.tm_min = v[4];
	tm.tm_sec = v[5];
	/* v[6] is the signed timezone offset, in 1/4-hour increments. */
	offset = ((const signed char *)v)[6];
	if (offset > -48 && offset < 52) {
		tm.tm_hour -= offset / 4;
		tm.tm_min -= (offset % 4) * 15;
	}
	return (time_from_tm(&tm));
}

static time_t
isodate17(const unsigned char *v)
{
	struct tm tm;
	int offset;
	memset(&tm, 0, sizeof(tm));
	tm.tm_year = (v[0] - '0') * 1000 + (v[1] - '0') * 100
	    + (v[2] - '0') * 10 + (v[3] - '0')
	    - 1900;
	tm.tm_mon = (v[4] - '0') * 10 + (v[5] - '0');
	tm.tm_mday = (v[6] - '0') * 10 + (v[7] - '0');
	tm.tm_hour = (v[8] - '0') * 10 + (v[9] - '0');
	tm.tm_min = (v[10] - '0') * 10 + (v[11] - '0');
	tm.tm_sec = (v[12] - '0') * 10 + (v[13] - '0');
	/* v[16] is the signed timezone offset, in 1/4-hour increments. */
	offset = ((const signed char *)v)[16];
	if (offset > -48 && offset < 52) {
		tm.tm_hour -= offset / 4;
		tm.tm_min -= (offset % 4) * 15;
	}
	return (time_from_tm(&tm));
}

static time_t
time_from_tm(struct tm *t)
{
#if HAVE_TIMEGM
	/* Use platform timegm() if available. */
	return (timegm(t));
#else
	/* Else use direct calculation using POSIX assumptions. */
	/* First, fix up tm_yday based on the year/month/day. */
	mktime(t);
	/* Then we can compute timegm() from first principles. */
	return (t->tm_sec + t->tm_min * 60 + t->tm_hour * 3600
	    + t->tm_yday * 86400 + (t->tm_year - 70) * 31536000
	    + ((t->tm_year - 69) / 4) * 86400 -
	    ((t->tm_year - 1) / 100) * 86400
	    + ((t->tm_year + 299) / 400) * 86400);
#endif
}

static const char *
build_pathname(struct archive_string *as, struct file_info *file)
{
	if (file->parent != NULL && archive_strlen(&file->parent->name) > 0) {
		build_pathname(as, file->parent);
		archive_strcat(as, "/");
	}
	if (archive_strlen(&file->name) == 0)
		archive_strcat(as, ".");
	else
		archive_string_concat(as, &file->name);
	return (as->s);
}

#if DEBUG
static void
dump_isodirrec(FILE *out, const unsigned char *isodirrec)
{
	fprintf(out, " l %d,",
	    toi(isodirrec + DR_length_offset, DR_length_size));
	fprintf(out, " a %d,",
	    toi(isodirrec + DR_ext_attr_length_offset, DR_ext_attr_length_size));
	fprintf(out, " ext 0x%x,",
	    toi(isodirrec + DR_extent_offset, DR_extent_size));
	fprintf(out, " s %d,",
	    toi(isodirrec + DR_size_offset, DR_extent_size));
	fprintf(out, " f 0x%02x,",
	    toi(isodirrec + DR_flags_offset, DR_flags_size));
	fprintf(out, " u %d,",
	    toi(isodirrec + DR_file_unit_size_offset, DR_file_unit_size_size));
	fprintf(out, " ilv %d,",
	    toi(isodirrec + DR_interleave_offset, DR_interleave_size));
	fprintf(out, " seq %d,",
	    toi(isodirrec + DR_volume_sequence_number_offset, DR_volume_sequence_number_size));
	fprintf(out, " nl %d:",
	    toi(isodirrec + DR_name_len_offset, DR_name_len_size));
	fprintf(out, " `%.*s'",
	    toi(isodirrec + DR_name_len_offset, DR_name_len_size), isodirrec + DR_name_offset);
}
#endif
