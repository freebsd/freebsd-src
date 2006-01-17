/*-
 * Copyright (c) 2003-2005 Tim Kientzle
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
#include <time.h>
#include <unistd.h>

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_string.h"

/*
 * 'tp' was the common archiving format for Fourth Edition through
 * Sixth Edition Unix.  It was replaced by 'tar' in Seventh Edition.
 * (First through Third Edition used the 'tap' archiver.)
 *
 * The format has a 512-byte boot block, followed by a table of
 * contents listing all of the files in the archive, followed by
 * the file data.  Like 'tar', it is block-oriented; file data is
 * padded to a whole number of blocks.
 *
 * There are three different variants with slightly different TOC
 * formats:
 *    Original tp:  64-byte TOC entries with 32-byte pathnames.
 *    Ian Johnson's AGSM 'itp':  64-byte TOC entries with 48-byte pathnames
 *    'dtp' ???: 128-byte TOC entries with 114-byte pathnames.
 *
 * All variants store similar metadata: 16-bit mode, 8-bit uid/gid,
 * 24-bit size, 32-bit timestamp.  (The later 'tar' format extended
 * these fields and added link support.  The earlier 'tap' format used
 * narrower 8-bit mode and 16-bit size.)
 */

/*
 * The support code here reads the entire TOC into memory
 * up front.  The following structure is used to store
 * a single TOC record in memory.
 */
struct file_info {
	unsigned int	 offset;  /* Offset in archive. */
	unsigned int 	 size;	/* File size in bytes. */
	time_t		 mtime;	/* File last modified time. */
	mode_t		 mode;
	uid_t		 uid;
	gid_t		 gid;
	char		*name; /* Null-terminated filename. */
};

/*
 * Format-specific data.
 */
struct tp {
	int	bid; /* If non-zero, return this as our bid. */

	struct file_info **pending_files;
	int	pending_files_allocated;
	int	pending_files_used;

	uint64_t current_position;
	int64_t entry_bytes_remaining;
	int64_t entry_sparse_offset;
	int	fake_inode;
	int	fake_dev;

	/*
	 * Pointer to a function to parse the dir entry for
	 * the selected format.
	 */
	struct file_info *(*parse_file_info)(struct archive *, const void *);
	ssize_t  toc_size;
	int toc_read; /* True if we've already read the TOC. */
};

static void	add_entry(struct tp *tp, struct file_info *file);
static int	archive_read_format_tp_bid(struct archive *);
static int	archive_read_format_tp_cleanup(struct archive *);
static int	archive_read_format_tp_read_data(struct archive *,
		    const void **, size_t *, off_t *);
static int	archive_read_format_tp_read_header(struct archive *,
		    struct archive_entry *);
static struct file_info *next_entry(struct tp *);
static int	next_entry_seek(struct archive *a, struct tp *tp,
		    struct file_info **pfile);
static struct file_info *parse_file_info_tp(struct archive *, const void *);
static struct file_info *parse_file_info_itp(struct archive *, const void *);
static void	release_file(struct tp *, struct file_info *);
static int	toi(const void *p, int n);

int
archive_read_support_format_tp(struct archive *a)
{
	struct tp *tp;
	int r;

	tp = malloc(sizeof(*tp));
	if (tp == NULL) {
		archive_set_error(a, ENOMEM, "Can't allocate tp data");
		return (ARCHIVE_FATAL);
	}
	memset(tp, 0, sizeof(*tp));
	tp->bid = -1; /* We haven't yet bid. */

	r = __archive_read_register_format(a,
	    tp,
	    archive_read_format_tp_bid,
	    archive_read_format_tp_read_header,
	    archive_read_format_tp_read_data,
	    NULL,
	    archive_read_format_tp_cleanup);

	if (r != ARCHIVE_OK) {
		free(tp);
		return (r);
	}
	return (ARCHIVE_OK);
}

static int
archive_read_format_tp_bid(struct archive *a)
{
	struct tp *tp;
	ssize_t bytes_read;
	const void *h;
	const char *p;
	int toc_count;

	tp = *(a->pformat_data);

	if (tp->bid >= 0)
		return (tp->bid);

	/* Read a large initial block and inspect it to see
	 * if it looks like a tp TOC. */
	bytes_read = (a->compression_read_ahead)(a, &h, 8192);
	if (bytes_read < 1024)
		return (tp->bid = 0);

	p = (const char *)h;

	/* Skip the 512-byte boot block. */
	bytes_read -= 512;
	p += 512;

	/*
	 * Check that there is something that looks like a tp TOC
	 * entry located every 64 bytes.
	 */
	tp->parse_file_info = parse_file_info_tp;
	tp->toc_size = 64;
	toc_count = 0;
	while (bytes_read > 64 && p[0] != '\0') {
		/* Null-terminated ASCII pathname starts at beginning
		 * of block and is no more than 32 characters long for
		 * tp format, 48 for 'itp' format. */
		const char *pn = p;
		while (*pn >= 0x20 && *pn <= 0x7e && pn < p + 64) {
			/* backslash is illegal in filenames */
			if (*pn == '\\')
				return (tp->bid = 0);
			pn++;
		}
		if (pn > p + 48) /* String longer than 48 chars? */
			return (tp->bid = 0);
		/* Must be Ian Johnson's AGSM extended version. */
		if (pn > p + 32)
			tp->parse_file_info = parse_file_info_itp;
		if (*pn != '\0') /* Has non-ASCII character. */
			return (tp->bid = 0);
		/* We've checked ~1 bit for each character. */
		tp->bid += pn - p;

		/*
		 * TODO: sanity-test the mode field; the upper bits
		 * of the mode should have only one of a small number
		 * of valid file types.
		 */
		toc_count++;
		p += tp->toc_size;
	}

	/*
	 * We now know how many TOC entries we have in memory.
	 * Read the offset/size values into memory, sort, and verify
	 * that they define non-overlapping blocks in the archive.
	 */
	{
		struct block_info { uint64_t offset; uint64_t size; } *blocks;
		struct block_info t;
		int i, not_sorted;

		blocks = malloc(sizeof(*blocks) * toc_count);
		memset(blocks, 0, sizeof(*blocks) * toc_count);
		p = (const char *)h;
		p += 512;
		for (i = 0; i < toc_count; i++) {
			/* TODO: If this is itp, use different offsets. */
			blocks[i].size = toi(p + 37, 3);
			blocks[i].offset = toi(p + 44, 2) * 512;
			p += 64;
			/* TODO: If this is dtp, use different offsets and stride. */
		}

		/*
		 * Sort blocks by offset, just in case the entries
		 * aren't already in sorted order.  Because we expect
		 * the entries to already be sorted, a bubble sort is
		 * actually appropriate: it's O(n) on already-sorted
		 * data, compared to O(n log n) for quicksort or merge
		 * sort and O(n^2) for insertion sort.
		 */
		do {
			not_sorted = 0;
			for (i = 0; i < toc_count - 1; i++) {
				if (blocks[i].offset > blocks[i + 1].offset) {
					t = blocks[i];
					blocks[i] = blocks[i + 1];
					blocks[i + 1] = t;
					not_sorted = 1;
				}
			}
		} while (not_sorted);

		/* Check that blocks don't overlap. */
		for (i = 0; i < toc_count - 1; i++) {
			if (blocks[i].offset + blocks[i].size
			    > blocks[i + 1].offset)
			{
				free(blocks);
				return (tp->bid = 0);
			}
		}
	}

	return (tp->bid);
}

static int
archive_read_format_tp_read_header(struct archive *a,
    struct archive_entry *entry)
{
	struct stat st;
	struct tp *tp;
	struct file_info *file;
	const char *p;
	ssize_t bytes_read;
	int r;

	tp = *(a->pformat_data);

	/* Read the entire TOC first. */
	if (!tp->toc_read) {
		/* Skip the initial block. */
		bytes_read = (a->compression_read_ahead)(a,
		    (const void **)&p, 512);
		if (bytes_read < 512)
			return (ARCHIVE_FATAL);
		bytes_read = 512;
		tp->current_position += bytes_read;
		(a->compression_read_consume)(a, bytes_read);

		/* Consume TOC entries. */
		do {
			bytes_read = (a->compression_read_ahead)(a,
			    (const void **)&p, tp->toc_size);
			if (bytes_read < tp->toc_size)
				return (ARCHIVE_FATAL);
			bytes_read = tp->toc_size;
			tp->current_position += bytes_read;
			(a->compression_read_consume)(a, bytes_read);
			file = (*tp->parse_file_info)(a, p);
			if (file != NULL)
				add_entry(tp, file);
			else if (p[0] != '\0')
				/* NULL is okay if this is the sentinel. */
				return (ARCHIVE_FATAL);
		} while (p[0] != '\0');

		tp->toc_read = 1;
	}

	/* Get the next entry that appears after the current offset. */
	r = next_entry_seek(a, tp, &file);
	if (r != ARCHIVE_OK)
		return (r);

	tp->entry_bytes_remaining = file->size;
	tp->entry_sparse_offset = 0; /* Offset for sparse-file-aware clients */

	/* Set up the entry structure with information about this entry. */
	memset(&st, 0, sizeof(st));
	st.st_mode = file->mode;
	st.st_uid = file->uid;
	st.st_gid = file->gid;
	st.st_nlink = 1;
	if (++tp->fake_inode > 0xfff0) {
		tp->fake_inode = 1;
		tp->fake_dev++;
	}
	st.st_ino = tp->fake_inode;
	st.st_dev = tp->fake_dev;
	st.st_mtime = file->mtime;
	st.st_ctime = file->mtime;
	st.st_atime = file->mtime;
	st.st_size = tp->entry_bytes_remaining;
	archive_entry_copy_stat(entry, &st);
	archive_entry_set_pathname(entry, file->name);

	release_file(tp, file);
	return (ARCHIVE_OK);
}

static int
archive_read_format_tp_read_data(struct archive *a,
    const void **buff, size_t *size, off_t *offset)
{
	ssize_t bytes_read;
	struct tp *tp;

	tp = *(a->pformat_data);
	if (tp->entry_bytes_remaining <= 0) {
		*buff = NULL;
		*size = 0;
		*offset = tp->entry_sparse_offset;
		return (ARCHIVE_EOF);
	}

	bytes_read = (a->compression_read_ahead)(a, buff, 1);
	if (bytes_read == 0)
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "Truncated input file");
	if (bytes_read <= 0)
		return (ARCHIVE_FATAL);
	if (bytes_read > tp->entry_bytes_remaining)
		bytes_read = tp->entry_bytes_remaining;
	*size = bytes_read;
	*offset = tp->entry_sparse_offset;
	tp->entry_sparse_offset += bytes_read;
	tp->entry_bytes_remaining -= bytes_read;
	tp->current_position += bytes_read;
	(a->compression_read_consume)(a, bytes_read);
	return (ARCHIVE_OK);
}

static int
archive_read_format_tp_cleanup(struct archive *a)
{
	struct tp *tp;
	struct file_info *file;

	tp = *(a->pformat_data);
	while ((file = next_entry(tp)) != NULL)
		release_file(tp, file);
	free(tp);
	*(a->pformat_data) = NULL;
	return (ARCHIVE_OK);
}

/*
 * This routine parses a single directory record.
 */
static struct file_info *
parse_file_info_tp(struct archive *a, const void *dir_p)
{
	struct file_info *file;
	const struct tpdir {
		char name[32];
		char mode[2];
		char uid[1];
		char gid[1];
		char unused[1];
		char size[3];
		char modtime[4];
		char tapeaddr[2];
		char unused2[16];
		char checksum[2];
	} *p = dir_p;

	(void)a; /* UNUSED */

	/* Create a new file entry and copy data from the dir record. */
	file = malloc(sizeof(*file));
	if (file == NULL) {
		archive_set_error(a, ENOMEM, "Can't allocate TOC record");
		return (NULL);
	}
	memset(file, 0, sizeof(*file));

	file->name = malloc(sizeof(p->name) + 1);
	if (file->name == NULL) {
		archive_set_error(a, ENOMEM, "Can't allocate TOC name");
		free(file);
		return (NULL);
	}
	memcpy(file->name, p->name, sizeof(p->name));
	file->name[sizeof(p->name)] = '\0';
	/* If name wasn't null-terminated, then it's not valid. */
	if (strlen(file->name) == sizeof(p->name) || strlen(file->name) == 0) {
		archive_set_error(a, ENOMEM, "Damaged tp archive; invalid TOC");
		free(file->name);
		free(file);
		return (NULL);
	}
	file->offset = toi(p->tapeaddr, sizeof(p->tapeaddr)) * 512;
	file->size = toi(p->size, sizeof(p->size));
	file->mtime = toi(p->modtime, sizeof(p->modtime));
	file->mode = toi(p->mode, sizeof(p->mode));
	file->uid = toi(p->uid, sizeof(p->uid));
	file->gid = toi(p->gid, sizeof(p->gid));
	return (file);
}


/*
 * Ian Johnson's extended tp for AGSM eliminated the 16 pad bytes and
 * extnded the name field, allowing for 48 byte names.
 */
static struct file_info *
parse_file_info_itp(struct archive *a, const void *dir_p)
{
	struct file_info *file;
	const struct itpdir {
		char name[48];
		char mode[2];
		char uid[1];
		char gid[1];
		char unused[1];
		char size[3];
		char modtime[4];
		char tapeaddr[2];
		char checksum[2];
	} *p = dir_p;

	(void)a; /* UNUSED */

	/* Create a new file entry and copy data from the dir record. */
	file = malloc(sizeof(*file));
	if (file == NULL) {
		archive_set_error(a, ENOMEM, "Can't allocate TOC record");
		return (NULL);
	}
	memset(file, 0, sizeof(*file));

	file->name = malloc(sizeof(p->name) + 1);
	if (file->name == NULL) {
		archive_set_error(a, ENOMEM, "Can't allocate TOC name");
		free(file);
		return (NULL);
	}
	memcpy(file->name, p->name, sizeof(p->name));
	file->name[sizeof(p->name)] = '\0';
	/* If name wasn't null-terminated, then it's not valid. */
	if (strlen(file->name) == sizeof(p->name) || strlen(file->name) == 0) {
		archive_set_error(a, ENOMEM, "Damaged tp archive; invalid TOC");
		free(file->name);
		free(file);
		return (NULL);
	}
	file->offset = toi(p->tapeaddr, sizeof(p->tapeaddr)) * 512;
	file->size = toi(p->size, sizeof(p->size));
	file->mtime = toi(p->modtime, sizeof(p->modtime));
	file->mode = toi(p->mode, sizeof(p->mode));
	file->uid = toi(p->uid, sizeof(p->uid));
	file->gid = toi(p->gid, sizeof(p->gid));
	return (file);
}

static void
add_entry(struct tp *tp, struct file_info *file)
{
	/* Expand our pending files list as necessary. */
	if (tp->pending_files_used >= tp->pending_files_allocated) {
		struct file_info **new_pending_files;
		int new_size = tp->pending_files_allocated * 2;

		if (new_size < 1024)
			new_size = 1024;
		new_pending_files = malloc(new_size * sizeof(new_pending_files[0]));
		if (new_pending_files == NULL)
			__archive_errx(1, "Out of memory");
		memcpy(new_pending_files, tp->pending_files,
		    tp->pending_files_allocated * sizeof(new_pending_files[0]));
		if (tp->pending_files != NULL)
			free(tp->pending_files);
		tp->pending_files = new_pending_files;
		tp->pending_files_allocated = new_size;
	}

	tp->pending_files[tp->pending_files_used++] = file;
}

static void
release_file(struct tp *tp, struct file_info *file)
{
	(void)tp; /* UNUSED */
	if (file->name)
		free(file->name);
	free(file);
}

static int
next_entry_seek(struct archive *a, struct tp *tp,
    struct file_info **pfile)
{
	struct file_info *file;
	uint64_t offset;

	*pfile = NULL;
	for (;;) {
		*pfile = file = next_entry(tp);
		if (file == NULL)
			return (ARCHIVE_EOF);
		offset = file->offset;

		/* Seek forward to the start of the entry. */
		while (tp->current_position < offset) {
			ssize_t step = offset - tp->current_position;
			ssize_t bytes_read;
			const void *buff;

			if (step > 512)
				step = 512;
			bytes_read = (a->compression_read_ahead)(a, &buff, step);
			if (bytes_read <= 0) {
				release_file(tp, file);
				return (ARCHIVE_FATAL);
			}
			if (bytes_read > step)
				bytes_read = step;
			tp->current_position += bytes_read;
			(a->compression_read_consume)(a, bytes_read);
		}

		/* We found body of file; handle it now. */
		if (offset == file->offset)
			return (ARCHIVE_OK);
	}
}

static struct file_info *
next_entry(struct tp *tp)
{
	int least_index;
	uint64_t least_offset;
	int i;
	struct file_info *r;

	if (tp->pending_files_used < 1)
		return (NULL);

	/* Assume the first file in the list is the earliest on disk. */
	least_index = 0;
	least_offset = tp->pending_files[0]->offset;

	/* Now, try to find an earlier one. */
	for(i = 0; i < tp->pending_files_used; i++) {
		uint64_t offset = tp->pending_files[i]->offset;
		if (least_offset > offset) {
			least_index = i;
			least_offset = offset;
		}
	}
	r = tp->pending_files[least_index];
	tp->pending_files[least_index]
	    = tp->pending_files[--tp->pending_files_used];
	return (r);
}

/*
 * 'tp' format was developed for PDP-11, so it uses the screwy PDP-11
 * byte order, which is big-endian words, little-endian bytes within a
 * word.  In particular, the 32-bit value 0x44332211 gets stored as
 * four bytes: 0x33 0x44 0x11 0x22
 */
static int
toi(const void *p, int n)
{
	const unsigned char *v = (const unsigned char *)p;
	switch(n) {
	case 1: return (v[0]);
	case 2: return (v[0] + v[1] * 0x100);
	case 3: return (v[0] * 0x10000 + toi(v + 1, 2));
	case 4: return (toi(v, 2) * 0x10000 + toi(v + 2, 2));
	default: return (0);
	}
}
