/*-
 * Copyright (c) 2008 Joerg Sonnenberger
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_write_private.h"

struct mtree_writer {
	struct archive_entry *entry;
	struct archive_string buf;
	int first;
};

static int
mtree_safe_char(char c)
{
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
		return 1;
	if (c >= '0' && c <= '9')
		return 1;
	if (c == 35 || c == 61 || c == 92)
		return 0; /* #, = and \ are always quoted */
	
	if (c >= 33 && c <= 47) /* !"$%&'()*+,-./ */
		return 1;
	if (c >= 58 && c <= 64) /* :;<>?@ */
		return 1;
	if (c >= 91 && c <= 96) /* []^_` */
		return 1;
	if (c >= 123 && c <= 126) /* {|}~ */
		return 1;
	return 0;
}

static void
mtree_quote(struct mtree_writer *mtree, const char *str)
{
	const char *start;
	char buf[4];
	unsigned char c;

	for (start = str; *str != '\0'; ++str) {
		if (mtree_safe_char(*str))
			continue;
		if (start != str)
			archive_strncat(&mtree->buf, start, str - start);
		c = (unsigned char)*str;
		buf[0] = '\\';
		buf[1] = (c / 64) + '0';
		buf[2] = (c / 8 % 8) + '0';
		buf[3] = (c % 8) + '0';
		archive_strncat(&mtree->buf, buf, 4);
		start = str + 1;
	}

	if (start != str)
		archive_strncat(&mtree->buf, start, str - start);
}

static int
archive_write_mtree_header(struct archive_write *a,
    struct archive_entry *entry)
{
	struct mtree_writer *mtree= a->format_data;
	const char *path;

	mtree->entry = archive_entry_clone(entry);
	path = archive_entry_pathname(mtree->entry);

	if (mtree->first) {
		mtree->first = 0;
		archive_strcat(&mtree->buf, "#mtree\n");
	}

	mtree_quote(mtree, path);

	return (ARCHIVE_OK);
}

static int
archive_write_mtree_finish_entry(struct archive_write *a)
{
	struct mtree_writer *mtree = a->format_data;
	struct archive_entry *entry;
	const char *name;
	int ret;

	entry = mtree->entry;
	if (entry == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
		    "Finished entry without being open first.");
		return (ARCHIVE_FATAL);
	}
	mtree->entry = NULL;

	if (archive_entry_nlink(entry) != 1 && 
	    archive_entry_filetype(entry) != AE_IFDIR)
		archive_string_sprintf(&mtree->buf,
		    " nlink=%u", archive_entry_nlink(entry));

	if ((name = archive_entry_gname(entry)) != NULL) {
		archive_strcat(&mtree->buf, " gname=");
		mtree_quote(mtree, name);
	}
	if ((name = archive_entry_uname(entry)) != NULL) {
		archive_strcat(&mtree->buf, " uname=");
		mtree_quote(mtree, name);
	}
	if ((name = archive_entry_fflags_text(entry)) != NULL) {
		archive_strcat(&mtree->buf, " flags=");
		mtree_quote(mtree, name);
	}

	archive_string_sprintf(&mtree->buf,
	    " time=%jd mode=%o gid=%jd uid=%jd",
	    (intmax_t)archive_entry_mtime(entry),
	    archive_entry_mode(entry) & 07777,
	    (intmax_t)archive_entry_gid(entry),
	    (intmax_t)archive_entry_uid(entry));

	switch (archive_entry_filetype(entry)) {
	case AE_IFLNK:
		archive_strcat(&mtree->buf, " type=link link=");
		mtree_quote(mtree, archive_entry_symlink(entry));
		archive_strcat(&mtree->buf, "\n");
		break;
	case AE_IFSOCK:
		archive_strcat(&mtree->buf, " type=socket\n");
		break;
	case AE_IFCHR:
		archive_string_sprintf(&mtree->buf,
		    " type=char device=native,%d,%d\n",
		    archive_entry_rdevmajor(entry),
		    archive_entry_rdevminor(entry));
		break;
	case AE_IFBLK:
		archive_string_sprintf(&mtree->buf,
		    " type=block device=native,%d,%d\n",
		    archive_entry_rdevmajor(entry),
		    archive_entry_rdevminor(entry));
		break;
	case AE_IFDIR:
		archive_strcat(&mtree->buf, " type=dir\n");
		break;
	case AE_IFIFO:
		archive_strcat(&mtree->buf, " type=fifo\n");
		break;
	case AE_IFREG:
	default:	/* Handle unknown file types as regular files. */
		archive_string_sprintf(&mtree->buf, " type=file size=%jd\n",
		    (intmax_t)archive_entry_size(entry));
		break;
	}

	archive_entry_free(entry);

	if (mtree->buf.length > 32768) {
		ret = (a->compressor.write)(a, mtree->buf.s, mtree->buf.length);
		archive_string_empty(&mtree->buf);
	} else
		ret = ARCHIVE_OK;

	return (ret == ARCHIVE_OK ? ret : ARCHIVE_FATAL);
}

static int
archive_write_mtree_finish(struct archive_write *a)
{
	struct mtree_writer *mtree= a->format_data;

	archive_write_set_bytes_in_last_block(&a->archive, 1);

	return (a->compressor.write)(a, mtree->buf.s, mtree->buf.length);
}

static ssize_t
archive_write_mtree_data(struct archive_write *a, const void *buff, size_t n)
{
	(void)a; /* UNUSED */
	(void)buff; /* UNUSED */
	return n;
}

static int
archive_write_mtree_destroy(struct archive_write *a)
{
	struct mtree_writer *mtree= a->format_data;

	if (mtree == NULL)
		return (ARCHIVE_OK);

	archive_entry_free(mtree->entry);
	archive_string_free(&mtree->buf);
	free(mtree);
	a->format_data = NULL;
	return (ARCHIVE_OK);
}

int
archive_write_set_format_mtree(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct mtree_writer *mtree;

	if (a->format_destroy != NULL)
		(a->format_destroy)(a);

	if ((mtree = malloc(sizeof(*mtree))) == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate mtree data");
		return (ARCHIVE_FATAL);
	}

	mtree->entry = NULL;
	mtree->first = 1;
	archive_string_init(&mtree->buf);
	a->format_data = mtree;
	a->format_destroy = archive_write_mtree_destroy;

	a->pad_uncompressed = 0;
	a->format_write_header = archive_write_mtree_header;
	a->format_finish = archive_write_mtree_finish;
	a->format_write_data = archive_write_mtree_data;
	a->format_finish_entry = archive_write_mtree_finish_entry;
	a->archive.archive_format = ARCHIVE_FORMAT_MTREE;
	a->archive.archive_format_name = "mtree";

	return (ARCHIVE_OK);
}
