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
#ifdef HAVE_SYS_ACL_H
#include <sys/acl.h>
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
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_FNMATCH_H
#include <fnmatch.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LINUX_FS_H
#include <linux/fs.h>	/* for Linux file flags */
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
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
#include "tree.h"

/* Size of buffer for holding file data prior to writing. */
#define FILEDATABUFLEN	65536

/* Fixed size of uname/gname caches. */
#define	name_cache_size 101

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
static int		 archive_names_from_file_helper(struct bsdtar *bsdtar,
			     const char *line);
static int		 copy_file_data(struct bsdtar *bsdtar,
			     struct archive *a, struct archive *ina);
static void		 create_cleanup(struct bsdtar *);
static void		 free_cache(struct name_cache *cache);
static const char *	 lookup_gname(struct bsdtar *bsdtar, gid_t gid);
static int		 lookup_gname_helper(struct bsdtar *bsdtar,
			     const char **name, id_t gid);
static const char *	 lookup_uname(struct bsdtar *bsdtar, uid_t uid);
static int		 lookup_uname_helper(struct bsdtar *bsdtar,
			     const char **name, id_t uid);
static int		 new_enough(struct bsdtar *, const char *path,
			     const struct stat *);
static void		 setup_acls(struct bsdtar *, struct archive_entry *,
			     const char *path);
static void		 setup_xattrs(struct bsdtar *, struct archive_entry *,
			     const char *path);
static void		 test_for_append(struct bsdtar *);
static void		 write_archive(struct archive *, struct bsdtar *);
static void		 write_entry(struct bsdtar *, struct archive *,
			     const struct stat *, const char *pathname,
			     const char *accpath);
static void		 write_entry_backend(struct bsdtar *, struct archive *,
			     struct archive_entry *);
static int		 write_file_data(struct bsdtar *, struct archive *,
			     struct archive_entry *, int fd);
static void		 write_hierarchy(struct bsdtar *, struct archive *,
			     const char *);

void
tar_mode_c(struct bsdtar *bsdtar)
{
	struct archive *a;
	int r;

	if (*bsdtar->argv == NULL && bsdtar->names_from_file == NULL)
		bsdtar_errc(bsdtar, 1, 0, "no files or directories specified");

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
		usage(bsdtar);
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
			archive_write_set_compression_none(a);
			break;
#ifdef HAVE_LIBBZ2
		case 'j': case 'y':
			archive_write_set_compression_bzip2(a);
			break;
#endif
#ifdef HAVE_LIBZ
		case 'z':
			archive_write_set_compression_gzip(a);
			break;
#endif
		case 'Z':
			archive_write_set_compression_compress(a);
			break;
		default:
			bsdtar_errc(bsdtar, 1, 0,
			    "Unrecognized compression option -%c",
			    bsdtar->create_compression);
		}
	}

	r = archive_write_open_file(a, bsdtar->filename);
	if (r != ARCHIVE_OK)
		bsdtar_errc(bsdtar, 1, 0, archive_error_string(a));

	write_archive(a, bsdtar);
}

/*
 * Same as 'c', except we only support tar or empty formats in
 * uncompressed files on disk.
 */
void
tar_mode_r(struct bsdtar *bsdtar)
{
	off_t	end_offset;
	int	format;
	struct archive *a;
	struct archive_entry *entry;
	int	r;

	/* Sanity-test some arguments and the file. */
	test_for_append(bsdtar);

	format = ARCHIVE_FORMAT_TAR_PAX_RESTRICTED;

	bsdtar->fd = open(bsdtar->filename, O_RDWR | O_CREAT, 0666);
	if (bsdtar->fd < 0)
		bsdtar_errc(bsdtar, 1, errno,
		    "Cannot open %s", bsdtar->filename);

	a = archive_read_new();
	archive_read_support_compression_all(a);
	archive_read_support_format_tar(a);
	archive_read_support_format_gnutar(a);
	r = archive_read_open_fd(a, bsdtar->fd, 10240);
	if (r != ARCHIVE_OK)
		bsdtar_errc(bsdtar, 1, archive_errno(a),
		    "Can't read archive %s: %s", bsdtar->filename,
		    archive_error_string(a));
	while (0 == archive_read_next_header(a, &entry)) {
		if (archive_compression(a) != ARCHIVE_COMPRESSION_NONE) {
			archive_read_finish(a);
			close(bsdtar->fd);
			bsdtar_errc(bsdtar, 1, 0,
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
			bsdtar_errc(bsdtar, 1, 0,
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
	lseek(bsdtar->fd, end_offset, SEEK_SET); /* XXX check return val XXX */
	archive_write_open_fd(a, bsdtar->fd); /* XXX check return val XXX */

	write_archive(a, bsdtar); /* XXX check return val XXX */

	close(bsdtar->fd);
	bsdtar->fd = -1;
}

void
tar_mode_u(struct bsdtar *bsdtar)
{
	off_t			 end_offset;
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

	bsdtar->fd = open(bsdtar->filename, O_RDWR);
	if (bsdtar->fd < 0)
		bsdtar_errc(bsdtar, 1, errno,
		    "Cannot open %s", bsdtar->filename);

	a = archive_read_new();
	archive_read_support_compression_all(a);
	archive_read_support_format_tar(a);
	archive_read_support_format_gnutar(a);
	if (archive_read_open_fd(a, bsdtar->fd,
	    bsdtar->bytes_per_block != 0 ? bsdtar->bytes_per_block :
		DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
		bsdtar_errc(bsdtar, 1, 0,
		    "Can't open %s: %s", bsdtar->filename,
		    archive_error_string(a));
	}

	/* Build a list of all entries and their recorded mod times. */
	while (0 == archive_read_next_header(a, &entry)) {
		if (archive_compression(a) != ARCHIVE_COMPRESSION_NONE) {
			archive_read_finish(a);
			close(bsdtar->fd);
			bsdtar_errc(bsdtar, 1, 0,
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
	lseek(bsdtar->fd, end_offset, SEEK_SET);
	ftruncate(bsdtar->fd, end_offset);
	archive_write_open_fd(a, bsdtar->fd);

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

	/* We want to catch SIGINFO and SIGUSR1. */
	siginfo_init(bsdtar);

	/* Allocate a buffer for file data. */
	if ((bsdtar->buff = malloc(FILEDATABUFLEN)) == NULL)
		bsdtar_errc(bsdtar, 1, 0, "cannot allocate memory");

	if ((bsdtar->resolver = archive_entry_linkresolver_new()) == NULL)
		bsdtar_errc(bsdtar, 1, 0, "cannot create link resolver");
	archive_entry_linkresolver_set_strategy(bsdtar->resolver,
	    archive_format(a));

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
					bsdtar_warnc(bsdtar, 1, 0,
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

	create_cleanup(bsdtar);
	if (archive_write_close(a)) {
		bsdtar_warnc(bsdtar, 0, "%s", archive_error_string(a));
		bsdtar->return_value = 1;
	}

cleanup:
	/* Free file data buffer. */
	free(bsdtar->buff);

	if (bsdtar->option_totals) {
		fprintf(stderr, "Total bytes written: " BSDTAR_FILESIZE_PRINTF "\n",
		    (BSDTAR_FILESIZE_TYPE)archive_position_compressed(a));
	}

	archive_write_finish(a);

	/* Restore old SIGINFO + SIGUSR1 handlers. */
	siginfo_done(bsdtar);
}

/*
 * Archive names specified in file.
 *
 * Unless --null was specified, a line containing exactly "-C" will
 * cause the next line to be a directory to pass to chdir().  If
 * --null is specified, then a line "-C" is just another filename.
 */
void
archive_names_from_file(struct bsdtar *bsdtar, struct archive *a)
{
	bsdtar->archive = a;

	bsdtar->next_line_is_dir = 0;
	process_lines(bsdtar, bsdtar->names_from_file,
	    archive_names_from_file_helper);
	if (bsdtar->next_line_is_dir)
		bsdtar_errc(bsdtar, 1, errno,
		    "Unexpected end of filename list; "
		    "directory expected after -C");
}

static int
archive_names_from_file_helper(struct bsdtar *bsdtar, const char *line)
{
	if (bsdtar->next_line_is_dir) {
		set_chdir(bsdtar, line);
		bsdtar->next_line_is_dir = 0;
	} else if (!bsdtar->option_null && strcmp(line, "-C") == 0)
		bsdtar->next_line_is_dir = 1;
	else {
		if (*line != '/')
			do_chdir(bsdtar); /* Handle a deferred -C */
		write_hierarchy(bsdtar, bsdtar->archive, line);
	}
	return (0);
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
		bsdtar_warnc(bsdtar, 0, "%s", archive_error_string(ina));
		bsdtar->return_value = 1;
		return (0);
	}

	rc = append_archive(bsdtar, a, ina);

	if (archive_errno(ina)) {
		bsdtar_warnc(bsdtar, 0, "Error reading archive %s: %s",
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
		if (excluded(bsdtar, archive_entry_pathname(in_entry)))
			continue;
		if (bsdtar->option_interactive &&
		    !yes("copy '%s'", archive_entry_pathname(in_entry)))
			continue;
		if (bsdtar->verbose)
			safe_fprintf(stderr, "a %s",
			    archive_entry_pathname(in_entry));
		siginfo_setinfo(bsdtar, "copying",
		    archive_entry_pathname(in_entry),
		    archive_entry_size(in_entry));
		siginfo_printinfo(bsdtar, 0);

		e = archive_write_header(a, in_entry);
		if (e != ARCHIVE_OK) {
			if (!bsdtar->verbose)
				bsdtar_warnc(bsdtar, 0, "%s: %s",
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
			else if (copy_file_data(bsdtar, a, ina))
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
copy_file_data(struct bsdtar *bsdtar, struct archive *a, struct archive *ina)
{
	ssize_t	bytes_read;
	ssize_t	bytes_written;
	off_t	progress = 0;

	bytes_read = archive_read_data(ina, bsdtar->buff, FILEDATABUFLEN);
	while (bytes_read > 0) {
		siginfo_printinfo(bsdtar, progress);

		bytes_written = archive_write_data(a, bsdtar->buff,
		    bytes_read);
		if (bytes_written < bytes_read) {
			bsdtar_warnc(bsdtar, 0, "%s", archive_error_string(a));
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
	struct tree *tree;
	char symlink_mode = bsdtar->symlink_mode;
	dev_t first_dev = 0;
	int dev_recorded = 0;
	int tree_ret;

	tree = tree_open(path);

	if (!tree) {
		bsdtar_warnc(bsdtar, errno, "%s: Cannot open", path);
		bsdtar->return_value = 1;
		return;
	}

	while ((tree_ret = tree_next(tree))) {
		const char *name = tree_current_path(tree);
		const struct stat *st = NULL, *lst = NULL;
		int descend;

		if (tree_ret == TREE_ERROR_FATAL)
			bsdtar_errc(bsdtar, 1, tree_errno(tree),
			    "%s: Unable to continue traversing directory tree",
			    name);
		if (tree_ret == TREE_ERROR_DIR) {
			bsdtar_warnc(bsdtar, errno,
			    "%s: Couldn't visit directory", name);
			bsdtar->return_value = 1;
		}
		if (tree_ret != TREE_REGULAR)
			continue;
		lst = tree_current_lstat(tree);
		if (lst == NULL) {
			/* Couldn't lstat(); must not exist. */
			bsdtar_warnc(bsdtar, errno, "%s: Cannot stat", name);

			/*
			 * Report an error via the exit code if the failed
			 * path is a prefix of what the user provided via
			 * the command line.  (Testing for string equality
			 * here won't work due to trailing '/' characters.)
			 */
			if (memcmp(name, path, strlen(name)) == 0)
				bsdtar->return_value = 1;

			continue;
		}
		if (S_ISLNK(lst->st_mode))
			st = tree_current_stat(tree);
		/* Default: descend into any dir or symlink to dir. */
		/* We'll adjust this later on. */
		descend = 0;
		if ((st != NULL) && S_ISDIR(st->st_mode))
			descend = 1;
		if ((lst != NULL) && S_ISDIR(lst->st_mode))
			descend = 1;

		/*
		 * If user has asked us not to cross mount points,
		 * then don't descend into into a dir on a different
		 * device.
		 */
		if (!dev_recorded) {
			first_dev = lst->st_dev;
			dev_recorded = 1;
		}
		if (bsdtar->option_dont_traverse_mounts) {
			if (lst != NULL && lst->st_dev != first_dev)
				descend = 0;
		}

		/*
		 * If this file/dir is flagged "nodump" and we're
		 * honoring such flags, skip this file/dir.
		 */
#ifdef HAVE_STRUCT_STAT_ST_FLAGS
		/* BSD systems store flags in struct stat */
		if (bsdtar->option_honor_nodump &&
		    (lst->st_flags & UF_NODUMP))
			continue;
#endif

#if defined(EXT2_IOC_GETFLAGS) && defined(EXT2_NODUMP_FL)
		/* Linux uses ioctl to read flags. */
		if (bsdtar->option_honor_nodump) {
			int fd = open(name, O_RDONLY | O_NONBLOCK);
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
		 * If this file/dir is excluded by a filename
		 * pattern, skip it.
		 */
		if (excluded(bsdtar, name))
			continue;

		/*
		 * If the user vetoes this file/directory, skip it.
		 */
		if (bsdtar->option_interactive &&
		    !yes("add '%s'", name))
			continue;

		/*
		 * If this is a dir, decide whether or not to recurse.
		 */
		if (bsdtar->option_no_subdirs)
			descend = 0;

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
			/* 'L': Archive symlink to file as file. */
			lst = tree_current_stat(tree);
			/* If stat fails, we have a broken symlink;
			 * in that case, archive the link as such. */
			if (lst == NULL)
				lst = tree_current_lstat(tree);
			break;
		default:
			/* 'P': Don't descend through a symlink to dir. */
			if (!S_ISDIR(lst->st_mode))
				descend = 0;
			/* 'P': Archive symlink to file as symlink. */
			/* lst = tree_current_lstat(tree); */
			break;
		}

		if (descend)
			tree_descend(tree);

		/*
		 * Write the entry.  Note that write_entry() handles
		 * pathname editing and newness testing.
		 */
		write_entry(bsdtar, a, lst, name,
		    tree_current_access_path(tree));
	}
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
		fd = open(pathname, O_RDONLY);
		if (fd == -1) {
			if (!bsdtar->verbose)
				bsdtar_warnc(bsdtar, errno,
				    "%s: could not open file", pathname);
			else
				fprintf(stderr, ": %s", strerror(errno));
			return;
		}
	}

	e = archive_write_header(a, entry);
	if (e != ARCHIVE_OK) {
		if (!bsdtar->verbose)
			bsdtar_warnc(bsdtar, 0, "%s: %s",
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

/*
 * Add a single filesystem object to the archive.
 */
static void
write_entry(struct bsdtar *bsdtar, struct archive *a, const struct stat *st,
    const char *pathname, const char *accpath)
{
	struct archive_entry	*entry, *sparse_entry;
	static char		 linkbuffer[PATH_MAX+1];

	entry = archive_entry_new();

	archive_entry_set_pathname(entry, pathname);
	archive_entry_copy_sourcepath(entry, accpath);

	/*
	 * Rewrite the pathname to be archived.  If rewrite
	 * fails, skip the entry.
	 */
	if (edit_pathname(bsdtar, entry))
		goto abort;

	/*
	 * In -u mode, check that the file is newer than what's
	 * already in the archive; in all modes, obey --newerXXX flags.
	 */
	if (!new_enough(bsdtar, archive_entry_pathname(entry), st))
		goto abort;

	/* Display entry as we process it. This format is required by SUSv2. */
	if (bsdtar->verbose)
		safe_fprintf(stderr, "a %s", archive_entry_pathname(entry));

	/* Read symbolic link information. */
	if ((st->st_mode & S_IFMT) == S_IFLNK) {
		int lnklen;

		lnklen = readlink(accpath, linkbuffer, PATH_MAX);
		if (lnklen < 0) {
			if (!bsdtar->verbose)
				bsdtar_warnc(bsdtar, errno,
				    "%s: Couldn't read symbolic link",
				    pathname);
			else
				safe_fprintf(stderr,
				    ": Couldn't read symbolic link: %s",
				    strerror(errno));
			goto cleanup;
		}
		linkbuffer[lnklen] = 0;
		archive_entry_set_symlink(entry, linkbuffer);
	}

	/* Look up username and group name. */
	archive_entry_set_uname(entry, lookup_uname(bsdtar, st->st_uid));
	archive_entry_set_gname(entry, lookup_gname(bsdtar, st->st_gid));

#ifdef HAVE_STRUCT_STAT_ST_FLAGS
	/* XXX TODO: archive_entry_copy_stat() should copy st_flags
	 * on platforms that support it.  This should go away then. */
	if (st->st_flags != 0)
		archive_entry_set_fflags(entry, st->st_flags, 0);
#endif

#ifdef EXT2_IOC_GETFLAGS
	/* XXX TODO: Find a way to merge this with the
	 * flags fetch for nodump support earlier. */
	if ((S_ISREG(st->st_mode) || S_ISDIR(st->st_mode))) {
		int fd = open(accpath, O_RDONLY | O_NONBLOCK);
		if (fd >= 0) {
			unsigned long stflags;
			int r = ioctl(fd, EXT2_IOC_GETFLAGS, &stflags);
			close(fd);
			if (r == 0 && stflags != 0)
				archive_entry_set_fflags(entry, stflags, 0);
		}
	}
#endif

	archive_entry_copy_stat(entry, st);
	setup_acls(bsdtar, entry, accpath);
	setup_xattrs(bsdtar, entry, accpath);

	/* Non-regular files get archived with zero size. */
	if (!S_ISREG(st->st_mode))
		archive_entry_set_size(entry, 0);

	/* Record what we're doing, for the benefit of SIGINFO / SIGUSR1. */
	siginfo_setinfo(bsdtar, "adding", archive_entry_pathname(entry),
	    archive_entry_size(entry));
	archive_entry_linkify(bsdtar->resolver, &entry, &sparse_entry);

	/* Handle SIGINFO / SIGUSR1 request if one was made. */
	siginfo_printinfo(bsdtar, 0);

	while (entry != NULL) {
		write_entry_backend(bsdtar, a, entry);
		archive_entry_free(entry);
		entry = sparse_entry;
		sparse_entry = NULL;
	}

cleanup:
	if (bsdtar->verbose)
		fprintf(stderr, "\n");

abort:
	if (entry != NULL)
		archive_entry_free(entry);
}


/* Helper function to copy file to archive. */
static int
write_file_data(struct bsdtar *bsdtar, struct archive *a,
    struct archive_entry *entry, int fd)
{
	ssize_t	bytes_read;
	ssize_t	bytes_written;
	off_t	progress = 0;

	bytes_read = read(fd, bsdtar->buff, FILEDATABUFLEN);
	while (bytes_read > 0) {
		siginfo_printinfo(bsdtar, progress);

		bytes_written = archive_write_data(a, bsdtar->buff,
		    FILEDATABUFLEN);
		if (bytes_written < 0) {
			/* Write failed; this is bad */
			bsdtar_warnc(bsdtar, 0, "%s", archive_error_string(a));
			return (-1);
		}
		if (bytes_written < bytes_read) {
			/* Write was truncated; warn but continue. */
			bsdtar_warnc(bsdtar, 0,
			    "%s: Truncated write; file may have grown while being archived.",
			    archive_entry_pathname(entry));
			return (0);
		}
		progress += bytes_written;
		bytes_read = read(fd, bsdtar->buff, FILEDATABUFLEN);
	}
	return 0;
}


static void
create_cleanup(struct bsdtar *bsdtar)
{
	free_cache(bsdtar->uname_cache);
	bsdtar->uname_cache = NULL;
	free_cache(bsdtar->gname_cache);
	bsdtar->gname_cache = NULL;
}

#ifdef HAVE_POSIX_ACL
static void		setup_acl(struct bsdtar *bsdtar,
			     struct archive_entry *entry, const char *accpath,
			     int acl_type, int archive_entry_acl_type);

static void
setup_acls(struct bsdtar *bsdtar, struct archive_entry *entry,
    const char *accpath)
{
	archive_entry_acl_clear(entry);

	setup_acl(bsdtar, entry, accpath,
	    ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
	/* Only directories can have default ACLs. */
	if (S_ISDIR(archive_entry_mode(entry)))
		setup_acl(bsdtar, entry, accpath,
		    ACL_TYPE_DEFAULT, ARCHIVE_ENTRY_ACL_TYPE_DEFAULT);
}

static void
setup_acl(struct bsdtar *bsdtar, struct archive_entry *entry,
    const char *accpath, int acl_type, int archive_entry_acl_type)
{
	acl_t		 acl;
	acl_tag_t	 acl_tag;
	acl_entry_t	 acl_entry;
	acl_permset_t	 acl_permset;
	int		 s, ae_id, ae_tag, ae_perm;
	const char	*ae_name;

	/* Retrieve access ACL from file. */
	acl = acl_get_file(accpath, acl_type);
	if (acl != NULL) {
		s = acl_get_entry(acl, ACL_FIRST_ENTRY, &acl_entry);
		while (s == 1) {
			ae_id = -1;
			ae_name = NULL;

			acl_get_tag_type(acl_entry, &acl_tag);
			if (acl_tag == ACL_USER) {
				ae_id = (int)*(uid_t *)acl_get_qualifier(acl_entry);
				ae_name = lookup_uname(bsdtar, ae_id);
				ae_tag = ARCHIVE_ENTRY_ACL_USER;
			} else if (acl_tag == ACL_GROUP) {
				ae_id = (int)*(gid_t *)acl_get_qualifier(acl_entry);
				ae_name = lookup_gname(bsdtar, ae_id);
				ae_tag = ARCHIVE_ENTRY_ACL_GROUP;
			} else if (acl_tag == ACL_MASK) {
				ae_tag = ARCHIVE_ENTRY_ACL_MASK;
			} else if (acl_tag == ACL_USER_OBJ) {
				ae_tag = ARCHIVE_ENTRY_ACL_USER_OBJ;
			} else if (acl_tag == ACL_GROUP_OBJ) {
				ae_tag = ARCHIVE_ENTRY_ACL_GROUP_OBJ;
			} else if (acl_tag == ACL_OTHER) {
				ae_tag = ARCHIVE_ENTRY_ACL_OTHER;
			} else {
				/* Skip types that libarchive can't support. */
				continue;
			}

			acl_get_permset(acl_entry, &acl_permset);
			ae_perm = 0;
			/*
			 * acl_get_perm() is spelled differently on different
			 * platforms; see bsdtar_platform.h for details.
			 */
			if (ACL_GET_PERM(acl_permset, ACL_EXECUTE))
				ae_perm |= ARCHIVE_ENTRY_ACL_EXECUTE;
			if (ACL_GET_PERM(acl_permset, ACL_READ))
				ae_perm |= ARCHIVE_ENTRY_ACL_READ;
			if (ACL_GET_PERM(acl_permset, ACL_WRITE))
				ae_perm |= ARCHIVE_ENTRY_ACL_WRITE;

			archive_entry_acl_add_entry(entry,
			    archive_entry_acl_type, ae_perm, ae_tag,
			    ae_id, ae_name);

			s = acl_get_entry(acl, ACL_NEXT_ENTRY, &acl_entry);
		}
		acl_free(acl);
	}
}
#else
static void
setup_acls(struct bsdtar *bsdtar, struct archive_entry *entry,
    const char *accpath)
{
	(void)bsdtar;
	(void)entry;
	(void)accpath;
}
#endif

#if HAVE_LISTXATTR && HAVE_LLISTXATTR && HAVE_GETXATTR && HAVE_LGETXATTR

static void
setup_xattr(struct bsdtar *bsdtar, struct archive_entry *entry,
    const char *accpath, const char *name)
{
	size_t size;
	void *value = NULL;
	char symlink_mode = bsdtar->symlink_mode;

	if (symlink_mode == 'H')
		size = getxattr(accpath, name, NULL, 0);
	else
		size = lgetxattr(accpath, name, NULL, 0);

	if (size == -1) {
		bsdtar_warnc(bsdtar, errno, "Couldn't get extended attribute");
		return;
	}

	if (size > 0 && (value = malloc(size)) == NULL) {
		bsdtar_errc(bsdtar, 1, errno, "Out of memory");
		return;
	}

	if (symlink_mode == 'H')
		size = getxattr(accpath, name, value, size);
	else
		size = lgetxattr(accpath, name, value, size);

	if (size == -1) {
		bsdtar_warnc(bsdtar, errno, "Couldn't get extended attribute");
		return;
	}

	archive_entry_xattr_add_entry(entry, name, value, size);

	free(value);
}

/*
 * Linux extended attribute support
 */
static void
setup_xattrs(struct bsdtar *bsdtar, struct archive_entry *entry,
    const char *accpath)
{
	char *list, *p;
	size_t list_size;
	char symlink_mode = bsdtar->symlink_mode;

	if (symlink_mode == 'H')
		list_size = listxattr(accpath, NULL, 0);
	else
		list_size = llistxattr(accpath, NULL, 0);

	if (list_size == -1) {
		bsdtar_warnc(bsdtar, errno,
			"Couldn't list extended attributes");
		return;
	} else if (list_size == 0)
		return;

	if ((list = malloc(list_size)) == NULL) {
		bsdtar_errc(bsdtar, 1, errno, "Out of memory");
		return;
	}

	if (symlink_mode == 'H')
		list_size = listxattr(accpath, list, list_size);
	else
		list_size = llistxattr(accpath, list, list_size);

	if (list_size == -1) {
		bsdtar_warnc(bsdtar, errno,
			"Couldn't list extended attributes");
		free(list);
		return;
	}

	for (p = list; (p - list) < list_size; p += strlen(p) + 1) {
		if (strncmp(p, "system.", 7) == 0 ||
				strncmp(p, "xfsroot.", 8) == 0)
			continue;

		setup_xattr(bsdtar, entry, accpath, p);
	}

	free(list);
}

#else

/*
 * Generic (stub) extended attribute support.
 */
static void
setup_xattrs(struct bsdtar *bsdtar, struct archive_entry *entry,
    const char *accpath)
{
	(void)bsdtar; /* UNUSED */
	(void)entry; /* UNUSED */
	(void)accpath; /* UNUSED */
}

#endif

static void
free_cache(struct name_cache *cache)
{
	size_t i;

	if (cache != NULL) {
		for (i = 0; i < cache->size; i++) {
			if (cache->cache[i].name != NULL &&
			    cache->cache[i].name != NO_NAME)
				free((void *)(uintptr_t)cache->cache[i].name);
		}
		free(cache);
	}
}

/*
 * Lookup uid/gid from uname/gname, return NULL if no match.
 */
static const char *
lookup_name(struct bsdtar *bsdtar, struct name_cache **name_cache_variable,
    int (*lookup_fn)(struct bsdtar *, const char **, id_t), id_t id)
{
	struct name_cache	*cache;
	const char *name;
	int slot;


	if (*name_cache_variable == NULL) {
		*name_cache_variable = malloc(sizeof(struct name_cache));
		if (*name_cache_variable == NULL)
			bsdtar_errc(bsdtar, 1, ENOMEM, "No more memory");
		memset(*name_cache_variable, 0, sizeof(struct name_cache));
		(*name_cache_variable)->size = name_cache_size;
	}

	cache = *name_cache_variable;
	cache->probes++;

	slot = id % cache->size;
	if (cache->cache[slot].name != NULL) {
		if (cache->cache[slot].id == id) {
			cache->hits++;
			if (cache->cache[slot].name == NO_NAME)
				return (NULL);
			return (cache->cache[slot].name);
		}
		if (cache->cache[slot].name != NO_NAME)
			free((void *)(uintptr_t)cache->cache[slot].name);
		cache->cache[slot].name = NULL;
	}

	if (lookup_fn(bsdtar, &name, id) == 0) {
		if (name == NULL || name[0] == '\0') {
			/* Cache the negative response. */
			cache->cache[slot].name = NO_NAME;
			cache->cache[slot].id = id;
		} else {
			cache->cache[slot].name = strdup(name);
			if (cache->cache[slot].name != NULL) {
				cache->cache[slot].id = id;
				return (cache->cache[slot].name);
			}
			/*
			 * Conveniently, NULL marks an empty slot, so
			 * if the strdup() fails, we've just failed to
			 * cache it.  No recovery necessary.
			 */
		}
	}
	return (NULL);
}

static const char *
lookup_uname(struct bsdtar *bsdtar, uid_t uid)
{
	return (lookup_name(bsdtar, &bsdtar->uname_cache,
		    &lookup_uname_helper, (id_t)uid));
}

static int
lookup_uname_helper(struct bsdtar *bsdtar, const char **name, id_t id)
{
	struct passwd	*pwent;

	(void)bsdtar; /* UNUSED */

	errno = 0;
	pwent = getpwuid((uid_t)id);
	if (pwent == NULL) {
		*name = NULL;
		if (errno != 0)
			bsdtar_warnc(bsdtar, errno, "getpwuid(%d) failed", id);
		return (errno);
	}

	*name = pwent->pw_name;
	return (0);
}

static const char *
lookup_gname(struct bsdtar *bsdtar, gid_t gid)
{
	return (lookup_name(bsdtar, &bsdtar->gname_cache,
		    &lookup_gname_helper, (id_t)gid));
}

static int
lookup_gname_helper(struct bsdtar *bsdtar, const char **name, id_t id)
{
	struct group	*grent;

	(void)bsdtar; /* UNUSED */

	errno = 0;
	grent = getgrgid((gid_t)id);
	if (grent == NULL) {
		*name = NULL;
		if (errno != 0)
			bsdtar_warnc(bsdtar, errno, "getgrgid(%d) failed", id);
		return (errno);
	}

	*name = grent->gr_name;
	return (0);
}

/*
 * Test if the specified file is new enough to include in the archive.
 */
int
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
		bsdtar_errc(bsdtar, 1, ENOMEM, "Can't read archive directory");

	p->name = strdup(path);
	if (p->name == NULL)
		bsdtar_errc(bsdtar, 1, ENOMEM, "Can't read archive directory");
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

void
test_for_append(struct bsdtar *bsdtar)
{
	struct stat s;

	if (*bsdtar->argv == NULL && bsdtar->names_from_file == NULL)
		bsdtar_errc(bsdtar, 1, 0, "no files or directories specified");
	if (bsdtar->filename == NULL)
		bsdtar_errc(bsdtar, 1, 0, "Cannot append to stdout.");

	if (bsdtar->create_compression != 0)
		bsdtar_errc(bsdtar, 1, 0,
		    "Cannot append to %s with compression", bsdtar->filename);

	if (stat(bsdtar->filename, &s) != 0)
		return;

	if (!S_ISREG(s.st_mode) && !S_ISBLK(s.st_mode))
		bsdtar_errc(bsdtar, 1, 0,
		    "Cannot append to %s: not a regular file.",
		    bsdtar->filename);
}
