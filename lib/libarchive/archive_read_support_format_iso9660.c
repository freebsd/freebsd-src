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
 * building a list of directory entries, sorted by starting
 * sector.  At each step, I look for the earliest dir entry that
 * hasn't yet been read, seek forward to that location and read
 * that entry.  If it's a dir, I slurp in the new dir entries and
 * add them to the heap; if it's a regular file, I return the
 * corresponding archive_entry and wait for the client to request
 * the file body.  This strategy allows us to read most compliant
 * CDs with a single pass through the data, as required by libarchive.
 */

/* Structure of on-disk PVD. */
struct iso9660_primary_volume_descriptor {
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
struct iso9660_directory_record {
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
struct file_info {
	struct file_info	*parent;
	int		 refcount;
	uint64_t	 offset;  /* Offset on disk. */
	uint64_t	 size;	/* File size in bytes. */
	uint64_t	 ce_offset; /* Offset of CE */
	uint64_t	 ce_size; /* Size of CE */
	time_t		 mtime;	/* File last modified time. */
	time_t		 atime;	/* File last accessed time. */
	time_t		 ctime;	/* File creation time. */
	mode_t		 mode;
	uid_t		 uid;
	gid_t		 gid;
	int		 nlinks;
	char		*name; /* Null-terminated filename. */
	struct archive_string symlink;
};


struct iso9660 {
	int	magic;
#define ISO9660_MAGIC   0x96609660
	int	bid; /* If non-zero, return this as our bid. */
	struct archive_string pathname;
	char	seenRockridge; /* Set true if RR extensions are used. */

	uint64_t	previous_offset;
	uint64_t	previous_size;
	struct archive_string previous_pathname;

	/* TODO: Make this a heap for fast inserts and deletions. */
	struct file_info **pending_files;
	int	pending_files_allocated;
	int	pending_files_used;

	uint64_t current_position;
	ssize_t	logical_block_size;

	off_t	entry_sparse_offset;
	ssize_t	entry_bytes_remaining;
};

static void	add_entry(struct iso9660 *iso9660, struct file_info *file);
static int	archive_read_format_iso9660_bid(struct archive *);
static int	archive_read_format_iso9660_cleanup(struct archive *);
static int	archive_read_format_iso9660_read_data(struct archive *,
		    const void **, size_t *, off_t *);
static int	archive_read_format_iso9660_read_header(struct archive *,
		    struct archive_entry *);
static const char *build_pathname(struct archive_string *, struct file_info *);
static void	dump_isodirrec(FILE *, const struct iso9660_directory_record *);
static time_t	isodate17(const void *);
static time_t	isodate7(const void *);
static int	isPVD(struct iso9660 *, const char *);
static struct file_info *next_entry(struct iso9660 *);
static int	next_entry_seek(struct archive *a, struct iso9660 *iso9660,
		    struct file_info **pfile);
static struct file_info *
		parse_file_info(struct iso9660 *iso9660,
		    struct file_info *parent,
		    const struct iso9660_directory_record *isodirrec);
static void	parse_rockridge(struct iso9660 *iso9660,
		    struct file_info *file, const unsigned char *start,
		    const unsigned char *end);
static void	release_file(struct iso9660 *, struct file_info *);
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
	const struct iso9660_primary_volume_descriptor *voldesc;
	struct file_info *file;

	if (h[0] != 1)
		return (0);
	if (memcmp(h+1, "CD001", 5) != 0)
		return (0);


	voldesc = (const struct iso9660_primary_volume_descriptor *)h;
	iso9660->logical_block_size = toi(&voldesc->logical_block_size, 2);

	/* Store the root directory in the pending list. */
	file = parse_file_info(iso9660, NULL,
	    (struct iso9660_directory_record *)&voldesc->root_directory_record);
	add_entry(iso9660, file);
	return (48);
}

static int
archive_read_format_iso9660_read_header(struct archive *a,
    struct archive_entry *entry)
{
	struct stat st;
	struct iso9660 *iso9660;
	struct file_info *file;
	ssize_t bytes_read;
	int r;

	iso9660 = *(a->pformat_data);

	if (iso9660->seenRockridge) {
		a->archive_format = ARCHIVE_FORMAT_ISO9660_ROCKRIDGE;
		a->archive_format_name = "ISO9660 with Rockridge extensions";
	} else {
		a->archive_format = ARCHIVE_FORMAT_ISO9660;
		a->archive_format_name = "ISO9660";
	}

	/* Get the next entry that appears after the current offset. */
	r = next_entry_seek(a, iso9660, &file);
	if (r != ARCHIVE_OK)
		return (r);

	iso9660->entry_bytes_remaining = file->size;
	iso9660->entry_sparse_offset = 0; /* Offset for sparse-file-aware clients. */

	/* Set up the entry structure with information about this entry. */
	memset(&st, 0, sizeof(st));
	st.st_mode = file->mode;
	st.st_uid = file->uid;
	st.st_gid = file->gid;
	st.st_nlink = file->nlinks;
	st.st_mtime = file->mtime;
	st.st_ctime = file->ctime;
	st.st_atime = file->atime;
	st.st_size = iso9660->entry_bytes_remaining;
	archive_entry_copy_stat(entry, &st);
	archive_string_empty(&iso9660->pathname);
	archive_entry_set_pathname(entry,
	    build_pathname(&iso9660->pathname, file));
	if (file->symlink.s != NULL)
		archive_entry_set_symlink(entry, file->symlink.s);

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
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
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
				release_file(iso9660, file);
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
				const struct iso9660_directory_record *dr
				    = (const struct iso9660_directory_record *)p;
				struct file_info *child;

				/* Skip '.' entry. */
				if (dr->name_len[0] == 1
				    && dr->name[0] == '\0')
					continue;
				/* Skip '..' entry. */
				if (dr->name_len[0] == 1
				    && dr->name[0] == '\001')
					continue;
				child = parse_file_info(iso9660, file, dr);
				add_entry(iso9660, child);
			}
		}
	}

	release_file(iso9660, file);
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
	struct file_info *file;

	iso9660 = *(a->pformat_data);
	while ((file = next_entry(iso9660)) != NULL)
		release_file(iso9660, file);
	archive_string_free(&iso9660->pathname);
	archive_string_free(&iso9660->previous_pathname);
	free(iso9660);
	*(a->pformat_data) = NULL;
	return (ARCHIVE_OK);
}

/*
 * This routine parses a single ISO directory record, makes sense
 * of any extensions, and stores the result in memory.
 */
static struct file_info *
parse_file_info(struct iso9660 *iso9660, struct file_info *parent,
    const struct iso9660_directory_record *isodirrec)
{
	struct file_info *file;

	/* TODO: Sanity check that name_len doesn't exceed length, etc. */

	/* Create a new file entry and copy data from the ISO dir record. */
	file = malloc(sizeof(*file));
	memset(file, 0, sizeof(*file));
	file->parent = parent;
	if (parent != NULL)
		parent->refcount++;
	file->offset = toi(isodirrec->extent, 4)
	    * iso9660->logical_block_size;
	file->size = toi(isodirrec->size, 4);
	file->mtime = isodate7(isodirrec->date);
	file->ctime = file->atime = file->mtime;
	file->name = malloc(isodirrec->name_len[0] + 1);
	memcpy(file->name, isodirrec->name, isodirrec->name_len[0]);
	file->name[(int)isodirrec->name_len[0]] = '\0';
	if (isodirrec->flags[0] & 0x02)
		file->mode = S_IFDIR | 0700;
	else
		file->mode = S_IFREG | 0400;

	/* Rockridge extensions overwrite information from above. */
	{
		const unsigned char *rr_start, *rr_end;
		rr_end = (const unsigned char *)isodirrec
		    + isodirrec->length[0];
		rr_start = isodirrec->name + isodirrec->name_len[0];
		if ((isodirrec->name_len[0] & 1) == 0)
			rr_start++;
		parse_rockridge(iso9660, file, rr_start, rr_end);
	}

	/* DEBUGGING: Warn about attributes I don't yet fully support. */
	if ((isodirrec->flags[0] & ~0x02) != 0) {
		fprintf(stderr, "\n ** Unrecognized flag: ");
		dump_isodirrec(stderr, isodirrec);
		fprintf(stderr, "\n");
	} else if (toi(isodirrec->volume_sequence_number, 2) != 1) {
		fprintf(stderr, "\n ** Unrecognized sequence number: ");
		dump_isodirrec(stderr, isodirrec);
		fprintf(stderr, "\n");
	} else if (isodirrec->file_unit_size[0] != 0) {
		fprintf(stderr, "\n ** Unexpected file unit size: ");
		dump_isodirrec(stderr, isodirrec);
		fprintf(stderr, "\n");
	} else if (isodirrec->interleave[0] != 0) {
		fprintf(stderr, "\n ** Unexpected interleave: ");
		dump_isodirrec(stderr, isodirrec);
		fprintf(stderr, "\n");
	} else if (isodirrec->ext_attr_length[0] != 0) {
		fprintf(stderr, "\n ** Unexpected extended attribute length: ");
		dump_isodirrec(stderr, isodirrec);
		fprintf(stderr, "\n");
	}

	return (file);
}

static void
add_entry(struct iso9660 *iso9660, struct file_info *file)
{
	/* Expand our pending files list as necessary. */
	if (iso9660->pending_files_used >= iso9660->pending_files_allocated) {
		struct file_info **new_pending_files;
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

	iso9660->pending_files[iso9660->pending_files_used++] = file;
}

static void
parse_rockridge(struct iso9660 *iso9660, struct file_info *file,
    const unsigned char *p, const unsigned char *end)
{
	(void)iso9660; /* UNUSED */

	while (p + 4 < end  /* Enough space for another entry. */
	    && p[0] >= 'A' && p[0] <= 'Z' /* Sanity-check 1st char of name. */
	    && p[1] >= 'A' && p[1] <= 'Z' /* Sanity-check 2nd char of name. */
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
			if (p[0] == 'C' && p[1] == 'E' && version == 1) {
				/*
				 * CE extension comprises:
				 *   8 byte sector containing extension
				 *   8 byte offset w/in above sector
				 *   8 byte length of continuation
				 */
				file->ce_offset = toi(data, 4)
				    * iso9660->logical_block_size
				    + toi(data + 8, 4);
				file->ce_size = toi(data + 16, 4);
				break;
			}
			/* FALLTHROUGH */
		case 'N':
			if (p[0] == 'N' && p[1] == 'M' && version == 1
				&& *data == 0) {
				/* NM extension with flag byte == 0 */
				/*
				 * NM extension comprises:
				 *   one byte flag
				 *   rest is long name
				 */
				/* TODO: Obey flags. */
				char *old_name = file->name;

				data++;  /* Skip flag byte. */
				data_length--;
				file->name = malloc(data_length + 1);
				if (file->name != NULL) {
					free(old_name);
					memcpy(file->name, data, data_length);
					file->name[data_length] = '\0';
				} else
					file->name = old_name;
				break;
			}
			/* FALLTHROUGH */
		case 'P':
			if (p[0] == 'P' && p[1] == 'X' && version == 1) {
				/*
				 * PX extension comprises:
				 *   8 bytes for mode,
				 *   8 bytes for nlinks,
				 *   8 bytes for uid,
				 *   8 bytes for gid.
				 */
				if (data_length == 32) {
					file->mode = toi(data, 4);
					file->nlinks = toi(data + 8, 4);
					file->uid = toi(data + 16, 4);
					file->gid = toi(data + 24, 4);
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
			if (p[0] == 'S' && p[1] == 'L' && version == 1
			    && *data == 0) {
				int cont = 1;
				/* SL extension with flags == 0 */
				/* TODO: handle non-zero flag values. */
				data++;  /* Skip flag byte. */
				data_length--;
				while (data_length > 0) {
					unsigned char flag = *data++;
					unsigned char nlen = *data++;
					data_length -= 2;

					if (cont == 0)
						archive_strcat(&file->symlink, "/");
					cont = 0;

					switch(flag) {
					case 0x01: /* Continue */
						archive_strncat(&file->symlink, data, nlen);
						cont = 1;
						break;
					case 0x02: /* Current */
						archive_strcat(&file->symlink, ".");
						break;
					case 0x04: /* Parent */
						archive_strcat(&file->symlink, "..");
						break;
					case 0x08: /* Root */
					case 0x10: /* Volume root */
						archive_string_empty(&file->symlink);
						break;
					case 0x20: /* Hostname */
						archive_strcat(&file->symlink, "hostname");
						break;
					case 0:
						archive_strncat(&file->symlink, data, nlen);
						break;
					default:
						/* TODO: issue a warning ? */
						break;
					}
					data += nlen;
					data_length -= nlen;
				}
				break;
			}
		case 'T':
			if (p[0] == 'T' && p[1] == 'F' && version == 1) {
				char flag = data[0];
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
				data++;
				if (flag & 0x80) {
					/* Use 17-byte time format. */
					if (flag & 1) /* Create time. */
						data += 17;
					if (flag & 2) { /* Modify time. */
						file->mtime = isodate17(data);
						data += 17;
					}
					if (flag & 4) { /* Access time. */
						file->atime = isodate17(data);
						data += 17;
					}
					if (flag & 8) { /* Attribute time. */
						file->ctime = isodate17(data);
						data += 17;
					}
				} else {
					/* Use 7-byte time format. */
					if (flag & 1) /* Create time. */
						data += 7;
					if (flag & 2) { /* Modify time. */
						file->mtime = isodate7(data);
						data += 7;
					}
					if (flag & 4) { /* Access time. */
						file->atime = isodate7(data);
						data += 7;
					}
					if (flag & 8) { /* Attribute time. */
						file->ctime = isodate7(data);
						data += 7;
					}
				}
				break;
			}
			/* FALLTHROUGH */
		default:
			/* The FALLTHROUGHs above leave us here for
			 * any unsupported extension. */
			{
				const unsigned char *t;
				fprintf(stderr, "\nUnsupported RRIP extension for %s\n", file->name);
				fprintf(stderr, " %c%c(%d):", p[0], p[1], data_length);
				for (t = data; t < data + data_length && t < data + 16; t++)
					fprintf(stderr, " %02x", *t);
				fprintf(stderr, "\n");
			}
		}



		p += p[2];
	}
}

static void
release_file(struct iso9660 *iso9660, struct file_info *file)
{
	struct file_info *parent;

	if (file->refcount == 0) {
		parent = file->parent;
		if (file->name)
			free(file->name);
		archive_string_free(&file->symlink);
		free(file);
		if (parent != NULL) {
			parent->refcount--;
			release_file(iso9660, parent);
		}
	}
}

static int
next_entry_seek(struct archive *a, struct iso9660 *iso9660,
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
fprintf(stderr, " *** Discarding CE data.\n");
			file->ce_offset = 0;
			file->ce_size = 0;
		}

		/* If CE exists, find and read it now. */
		if (file->ce_offset > 0)
			offset = file->ce_offset;
		else
			offset = file->offset;

		/* Seek forward to the start of the entry. */
		while (iso9660->current_position < offset) {
			ssize_t step = offset - iso9660->current_position;
			ssize_t bytes_read;
			const void *buff;

			if (step > iso9660->logical_block_size)
				step = iso9660->logical_block_size;
			bytes_read = (a->compression_read_ahead)(a, &buff, step);
			if (bytes_read <= 0) {
				release_file(iso9660, file);
				return (ARCHIVE_FATAL);
			}
			if (bytes_read > step)
				bytes_read = step;
			iso9660->current_position += bytes_read;
			(a->compression_read_consume)(a, bytes_read);
		}

		/* We found body of file; handle it now. */
		if (offset == file->offset)
			return (ARCHIVE_OK);

		/* Found CE?  Process it and push the file back onto list. */
		if (offset == file->ce_offset) {
			const void *p;
			ssize_t size = file->ce_size;
			ssize_t bytes_read;
			const unsigned char *rr_start;

			file->ce_offset = 0;
			file->ce_size = 0;
			bytes_read = (a->compression_read_ahead)(a, &p, size);
			if (bytes_read > size)
				bytes_read = size;
			rr_start = (const unsigned char *)p;
			parse_rockridge(iso9660, file, rr_start,
			    rr_start + bytes_read);
			(a->compression_read_consume)(a, bytes_read);
			iso9660->current_position += bytes_read;
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
	for(i = 0; i < iso9660->pending_files_used; i++) {
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
isodate7(const void *p)
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
	if (offset > -48 && offset < 52) {
		tm.tm_hour -= offset / 4;
		tm.tm_min -= (offset % 4) * 15;
	}
	return (timegm(&tm));
}

static time_t
isodate17(const void *p)
{
	struct tm tm;
	const unsigned char *v = (const unsigned char *)p;
	int offset;
	tm.tm_year = (v[0] - '0') * 1000 + (v[1] - '0') * 100
	    + (v[2] - '0') * 10 + (v[3] - '0')
	    - 1900;
	tm.tm_mon = (v[4] - '0') * 10 + (v[5] - '0');
	tm.tm_mday = (v[6] - '0') * 10 + (v[7] - '0');
	tm.tm_hour = (v[8] - '0') * 10 + (v[9] - '0');
	tm.tm_min = (v[10] - '0') * 10 + (v[11] - '0');
	tm.tm_sec = (v[12] - '0') * 10 + (v[13] - '0');
	/* v[16] is the timezone offset, in 1/4-hour increments. */
	offset = ((const signed char *)p)[16];
	if (offset > -48 && offset < 52) {
		tm.tm_hour -= offset / 4;
		tm.tm_min -= (offset % 4) * 15;
	}
	return (timegm(&tm));
}

static const char *
build_pathname(struct archive_string *as, struct file_info *file)
{
	if (file->parent != NULL && file->parent->name[0] != '\0') {
		build_pathname(as, file->parent);
		archive_strcat(as, "/");
	}
	if (file->name[0] == '\0')
		archive_strcat(as, ".");
	else
		archive_strcat(as, file->name);
	return (as->s);
}

static void
dump_isodirrec(FILE *out, const struct iso9660_directory_record *isodirrec)
{
	fprintf(out, " l %d,", isodirrec->length[0]);
	fprintf(out, " a %d,", isodirrec->ext_attr_length[0]);
	fprintf(out, " ext 0x%x,", toi(isodirrec->extent, 4));
	fprintf(out, " s %d,", toi(isodirrec->size, 4));
	fprintf(out, " f 0x%02x,", isodirrec->flags[0]);
	fprintf(out, " u %d,", isodirrec->file_unit_size[0]);
	fprintf(out, " ilv %d,", isodirrec->interleave[0]);
	fprintf(out, " seq %d,", toi(isodirrec->volume_sequence_number,2));
	fprintf(out, " nl %d:", isodirrec->name_len[0]);
	fprintf(out, " `%.*s'", isodirrec->name_len[0], isodirrec->name);
}
