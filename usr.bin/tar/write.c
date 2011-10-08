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

#include "bsdtar_platform.h"
__FBSDID("$FreeBSD$");

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ATTR_XATTR_H
#include <attr/xattr.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LINUX_FS_H
#include <linux/fs.h>	/* for Linux file flags */
#endif
/*
 * Some Linux distributions have both linux/ext2_fs.h and ext2fs/ext2_fs.h.
 * As the include guards don't agree, the order of include is important.
 */
#ifdef HAVE_LINUX_EXT2_FS_H
#include <linux/ext2_fs.h>	/* for Linux file flags */
#endif
#if defined(HAVE_EXT2FS_EXT2_FS_H) && !defined(__CYGWIN__)
/* This header exists but is broken on Cygwin. */
#include <ext2fs/ext2_fs.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "bsdtar.h"
#include "err.h"
#include "line_reader.h"
#include "tree.h"

/* Size of buffer for holding file data prior to writing. */
#define FILEDATABUFLEN	65536

/* Fixed size of uname/gname caches. */
#define	name_cache_size 101

#ifndef O_BINARY
#define O_BINARY 0
#endif

static const char * const NO_NAME = "(noname)";

struct archive_dir_entry {
	struct archive_dir_entry	*next;
	time_t			 mtime_sec;
	int			 mtime_nsec;
	char			*name;
};

struct archive_dir {
	struct archive_dir_entry *head, *tail;
};

struct name_cache {
	int	probes;
	int	hits;
	size_t	size;
	struct {
		id_t id;
		const char *name;
	} cache[name_cache_size];
};

static void		 add_dir_list(struct bsdtar *bsdtar, const char *path,
			     time_t mtime_sec, int mtime_nsec);
static int		 append_archive(struct bsdtar *, struct archive *,
			     struct archive *ina);
static int		 append_archive_filename(struct bsdtar *,
			     struct archive *, const char *fname);
static void		 archive_names_from_file(struct bsdtar *bsdtar,
			     struct archive *a);
static int		 copy_file_data(struct bsdtar *, struct archive *a,
			     struct archive *ina, struct archive_entry *);
static int		 new_enough(struct bsdtar *, const char *path,
			     const struct stat *);
static void		 report_write(struct bsdtar *, struct archive *,
			     struct archive_entry *, int64_t progress);
static void		 test_for_append(struct bsdtar *);
static void		 write_archive(struct archive *, struct bsdtar *);
static void		 write_entry_backend(struct bsdtar *, struct archive *,
			     struct archive_entry *);
static int		 write_file_data(struct bsdtar *, struct archive *,
			     struct archive_entry *, int fd);
static void		 write_hierarchy(struct bsdtar *, struct archive *,
			     const char *);

#if defined(_WIN32) && !defined(__CYGWIN__)
/* Not a full lseek() emulation, but enough for our needs here. */
static int
seek_file(int fd, int64_t offset, int whence)
{
	LARGE_INTEGER distance;
	(void)whence; /* UNUSED */
	distance.QuadPart = offset;
	return (SetFilePointerEx((HANDLE)_get_osfhandle(fd),
		distance, NULL, FILE_BEGIN) ? 1 : -1);
}
#define open _open
#define close _close
#define read _read
#define lseek seek_file
#endif

void
tar_mode_c(struct bsdtar *bsdtar)
{
	struct archive *a;
	int r;

	if (*bsdtar->argv == NULL && bsdtar->names_from_file == NULL)
		lafe_errc(1, 0, "no files or directories specified");

	a = archive_write_new();

	/* Support any format that the library supports. */
	if (bsdtar->create_format == NULL) {
		r = archive_write_set_format_pax_restricted(a);
		bsdtar->create_format = "pax restricted";
	} else {
		r = archive_write_set_format_by_name(a, bsdtar->create_format);
	}
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "Can't use format %s: %s\n",
		    bsdtar->create_format,
		    archive_error_string(a));
		usage();
	}

	/*
	 * If user explicitly set the block size, then assume they
	 * want the last block padded as well.  Otherwise, use the
	 * default block size and accept archive_write_open_file()'s
	 * default padding decisions.
	 */
	if (bsdtar->bytes_per_block != 0) {
		archive_write_set_bytes_per_block(a, bsdtar->bytes_per_block);
		archive_write_set_bytes_in_last_block(a,
		    bsdtar->bytes_per_block);
	} else
		archive_write_set_bytes_per_block(a, DEFAULT_BYTES_PER_BLOCK);

	if (bsdtar->compress_program) {
		archive_write_set_compression_program(a, bsdtar->compress_program);
	} else {
		switch (bsdtar->create_compression) {
		case 0:
			r = archive_write_set_compression_none(a);
			break;
		case 'j': case 'y':
			r = archive_write_set_compression_bzip2(a);
			break;
		case 'J':
			r = archive_write_set_compression_xz(a);
			break;
		case OPTION_LZMA:
			r = archive_write_set_compression_lzma(a);
			break;
		case 'z':
			r = archive_write_set_compression_gzip(a);
			break;
		case 'Z':
			r = archive_write_set_compression_compress(a);
			break;
		default:
			lafe_errc(1, 0,
			    "Unrecognized compression option -%c",
			    bsdtar->create_compression);
		}
		if (r != ARCHIVE_OK) {
			lafe_errc(1, 0,
			    "Unsupported compression option -%c",
			    bsdtar->create_compression);
		}
	}

	if (ARCHIVE_OK != archive_write_set_options(a, bsdtar->option_options))
		lafe_errc(1, 0, "%s", archive_error_string(a));
	if (ARCHIVE_OK != archive_write_open_file(a, bsdtar->filename))
		lafe_errc(1, 0, "%s", archive_error_string(a));
	write_archive(a, bsdtar);
}

/*
 * Same as 'c', except we only support tar or empty formats in
 * uncompressed files on disk.
 */
void
tar_mode_r(struct bsdtar *bsdtar)
{
	int64_t	end_offset;
	int	format;
	struct archive *a;
	struct archive_entry *entry;
	int	r;

	/* Sanity-test some arguments and the file. */
	test_for_append(bsdtar);

	format = ARCHIVE_FORMAT_TAR_PAX_RESTRICTED;

#if defined(__BORLANDC__)
	bsdtar->fd = open(bsdtar->filename, O_RDWR | O_CREAT | O_BINARY);
#else
	bsdtar->fd = open(bsdtar->filename, O_RDWR | O_CREAT | O_BINARY, 0666);
#endif
	if (bsdtar->fd < 0)
		lafe_errc(1, errno,
		    "Cannot open %s", bsdtar->filename);

	a = archive_read_new();
	archive_read_support_compression_all(a);
	archive_read_support_format_tar(a);
	archive_read_support_format_gnutar(a);
	r = archive_read_open_fd(a, bsdtar->fd, 10240);
	if (r != ARCHIVE_OK)
		lafe_errc(1, archive_errno(a),
		    "Can't read archive %s: %s", bsdtar->filename,
		    archive_error_string(a));
	while (0 == archive_read_next_header(a, &entry)) {
		if (archive_compression(a) != ARCHIVE_COMPRESSION_NONE) {
			archive_read_finish(a);
			close(bsdtar->fd);
			lafe_errc(1, 0,
			    "Cannot append to compressed archive.");
		}
		/* Keep going until we hit end-of-archive */
		format = archive_format(a);
	}

	end_offset = archive_read_header_position(a);
	archive_read_finish(a);

	/* Re-open archive for writing */
	a = archive_write_new();
	archive_write_set_compression_none(a);
	/*
	 * Set the format to be used for writing.  To allow people to
	 * extend empty files, we need to allow them to specify the format,
	 * which opens the possibility that they will specify a format that
	 * doesn't match the existing format.  Hence, the following bit
	 * of arcane ugliness.
	 */

	if (bsdtar->create_format != NULL) {
		/* If the user requested a format, use that, but ... */
		archive_write_set_format_by_name(a,
		    bsdtar->create_format);
		/* ... complain if it's not compatible. */
		format &= ARCHIVE_FORMAT_BASE_MASK;
		if (format != (int)(archive_format(a) & ARCHIVE_FORMAT_BASE_MASK)
		    && format != ARCHIVE_FORMAT_EMPTY) {
			lafe_errc(1, 0,
			    "Format %s is incompatible with the archive %s.",
			    bsdtar->create_format, bsdtar->filename);
		}
	} else {
		/*
		 * Just preserve the current format, with a little care
		 * for formats that libarchive can't write.
		 */
		if (format == ARCHIVE_FORMAT_TAR_GNUTAR)
			/* TODO: When gtar supports pax, use pax restricted. */
			format = ARCHIVE_FORMAT_TAR_USTAR;
		if (format == ARCHIVE_FORMAT_EMPTY)
			format = ARCHIVE_FORMAT_TAR_PAX_RESTRICTED;
		archive_write_set_format(a, format);
	}
	if (lseek(bsdtar->fd, end_offset, SEEK_SET) < 0)
		lafe_errc(1, errno, "Could not seek to archive end");
	if (ARCHIVE_OK != archive_write_set_options(a, bsdtar->option_options))
		lafe_errc(1, 0, "%s", archive_error_string(a));
	if (ARCHIVE_OK != archive_write_open_fd(a, bsdtar->fd))
		lafe_errc(1, 0, "%s", archive_error_string(a));

	write_archive(a, bsdtar); /* XXX check return val XXX */

	close(bsdtar->fd);
	bsdtar->fd = -1;
}

void
tar_mode_u(struct bsdtar *bsdtar)
{
	int64_t			 end_offset;
	struct archive		*a;
	struct archive_entry	*entry;
	int			 format;
	struct archive_dir_entry	*p;
	struct archive_dir	 archive_dir;

	bsdtar->archive_dir = &archive_dir;
	memset(&archive_dir, 0, sizeof(archive_dir));

	format = ARCHIVE_FORMAT_TAR_PAX_RESTRICTED;

	/* Sanity-test some arguments and the file. */
	test_for_append(bsdtar);

	bsdtar->fd = open(bsdtar->filename, O_RDWR | O_BINARY);
	if (bsdtar->fd < 0)
		lafe_errc(1, errno,
		    "Cannot open %s", bsdtar->filename);

	a = archive_read_new();
	archive_read_support_compression_all(a);
	archive_read_support_format_tar(a);
	archive_read_support_format_gnutar(a);
	if (archive_read_open_fd(a, bsdtar->fd,
	    bsdtar->bytes_per_block != 0 ? bsdtar->bytes_per_block :
		DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
		lafe_errc(1, 0,
		    "Can't open %s: %s", bsdtar->filename,
		    archive_error_string(a));
	}

	/* Build a list of all entries and their recorded mod times. */
	while (0 == archive_read_next_header(a, &entry)) {
		if (archive_compression(a) != ARCHIVE_COMPRESSION_NONE) {
			archive_read_finish(a);
			close(bsdtar->fd);
			lafe_errc(1, 0,
			    "Cannot append to compressed archive.");
		}
		add_dir_list(bsdtar, archive_entry_pathname(entry),
		    archive_entry_mtime(entry),
		    archive_entry_mtime_nsec(entry));
		/* Record the last format determination we see */
		format = archive_format(a);
		/* Keep going until we hit end-of-archive */
	}

	end_offset = archive_read_header_position(a);
	archive_read_finish(a);

	/* Re-open archive for writing. */
	a = archive_write_new();
	archive_write_set_compression_none(a);
	/*
	 * Set format to same one auto-detected above, except that
	 * we don't write GNU tar format, so use ustar instead.
	 */
	if (format == ARCHIVE_FORMAT_TAR_GNUTAR)
		format = ARCHIVE_FORMAT_TAR_USTAR;
	archive_write_set_format(a, format);
	if (bsdtar->bytes_per_block != 0) {
		archive_write_set_bytes_per_block(a, bsdtar->bytes_per_block);
		archive_write_set_bytes_in_last_block(a,
		    bsdtar->bytes_per_block);
	} else
		archive_write_set_bytes_per_block(a, DEFAULT_BYTES_PER_BLOCK);
	if (lseek(bsdtar->fd, end_offset, SEEK_SET) < 0)
		lafe_errc(1, errno, "Could not seek to archive end");
	if (ARCHIVE_OK != archive_write_set_options(a, bsdtar->option_options))
		lafe_errc(1, 0, "%s", archive_error_string(a));
	if (ARCHIVE_OK != archive_write_open_fd(a, bsdtar->fd))
		lafe_errc(1, 0, "%s", archive_error_string(a));

	write_archive(a, bsdtar);

	close(bsdtar->fd);
	bsdtar->fd = -1;

	while (bsdtar->archive_dir->head != NULL) {
		p = bsdtar->archive_dir->head->next;
		free(bsdtar->archive_dir->head->name);
		free(bsdtar->archive_dir->head);
		bsdtar->archive_dir->head = p;
	}
	bsdtar->archive_dir->tail = NULL;
}


/*
 * Write user-specified files/dirs to opened archive.
 */
static void
write_archive(struct archive *a, struct bsdtar *bsdtar)
{
	const char *arg;
	struct archive_entry *entry, *sparse_entry;

	/* Allocate a buffer for file data. */
	if ((bsdtar->buff = malloc(FILEDATABUFLEN)) == NULL)
		lafe_errc(1, 0, "cannot allocate memory");

	if ((bsdtar->resolver = archive_entry_linkresolver_new()) == NULL)
		lafe_errc(1, 0, "cannot create link resolver");
	archive_entry_linkresolver_set_strategy(bsdtar->resolver,
	    archive_format(a));
	if ((bsdtar->diskreader = archive_read_disk_new()) == NULL)
		lafe_errc(1, 0, "Cannot create read_disk object");
	archive_read_disk_set_standard_lookup(bsdtar->diskreader);

	if (bsdtar->names_from_file != NULL)
		archive_names_from_file(bsdtar, a);

	while (*bsdtar->argv) {
		arg = *bsdtar->argv;
		if (arg[0] == '-' && arg[1] == 'C') {
			arg += 2;
			if (*arg == '\0') {
				bsdtar->argv++;
				arg = *bsdtar->argv;
				if (arg == NULL) {
					lafe_warnc(0, "%s",
					    "Missing argument for -C");
					bsdtar->return_value = 1;
					goto cleanup;
				}
			}
			set_chdir(bsdtar, arg);
		} else {
			if (*arg != '/' && (arg[0] != '@' || arg[1] != '/'))
				do_chdir(bsdtar); /* Handle a deferred -C */
			if (*arg == '@') {
				if (append_archive_filename(bsdtar, a,
				    arg + 1) != 0)
					break;
			} else
				write_hierarchy(bsdtar, a, arg);
		}
		bsdtar->argv++;
	}

	entry = NULL;
	archive_entry_linkify(bsdtar->resolver, &entry, &sparse_entry);
	while (entry != NULL) {
		write_entry_backend(bsdtar, a, entry);
		archive_entry_free(entry);
		entry = NULL;
		archive_entry_linkify(bsdtar->resolver, &entry, &sparse_entry);
	}

	if (archive_write_close(a)) {
		lafe_warnc(0, "%s", archive_error_string(a));
		bsdtar->return_value = 1;
	}

cleanup:
	/* Free file data buffer. */
	free(bsdtar->buff);
	archive_entry_linkresolver_free(bsdtar->resolver);
	bsdtar->resolver = NULL;
	archive_read_finish(bsdtar->diskreader);
	bsdtar->diskreader = NULL;

	if (bsdtar->option_totals) {
		fprintf(stderr, "Total bytes written: %s\n",
		    tar_i64toa(archive_position_compressed(a)));
	}

	archive_write_finish(a);
}

/*
 * Archive names specified in file.
 *
 * Unless --null was specified, a line containing exactly "-C" will
 * cause the next line to be a directory to pass to chdir().  If
 * --null is specified, then a line "-C" is just another filename.
 */
static void
archive_names_from_file(struct bsdtar *bsdtar, struct archive *a)
{
	struct lafe_line_reader *lr;
	const char *line;

	bsdtar->next_line_is_dir = 0;

	lr = lafe_line_reader(bsdtar->names_from_file, bsdtar->option_null);
	while ((line = lafe_line_reader_next(lr)) != NULL) {
		if (bsdtar->next_line_is_dir) {
			set_chdir(bsdtar, line);
			bsdtar->next_line_is_dir = 0;
		} else if (!bsdtar->option_null && strcmp(line, "-C") == 0)
			bsdtar->next_line_is_dir = 1;
		else {
			if (*line != '/')
				do_chdir(bsdtar); /* Handle a deferred -C */
			write_hierarchy(bsdtar, a, line);
		}
	}
	lafe_line_reader_free(lr);
	if (bsdtar->next_line_is_dir)
		lafe_errc(1, errno,
		    "Unexpected end of filename list; "
		    "directory expected after -C");
}

/*
 * Copy from specified archive to current archive.  Returns non-zero
 * for write errors (which force us to terminate the entire archiving
 * operation).  If there are errors reading the input archive, we set
 * bsdtar->return_value but return zero, so the overall archiving
 * operation will complete and return non-zero.
 */
static int
append_archive_filename(struct bsdtar *bsdtar, struct archive *a,
    const char *filename)
{
	struct archive *ina;
	int rc;

	if (strcmp(filename, "-") == 0)
		filename = NULL; /* Library uses NULL for stdio. */

	ina = archive_read_new();
	archive_read_support_format_all(ina);
	archive_read_support_compression_all(ina);
	if (archive_read_open_file(ina, filename, 10240)) {
		lafe_warnc(0, "%s", archive_error_string(ina));
		bsdtar->return_value = 1;
		return (0);
	}

	rc = append_archive(bsdtar, a, ina);

	if (rc != ARCHIVE_OK) {
		lafe_warnc(0, "Error reading archive %s: %s",
		    filename, archive_error_string(ina));
		bsdtar->return_value = 1;
	}
	archive_read_finish(ina);

	return (rc);
}

static int
append_archive(struct bsdtar *bsdtar, struct archive *a, struct archive *ina)
{
	struct archive_entry *in_entry;
	int e;

	while (0 == archive_read_next_header(ina, &in_entry)) {
		if (!new_enough(bsdtar, archive_entry_pathname(in_entry),
			archive_entry_stat(in_entry)))
			continue;
		if (lafe_excluded(bsdtar->matching, archive_entry_pathname(in_entry)))
			continue;
		if (bsdtar->option_interactive &&
		    !yes("copy '%s'", archive_entry_pathname(in_entry)))
			continue;
		if (bsdtar->verbose)
			safe_fprintf(stderr, "a %s",
			    archive_entry_pathname(in_entry));
		if (need_report())
			report_write(bsdtar, a, in_entry, 0);

		e = archive_write_header(a, in_entry);
		if (e != ARCHIVE_OK) {
			if (!bsdtar->verbose)
				lafe_warnc(0, "%s: %s",
				    archive_entry_pathname(in_entry),
				    archive_error_string(a));
			else
				fprintf(stderr, ": %s", archive_error_string(a));
		}
		if (e == ARCHIVE_FATAL)
			exit(1);

		if (e >= ARCHIVE_WARN) {
			if (archive_entry_size(in_entry) == 0)
				archive_read_data_skip(ina);
			else if (copy_file_data(bsdtar, a, ina, in_entry))
				exit(1);
		}

		if (bsdtar->verbose)
			fprintf(stderr, "\n");
	}

	/* Note: If we got here, we saw no write errors, so return success. */
	return (0);
}

/* Helper function to copy data between archives. */
static int
copy_file_data(struct bsdtar *bsdtar, struct archive *a,
    struct archive *ina, struct archive_entry *entry)
{
	ssize_t	bytes_read;
	ssize_t	bytes_written;
	int64_t	progress = 0;

	bytes_read = archive_read_data(ina, bsdtar->buff, FILEDATABUFLEN);
	while (bytes_read > 0) {
		if (need_report())
			report_write(bsdtar, a, entry, progress);

		bytes_written = archive_write_data(a, bsdtar->buff,
		    bytes_read);
		if (bytes_written < bytes_read) {
			lafe_warnc(0, "%s", archive_error_string(a));
			return (-1);
		}
		progress += bytes_written;
		bytes_read = archive_read_data(ina, bsdtar->buff,
		    FILEDATABUFLEN);
	}

	return (0);
}

/*
 * Add the file or dir hierarchy named by 'path' to the archive
 */
static void
write_hierarchy(struct bsdtar *bsdtar, struct archive *a, const char *path)
{
	struct archive_entry *entry = NULL, *spare_entry = NULL;
	struct tree *tree;
	char symlink_mode = bsdtar->symlink_mode;
	dev_t first_dev = 0;
	int dev_recorded = 0;
	int tree_ret;

	tree = tree_open(path);

	if (!tree) {
		lafe_warnc(errno, "%s: Cannot open", path);
		bsdtar->return_value = 1;
		return;
	}

	while ((tree_ret = tree_next(tree)) != 0) {
		int r;
		const char *name = tree_current_path(tree);
		const struct stat *st = NULL; /* info to use for this entry */
		const struct stat *lst = NULL; /* lstat() information */
		int descend;

		if (tree_ret == TREE_ERROR_FATAL)
			lafe_errc(1, tree_errno(tree),
			    "%s: Unable to continue traversing directory tree",
			    name);
		if (tree_ret == TREE_ERROR_DIR) {
			lafe_warnc(errno,
			    "%s: Couldn't visit directory", name);
			bsdtar->return_value = 1;
		}
		if (tree_ret != TREE_REGULAR)
			continue;

		/*
		 * If this file/dir is excluded by a filename
		 * pattern, skip it.
		 */
		if (lafe_excluded(bsdtar->matching, name))
			continue;

		/*
		 * Get lstat() info from the tree library.
		 */
		lst = tree_current_lstat(tree);
		if (lst == NULL) {
			/* Couldn't lstat(); must not exist. */
			lafe_warnc(errno, "%s: Cannot stat", name);
			/* Return error if files disappear during traverse. */
			bsdtar->return_value = 1;
			continue;
		}

		/*
		 * Distinguish 'L'/'P'/'H' symlink following.
		 */
		switch(symlink_mode) {
		case 'H':
			/* 'H': After the first item, rest like 'P'. */
			symlink_mode = 'P';
			/* 'H': First item (from command line) like 'L'. */
			/* FALLTHROUGH */
		case 'L':
			/* 'L': Do descend through a symlink to dir. */
			descend = tree_current_is_dir(tree);
			/* 'L': Follow symlinks to files. */
			archive_read_disk_set_symlink_logical(bsdtar->diskreader);
			/* 'L': Archive symlinks as targets, if we can. */
			st = tree_current_stat(tree);
			if (st != NULL)
				break;
			/* If stat fails, we have a broken symlink;
			 * in that case, don't follow the link. */
			/* FALLTHROUGH */
		default:
			/* 'P': Don't descend through a symlink to dir. */
			descend = tree_current_is_physical_dir(tree);
			/* 'P': Don't follow symlinks to files. */
			archive_read_disk_set_symlink_physical(bsdtar->diskreader);
			/* 'P': Archive symlinks as symlinks. */
			st = lst;
			break;
		}

		if (bsdtar->option_no_subdirs)
			descend = 0;

		/*
		 * Are we about to cross to a new filesystem?
		 */
		if (!dev_recorded) {
			/* This is the initial file system. */
			first_dev = lst->st_dev;
			dev_recorded = 1;
		} else if (lst->st_dev == first_dev) {
			/* The starting file system is always acceptable. */
		} else if (descend == 0) {
			/* We're not descending, so no need to check. */
		} else if (bsdtar->option_dont_traverse_mounts) {
			descend = 0;
		} else {
			/* We're prepared to cross a mount point. */

			/* XXX TODO: check whether this filesystem is
			 * synthetic and/or local.  Add a new
			 * --local-only option to skip non-local
			 * filesystems.  Skip synthetic filesystems
			 * regardless.
			 *
			 * The results should be cached, since
			 * tree.c doesn't usually visit a directory
			 * and the directory contents together.  A simple
			 * move-to-front list should perform quite well.
			 *
			 * This is going to be heavily OS dependent:
			 * FreeBSD's statfs() in conjunction with getvfsbyname()
			 * provides all of this; NetBSD's statvfs() does
			 * most of it; other systems will vary.
			 */
		}

		/*
		 * In -u mode, check that the file is newer than what's
		 * already in the archive; in all modes, obey --newerXXX flags.
		 */
		if (!new_enough(bsdtar, name, st)) {
			if (!descend)
				continue;
			if (bsdtar->option_interactive &&
			    !yes("add '%s'", name))
				continue;
			tree_descend(tree);
			continue;
		}

		archive_entry_free(entry);
		entry = archive_entry_new();

		archive_entry_set_pathname(entry, name);
		archive_entry_copy_sourcepath(entry,
		    tree_current_access_path(tree));

		/* Populate the archive_entry with metadata from the disk. */
		/* XXX TODO: Arrange to open a regular file before
		 * calling this so we can pass in an fd and shorten
		 * the race to query metadata.  The linkify dance
		 * makes this more complex than it might sound. */
#if defined(_WIN32) && !defined(__CYGWIN__)
		/* TODO: tree.c uses stat(), which is badly broken
		 * on Windows.  To fix this, we should
		 * deprecate tree_current_stat() and provide a new
		 * call tree_populate_entry(t, entry).  This call
		 * would use stat() internally on POSIX and
		 * GetInfoByFileHandle() internally on Windows.
		 * This would be another step towards a tree-walker
		 * that can be integrated deep into libarchive.
		 * For now, just set st to NULL on Windows;
		 * archive_read_disk_entry_from_file() should
		 * be smart enough to use platform-appropriate
		 * ways to probe file information.
		 */
		st = NULL;
#endif
		r = archive_read_disk_entry_from_file(bsdtar->diskreader,
		    entry, -1, st);
		if (r != ARCHIVE_OK)
			lafe_warnc(archive_errno(bsdtar->diskreader),
			    "%s", archive_error_string(bsdtar->diskreader));
		if (r < ARCHIVE_WARN)
			continue;

		/* XXX TODO: Just use flag data from entry; avoid the
		 * duplicate check here. */

		/*
		 * If this file/dir is flagged "nodump" and we're
		 * honoring such flags, skip this file/dir.
		 */
#if defined(HAVE_STRUCT_STAT_ST_FLAGS) && defined(UF_NODUMP)
		/* BSD systems store flags in struct stat */
		if (bsdtar->option_honor_nodump &&
		    (lst->st_flags & UF_NODUMP))
			continue;
#endif

#if defined(EXT2_IOC_GETFLAGS) && defined(EXT2_NODUMP_FL)
		/* Linux uses ioctl to read flags. */
		if (bsdtar->option_honor_nodump) {
			int fd = open(name, O_RDONLY | O_NONBLOCK | O_BINARY);
			if (fd >= 0) {
				unsigned long fflags;
				int r = ioctl(fd, EXT2_IOC_GETFLAGS, &fflags);
				close(fd);
				if (r >= 0 && (fflags & EXT2_NODUMP_FL))
					continue;
			}
		}
#endif

		/*
		 * If the user vetoes this file/directory, skip it.
		 * We want this to be fairly late; if some other
		 * check would veto this file, we shouldn't bother
		 * the user with it.
		 */
		if (bsdtar->option_interactive &&
		    !yes("add '%s'", name))
			continue;

		if (descend)
			tree_descend(tree);

		/*
		 * Rewrite the pathname to be archived.  If rewrite
		 * fails, skip the entry.
		 */
		if (edit_pathname(bsdtar, entry))
			continue;

		/* Display entry as we process it.
		 * This format is required by SUSv2. */
		if (bsdtar->verbose)
			safe_fprintf(stderr, "a %s",
			    archive_entry_pathname(entry));

		/* Non-regular files get archived with zero size. */
		if (archive_entry_filetype(entry) != AE_IFREG)
			archive_entry_set_size(entry, 0);

		archive_entry_linkify(bsdtar->resolver, &entry, &spare_entry);

		while (entry != NULL) {
			write_entry_backend(bsdtar, a, entry);
			archive_entry_free(entry);
			entry = spare_entry;
			spare_entry = NULL;
		}

		if (bsdtar->verbose)
			fprintf(stderr, "\n");
	}
	archive_entry_free(entry);
	tree_close(tree);
}

/*
 * Backend for write_entry.
 */
static void
write_entry_backend(struct bsdtar *bsdtar, struct archive *a,
    struct archive_entry *entry)
{
	int fd = -1;
	int e;

	if (archive_entry_size(entry) > 0) {
		const char *pathname = archive_entry_sourcepath(entry);
		fd = open(pathname, O_RDONLY | O_BINARY);
		if (fd == -1) {
			bsdtar->return_value = 1;
			if (!bsdtar->verbose)
				lafe_warnc(errno,
				    "%s: could not open file", pathname);
			else
				fprintf(stderr, ": %s", strerror(errno));
			return;
		}
	}

	e = archive_write_header(a, entry);
	if (e != ARCHIVE_OK) {
		if (!bsdtar->verbose)
			lafe_warnc(0, "%s: %s",
			    archive_entry_pathname(entry),
			    archive_error_string(a));
		else
			fprintf(stderr, ": %s", archive_error_string(a));
	}

	if (e == ARCHIVE_FATAL)
		exit(1);

	/*
	 * If we opened a file earlier, write it out now.  Note that
	 * the format handler might have reset the size field to zero
	 * to inform us that the archive body won't get stored.  In
	 * that case, just skip the write.
	 */
	if (e >= ARCHIVE_WARN && fd >= 0 && archive_entry_size(entry) > 0) {
		if (write_file_data(bsdtar, a, entry, fd))
			exit(1);
	}

	/*
	 * If we opened a file, close it now even if there was an error
	 * which made us decide not to write the archive body.
	 */
	if (fd >= 0)
		close(fd);
}

static void
report_write(struct bsdtar *bsdtar, struct archive *a,
    struct archive_entry *entry, int64_t progress)
{
	uint64_t comp, uncomp;
	int compression;

	if (bsdtar->verbose)
		fprintf(stderr, "\n");
	comp = archive_position_compressed(a);
	uncomp = archive_position_uncompressed(a);
	fprintf(stderr, "In: %d files, %s bytes;",
	    archive_file_count(a), tar_i64toa(uncomp));
	if (comp > uncomp)
		compression = 0;
	else
		compression = (int)((uncomp - comp) * 100 / uncomp);
	fprintf(stderr,
	    " Out: %s bytes, compression %d%%\n",
	    tar_i64toa(comp), compression);
	/* Can't have two calls to tar_i64toa() pending, so split the output. */
	safe_fprintf(stderr, "Current: %s (%s",
	    archive_entry_pathname(entry),
	    tar_i64toa(progress));
	fprintf(stderr, "/%s bytes)\n",
	    tar_i64toa(archive_entry_size(entry)));
}


/* Helper function to copy file to archive. */
static int
write_file_data(struct bsdtar *bsdtar, struct archive *a,
    struct archive_entry *entry, int fd)
{
	ssize_t	bytes_read;
	ssize_t	bytes_written;
	int64_t	progress = 0;

	bytes_read = read(fd, bsdtar->buff, FILEDATABUFLEN);
	while (bytes_read > 0) {
		if (need_report())
			report_write(bsdtar, a, entry, progress);

		bytes_written = archive_write_data(a, bsdtar->buff,
		    bytes_read);
		if (bytes_written < 0) {
			/* Write failed; this is bad */
			lafe_warnc(0, "%s", archive_error_string(a));
			return (-1);
		}
		if (bytes_written < bytes_read) {
			/* Write was truncated; warn but continue. */
			lafe_warnc(0,
			    "%s: Truncated write; file may have grown while being archived.",
			    archive_entry_pathname(entry));
			return (0);
		}
		progress += bytes_written;
		bytes_read = read(fd, bsdtar->buff, FILEDATABUFLEN);
	}
	if (bytes_read < 0) {
		lafe_warnc(errno,
			     "%s: Read error",
			     archive_entry_pathname(entry));
		bsdtar->return_value = 1;
	}
	return 0;
}

/*
 * Test if the specified file is new enough to include in the archive.
 */
static int
new_enough(struct bsdtar *bsdtar, const char *path, const struct stat *st)
{
	struct archive_dir_entry *p;

	/*
	 * If this file/dir is excluded by a time comparison, skip it.
	 */
	if (bsdtar->newer_ctime_sec > 0) {
		if (st->st_ctime < bsdtar->newer_ctime_sec)
			return (0); /* Too old, skip it. */
		if (st->st_ctime == bsdtar->newer_ctime_sec
		    && ARCHIVE_STAT_CTIME_NANOS(st)
		    <= bsdtar->newer_ctime_nsec)
			return (0); /* Too old, skip it. */
	}
	if (bsdtar->newer_mtime_sec > 0) {
		if (st->st_mtime < bsdtar->newer_mtime_sec)
			return (0); /* Too old, skip it. */
		if (st->st_mtime == bsdtar->newer_mtime_sec
		    && ARCHIVE_STAT_MTIME_NANOS(st)
		    <= bsdtar->newer_mtime_nsec)
			return (0); /* Too old, skip it. */
	}

	/*
	 * In -u mode, we only write an entry if it's newer than
	 * what was already in the archive.
	 */
	if (bsdtar->archive_dir != NULL &&
	    bsdtar->archive_dir->head != NULL) {
		for (p = bsdtar->archive_dir->head; p != NULL; p = p->next) {
			if (pathcmp(path, p->name)==0)
				return (p->mtime_sec < st->st_mtime ||
				    (p->mtime_sec == st->st_mtime &&
					p->mtime_nsec
					< ARCHIVE_STAT_MTIME_NANOS(st)));
		}
	}

	/* If the file wasn't rejected, include it. */
	return (1);
}

/*
 * Add an entry to the dir list for 'u' mode.
 *
 * XXX TODO: Make this fast.
 */
static void
add_dir_list(struct bsdtar *bsdtar, const char *path,
    time_t mtime_sec, int mtime_nsec)
{
	struct archive_dir_entry	*p;

	/*
	 * Search entire list to see if this file has appeared before.
	 * If it has, override the timestamp data.
	 */
	p = bsdtar->archive_dir->head;
	while (p != NULL) {
		if (strcmp(path, p->name)==0) {
			p->mtime_sec = mtime_sec;
			p->mtime_nsec = mtime_nsec;
			return;
		}
		p = p->next;
	}

	p = malloc(sizeof(*p));
	if (p == NULL)
		lafe_errc(1, ENOMEM, "Can't read archive directory");

	p->name = strdup(path);
	if (p->name == NULL)
		lafe_errc(1, ENOMEM, "Can't read archive directory");
	p->mtime_sec = mtime_sec;
	p->mtime_nsec = mtime_nsec;
	p->next = NULL;
	if (bsdtar->archive_dir->tail == NULL) {
		bsdtar->archive_dir->head = bsdtar->archive_dir->tail = p;
	} else {
		bsdtar->archive_dir->tail->next = p;
		bsdtar->archive_dir->tail = p;
	}
}

static void
test_for_append(struct bsdtar *bsdtar)
{
	struct stat s;

	if (*bsdtar->argv == NULL && bsdtar->names_from_file == NULL)
		lafe_errc(1, 0, "no files or directories specified");
	if (bsdtar->filename == NULL)
		lafe_errc(1, 0, "Cannot append to stdout.");

	if (bsdtar->create_compression != 0)
		lafe_errc(1, 0,
		    "Cannot append to %s with compression", bsdtar->filename);

	if (stat(bsdtar->filename, &s) != 0)
		return;

	if (!S_ISREG(s.st_mode) && !S_ISBLK(s.st_mode))
		lafe_errc(1, 0,
		    "Cannot append to %s: not a regular file.",
		    bsdtar->filename);

/* Is this an appropriate check here on Windows? */
/*
	if (GetFileType(handle) != FILE_TYPE_DISK)
		lafe_errc(1, 0, "Cannot append");
*/

}
