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
__FBSDID("$FreeBSD: src/lib/libarchive/archive_write_set_format_shar.c,v 1.18 2007/05/29 01:00:19 kientzle Exp $");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_write_private.h"

struct shar {
	int			 dump;
	int			 end_of_line;
	struct archive_entry	*entry;
	int			 has_data;
	char			*last_dir;
	char			 outbuff[1024];
	size_t			 outbytes;
	size_t			 outpos;
	int			 uuavail;
	char			 uubuffer[3];
	int			 wrote_header;
	struct archive_string	 work;
};

static int	archive_write_shar_finish(struct archive_write *);
static int	archive_write_shar_destroy(struct archive_write *);
static int	archive_write_shar_header(struct archive_write *,
		    struct archive_entry *);
static ssize_t	archive_write_shar_data_sed(struct archive_write *,
		    const void * buff, size_t);
static ssize_t	archive_write_shar_data_uuencode(struct archive_write *,
		    const void * buff, size_t);
static int	archive_write_shar_finish_entry(struct archive_write *);
static int	shar_printf(struct archive_write *, const char *fmt, ...);
static void	uuencode_group(struct shar *);

static int
shar_printf(struct archive_write *a, const char *fmt, ...)
{
	struct shar *shar;
	va_list ap;
	int ret;

	shar = (struct shar *)a->format_data;
	va_start(ap, fmt);
	archive_string_empty(&(shar->work));
	archive_string_vsprintf(&(shar->work), fmt, ap);
	ret = ((a->compressor.write)(a, shar->work.s, strlen(shar->work.s)));
	va_end(ap);
	return (ret);
}

/*
 * Set output format to 'shar' format.
 */
int
archive_write_set_format_shar(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct shar *shar;

	/* If someone else was already registered, unregister them. */
	if (a->format_destroy != NULL)
		(a->format_destroy)(a);

	shar = (struct shar *)malloc(sizeof(*shar));
	if (shar == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Can't allocate shar data");
		return (ARCHIVE_FATAL);
	}
	memset(shar, 0, sizeof(*shar));
	a->format_data = shar;

	a->pad_uncompressed = 0;
	a->format_write_header = archive_write_shar_header;
	a->format_finish = archive_write_shar_finish;
	a->format_destroy = archive_write_shar_destroy;
	a->format_write_data = archive_write_shar_data_sed;
	a->format_finish_entry = archive_write_shar_finish_entry;
	a->archive_format = ARCHIVE_FORMAT_SHAR_BASE;
	a->archive_format_name = "shar";
	return (ARCHIVE_OK);
}

/*
 * An alternate 'shar' that uses uudecode instead of 'sed' to encode
 * file contents and can therefore be used to archive binary files.
 * In addition, this variant also attempts to restore ownership, file modes,
 * and other extended file information.
 */
int
archive_write_set_format_shar_dump(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct shar *shar;

	archive_write_set_format_shar(&a->archive);
	shar = (struct shar *)a->format_data;
	shar->dump = 1;
	a->format_write_data = archive_write_shar_data_uuencode;
	a->archive_format = ARCHIVE_FORMAT_SHAR_DUMP;
	a->archive_format_name = "shar dump";
	return (ARCHIVE_OK);
}

static int
archive_write_shar_header(struct archive_write *a, struct archive_entry *entry)
{
	const char *linkname;
	const char *name;
	char *p, *pp;
	struct shar *shar;
	int ret;

	shar = (struct shar *)a->format_data;
	if (!shar->wrote_header) {
		ret = shar_printf(a, "#!/bin/sh\n");
		if (ret != ARCHIVE_OK)
			return (ret);
		ret = shar_printf(a, "# This is a shell archive\n");
		if (ret != ARCHIVE_OK)
			return (ret);
		shar->wrote_header = 1;
	}

	/* Save the entry for the closing. */
	if (shar->entry)
		archive_entry_free(shar->entry);
	shar->entry = archive_entry_clone(entry);
	name = archive_entry_pathname(entry);

	/* Handle some preparatory issues. */
	switch(archive_entry_filetype(entry)) {
	case AE_IFREG:
		/* Only regular files have non-zero size. */
		break;
	case AE_IFDIR:
		archive_entry_set_size(entry, 0);
		/* Don't bother trying to recreate '.' */
		if (strcmp(name, ".") == 0  ||  strcmp(name, "./") == 0)
			return (ARCHIVE_OK);
		break;
	case AE_IFIFO:
	case AE_IFCHR:
	case AE_IFBLK:
		/* All other file types have zero size in the archive. */
		archive_entry_set_size(entry, 0);
		break;
	default:
		archive_entry_set_size(entry, 0);
		if (archive_entry_hardlink(entry) == NULL &&
		    archive_entry_symlink(entry) == NULL) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "shar format cannot archive this");
			return (ARCHIVE_WARN);
		}
	}

	/* Stock preparation for all file types. */
	ret = shar_printf(a, "echo x %s\n", name);
	if (ret != ARCHIVE_OK)
		return (ret);

	if (archive_entry_filetype(entry) != AE_IFDIR) {
		/* Try to create the dir. */
		p = strdup(name);
		pp = strrchr(p, '/');
		/* If there is a / character, try to create the dir. */
		if (pp != NULL) {
			*pp = '\0';

			/* Try to avoid a lot of redundant mkdir commands. */
			if (strcmp(p, ".") == 0) {
				/* Don't try to "mkdir ." */
				free(p);
			} else if (shar->last_dir == NULL) {
				ret = shar_printf(a,
				    "mkdir -p %s > /dev/null 2>&1\n", p);
				if (ret != ARCHIVE_OK) {
					free(p);
					return (ret);
				}
				shar->last_dir = p;
			} else if (strcmp(p, shar->last_dir) == 0) {
				/* We've already created this exact dir. */
				free(p);
			} else if (strlen(p) < strlen(shar->last_dir) &&
			    strncmp(p, shar->last_dir, strlen(p)) == 0) {
				/* We've already created a subdir. */
				free(p);
			} else {
				ret = shar_printf(a,
				    "mkdir -p %s > /dev/null 2>&1\n", p);
				if (ret != ARCHIVE_OK) {
					free(p);
					return (ret);
				}
				free(shar->last_dir);
				shar->last_dir = p;
			}
		} else {
			free(p);
		}
	}

	/* Handle file-type specific issues. */
	shar->has_data = 0;
	if ((linkname = archive_entry_hardlink(entry)) != NULL) {
		ret = shar_printf(a, "ln -f %s %s\n", linkname, name);
		if (ret != ARCHIVE_OK)
			return (ret);
	} else if ((linkname = archive_entry_symlink(entry)) != NULL) {
		ret = shar_printf(a, "ln -fs %s %s\n", linkname, name);
		if (ret != ARCHIVE_OK)
			return (ret);
	} else {
		switch(archive_entry_filetype(entry)) {
		case AE_IFREG:
			if (archive_entry_size(entry) == 0) {
				/* More portable than "touch." */
				ret = shar_printf(a, "test -e \"%s\" || :> \"%s\"\n", name, name);
				if (ret != ARCHIVE_OK)
					return (ret);
			} else {
				if (shar->dump) {
					ret = shar_printf(a,
					    "uudecode -o %s << 'SHAR_END'\n",
					    name);
					if (ret != ARCHIVE_OK)
						return (ret);
					ret = shar_printf(a, "begin %o %s\n",
					    archive_entry_mode(entry) & 0777,
					    name);
					if (ret != ARCHIVE_OK)
						return (ret);
				} else {
					ret = shar_printf(a,
					    "sed 's/^X//' > %s << 'SHAR_END'\n",
					    name);
					if (ret != ARCHIVE_OK)
						return (ret);
				}
				shar->has_data = 1;
				shar->end_of_line = 1;
				shar->outpos = 0;
				shar->outbytes = 0;
			}
			break;
		case AE_IFDIR:
			ret = shar_printf(a, "mkdir -p %s > /dev/null 2>&1\n",
			    name);
			if (ret != ARCHIVE_OK)
				return (ret);
			/* Record that we just created this directory. */
			if (shar->last_dir != NULL)
				free(shar->last_dir);

			shar->last_dir = strdup(name);
			/* Trim a trailing '/'. */
			pp = strrchr(shar->last_dir, '/');
			if (pp != NULL && pp[1] == '\0')
				*pp = '\0';
			/*
			 * TODO: Put dir name/mode on a list to be fixed
			 * up at end of archive.
			 */
			break;
		case AE_IFIFO:
			ret = shar_printf(a, "mkfifo %s\n", name);
			if (ret != ARCHIVE_OK)
				return (ret);
			break;
		case AE_IFCHR:
			ret = shar_printf(a, "mknod %s c %d %d\n", name,
			    archive_entry_rdevmajor(entry),
			    archive_entry_rdevminor(entry));
			if (ret != ARCHIVE_OK)
				return (ret);
			break;
		case AE_IFBLK:
			ret = shar_printf(a, "mknod %s b %d %d\n", name,
			    archive_entry_rdevmajor(entry),
			    archive_entry_rdevminor(entry));
			if (ret != ARCHIVE_OK)
				return (ret);
			break;
		default:
			return (ARCHIVE_WARN);
		}
	}

	return (ARCHIVE_OK);
}

/* XXX TODO: This could be more efficient XXX */
static ssize_t
archive_write_shar_data_sed(struct archive_write *a, const void *buff, size_t n)
{
	struct shar *shar;
	const char *src;
	int ret;
	size_t written = n;

	shar = (struct shar *)a->format_data;
	if (!shar->has_data)
		return (0);

	src = (const char *)buff;
	ret = ARCHIVE_OK;
	shar->outpos = 0;
	while (n-- > 0) {
		if (shar->end_of_line) {
			shar->outbuff[shar->outpos++] = 'X';
			shar->end_of_line = 0;
		}
		if (*src == '\n')
			shar->end_of_line = 1;
		shar->outbuff[shar->outpos++] = *src++;

		if (shar->outpos > sizeof(shar->outbuff) - 2) {
			ret = (a->compressor.write)(a, shar->outbuff,
			    shar->outpos);
			if (ret != ARCHIVE_OK)
				return (ret);
			shar->outpos = 0;
		}
	}

	if (shar->outpos > 0)
		ret = (a->compressor.write)(a, shar->outbuff, shar->outpos);
	if (ret != ARCHIVE_OK)
		return (ret);
	return (written);
}

#define	UUENC(c)	(((c)!=0) ? ((c) & 077) + ' ': '`')

/* XXX This could be a lot more efficient. XXX */
static void
uuencode_group(struct shar *shar)
{
	int	t;

	t = 0;
	if (shar->uuavail > 0)
		t = 0xff0000 & (shar->uubuffer[0] << 16);
	if (shar->uuavail > 1)
		t |= 0x00ff00 & (shar->uubuffer[1] << 8);
	if (shar->uuavail > 2)
		t |= 0x0000ff & (shar->uubuffer[2]);
	shar->outbuff[shar->outpos++] = UUENC( 0x3f & (t>>18) );
	shar->outbuff[shar->outpos++] = UUENC( 0x3f & (t>>12) );
	shar->outbuff[shar->outpos++] = UUENC( 0x3f & (t>>6) );
	shar->outbuff[shar->outpos++] = UUENC( 0x3f & (t) );
	shar->uuavail = 0;
	shar->outbytes += shar->uuavail;
	shar->outbuff[shar->outpos] = 0;
}

static ssize_t
archive_write_shar_data_uuencode(struct archive_write *a, const void *buff,
    size_t length)
{
	struct shar *shar;
	const char *src;
	size_t n;
	int ret;

	shar = (struct shar *)a->format_data;
	if (!shar->has_data)
		return (ARCHIVE_OK);
	src = (const char *)buff;
	n = length;
	while (n-- > 0) {
		if (shar->uuavail == 3)
			uuencode_group(shar);
		if (shar->outpos >= 60) {
			ret = shar_printf(a, "%c%s\n", UUENC(shar->outbytes),
			    shar->outbuff);
			if (ret != ARCHIVE_OK)
				return (ret);
			shar->outpos = 0;
			shar->outbytes = 0;
		}

		shar->uubuffer[shar->uuavail++] = *src++;
		shar->outbytes++;
	}
	return (length);
}

static int
archive_write_shar_finish_entry(struct archive_write *a)
{
	const char *g, *p, *u;
	struct shar *shar;
	int ret;

	shar = (struct shar *)a->format_data;
	if (shar->entry == NULL)
		return (0);

	if (shar->dump) {
		/* Finish uuencoded data. */
		if (shar->has_data) {
			if (shar->uuavail > 0)
				uuencode_group(shar);
			if (shar->outpos > 0) {
				ret = shar_printf(a, "%c%s\n",
				    UUENC(shar->outbytes), shar->outbuff);
				if (ret != ARCHIVE_OK)
					return (ret);
				shar->outpos = 0;
				shar->uuavail = 0;
				shar->outbytes = 0;
			}
			ret = shar_printf(a, "%c\n", UUENC(0));
			if (ret != ARCHIVE_OK)
				return (ret);
			ret = shar_printf(a, "end\n", UUENC(0));
			if (ret != ARCHIVE_OK)
				return (ret);
			ret = shar_printf(a, "SHAR_END\n");
			if (ret != ARCHIVE_OK)
				return (ret);
		}
		/* Restore file mode, owner, flags. */
		/*
		 * TODO: Don't immediately restore mode for
		 * directories; defer that to end of script.
		 */
		ret = shar_printf(a, "chmod %o %s\n",
		    archive_entry_mode(shar->entry) & 07777,
		    archive_entry_pathname(shar->entry));
		if (ret != ARCHIVE_OK)
			return (ret);

		u = archive_entry_uname(shar->entry);
		g = archive_entry_gname(shar->entry);
		if (u != NULL || g != NULL) {
			ret = shar_printf(a, "chown %s%s%s %s\n",
			    (u != NULL) ? u : "",
			    (g != NULL) ? ":" : "", (g != NULL) ? g : "",
			    archive_entry_pathname(shar->entry));
			if (ret != ARCHIVE_OK)
				return (ret);
		}

		if ((p = archive_entry_fflags_text(shar->entry)) != NULL) {
			ret = shar_printf(a, "chflags %s %s\n", p,
			    archive_entry_pathname(shar->entry));
			if (ret != ARCHIVE_OK)
				return (ret);
		}

		/* TODO: restore ACLs */

	} else {
		if (shar->has_data) {
			/* Finish sed-encoded data:  ensure last line ends. */
			if (!shar->end_of_line) {
				ret = shar_printf(a, "\n");
				if (ret != ARCHIVE_OK)
					return (ret);
			}
			ret = shar_printf(a, "SHAR_END\n");
			if (ret != ARCHIVE_OK)
				return (ret);
		}
	}

	archive_entry_free(shar->entry);
	shar->entry = NULL;
	return (0);
}

static int
archive_write_shar_finish(struct archive_write *a)
{
	struct shar *shar;
	int ret;

	/*
	 * TODO: Accumulate list of directory names/modes and
	 * fix them all up at end-of-archive.
	 */

	shar = (struct shar *)a->format_data;

	/*
	 * Only write the end-of-archive markers if the archive was
	 * actually started.  This avoids problems if someone sets
	 * shar format, then sets another format (which would invoke
	 * shar_finish to free the format-specific data).
	 */
	if (shar->wrote_header) {
		ret = shar_printf(a, "exit\n");
		if (ret != ARCHIVE_OK)
			return (ret);
		/* Shar output is never padded. */
		archive_write_set_bytes_in_last_block(&a->archive, 1);
		/*
		 * TODO: shar should also suppress padding of
		 * uncompressed data within gzip/bzip2 streams.
		 */
	}
	return (ARCHIVE_OK);
}

static int
archive_write_shar_destroy(struct archive_write *a)
{
	struct shar *shar;

	shar = (struct shar *)a->format_data;
	if (shar->entry != NULL)
		archive_entry_free(shar->entry);
	if (shar->last_dir != NULL)
		free(shar->last_dir);
	archive_string_free(&(shar->work));
	free(shar);
	a->format_data = NULL;
	return (ARCHIVE_OK);
}
