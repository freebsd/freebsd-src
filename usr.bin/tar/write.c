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

#include "bsdtar_platform.h"
__FBSDID("$FreeBSD$");

#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_POSIX_ACL
#include <sys/acl.h>
#endif
#include <archive.h>
#include <archive_entry.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux
#include <ext2fs/ext2_fs.h>
#include <sys/ioctl.h>
#endif

#include "bsdtar.h"
#include "tree.h"

/* Fixed size of uname/gname caches. */
#define	name_cache_size 101

static const char * const NO_NAME = "(noname)";

/* Initial size of link cache. */
#define	links_cache_initial_size 1024

struct archive_dir_entry {
	struct archive_dir_entry	*next;
	time_t			 mtime_sec;
	int			 mtime_nsec;
	char			*name;
};

struct archive_dir {
	struct archive_dir_entry *head, *tail;
};

struct links_cache {
	unsigned long		  number_entries;
	size_t			  number_buckets;
	struct links_entry	**buckets;
};

struct links_entry {
	struct links_entry	*next;
	struct links_entry	*previous;
	int			 links;
	dev_t			 dev;
	ino_t			 ino;
	char			*name;
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
			     const char *fname);
static void		 archive_names_from_file(struct bsdtar *bsdtar,
			     struct archive *a);
static int		 archive_names_from_file_helper(struct bsdtar *bsdtar,
			     const char *line);
static void		 create_cleanup(struct bsdtar *);
static void		 free_buckets(struct bsdtar *, struct links_cache *);
static void		 free_cache(struct name_cache *cache);
static const char *	 lookup_gname(struct bsdtar *bsdtar, gid_t gid);
static int		 lookup_gname_helper(struct bsdtar *bsdtar,
			     const char **name, id_t gid);
static void		 lookup_hardlink(struct bsdtar *,
			     struct archive_entry *entry, const struct stat *);
static const char *	 lookup_uname(struct bsdtar *bsdtar, uid_t uid);
static int		 lookup_uname_helper(struct bsdtar *bsdtar,
			     const char **name, id_t uid);
static int		 new_enough(struct bsdtar *, const char *path,
			     const struct stat *);
static void		 setup_acls(struct bsdtar *, struct archive_entry *,
			     const char *path);
static void		 test_for_append(struct bsdtar *);
static void		 write_archive(struct archive *, struct bsdtar *);
static void		 write_entry(struct bsdtar *, struct archive *,
			     const struct stat *, const char *pathname,
			     unsigned pathlen, const char *accpath);
static int		 write_file_data(struct bsdtar *, struct archive *,
			     int fd);
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

	switch (bsdtar->create_compression) {
	case 0:
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
	default:
		bsdtar_errc(bsdtar, 1, 0,
		    "Unrecognized compression option -%c",
		    bsdtar->create_compression);
	}

	r = archive_write_open_file(a, bsdtar->filename);
	if (r != ARCHIVE_OK)
		bsdtar_errc(bsdtar, 1, 0, archive_error_string(a));

	write_archive(a, bsdtar);

	if (bsdtar->option_totals) {
		fprintf(stderr, "Total bytes written: " BSDTAR_FILESIZE_PRINTF "\n",
		    (BSDTAR_FILESIZE_TYPE)archive_position_compressed(a));
	}

	archive_write_finish(a);
}

/*
 * Same as 'c', except we only support tar formats in uncompressed
 * files on disk.
 */
void
tar_mode_r(struct bsdtar *bsdtar)
{
	off_t	end_offset;
	int	format;
	struct archive *a;
	struct archive_entry *entry;

	/* Sanity-test some arguments and the file. */
	test_for_append(bsdtar);

	format = ARCHIVE_FORMAT_TAR_PAX_RESTRICTED;

	bsdtar->fd = open(bsdtar->filename, O_RDWR);
	if (bsdtar->fd < 0)
		bsdtar_errc(bsdtar, 1, errno,
		    "Cannot open %s", bsdtar->filename);

	a = archive_read_new();
	archive_read_support_compression_all(a);
	archive_read_support_format_tar(a);
	archive_read_support_format_gnutar(a);
	archive_read_open_fd(a, bsdtar->fd, 10240);
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
	 * Set format to same one auto-detected above, except use
	 * ustar for appending to GNU tar, since the library doesn't
	 * write GNU tar format.
	 */
	if (format == ARCHIVE_FORMAT_TAR_GNUTAR)
		format = ARCHIVE_FORMAT_TAR_USTAR;
	archive_write_set_format(a, format);
	lseek(bsdtar->fd, end_offset, SEEK_SET); /* XXX check return val XXX */
	archive_write_open_fd(a, bsdtar->fd); /* XXX check return val XXX */

	write_archive(a, bsdtar); /* XXX check return val XXX */

	if (bsdtar->option_totals) {
		fprintf(stderr, "Total bytes written: " BSDTAR_FILESIZE_PRINTF "\n",
		    (BSDTAR_FILESIZE_TYPE)archive_position_compressed(a));
	}

	archive_write_finish(a);
	close(bsdtar->fd);
	bsdtar->fd = -1;
}

void
tar_mode_u(struct bsdtar *bsdtar)
{
	off_t			 end_offset;
	struct archive		*a;
	struct archive_entry	*entry;
	const char		*filename;
	int			 format;
	struct archive_dir_entry	*p;
	struct archive_dir	 archive_dir;

	bsdtar->archive_dir = &archive_dir;
	memset(&archive_dir, 0, sizeof(archive_dir));

	filename = NULL;
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
	archive_read_open_fd(a, bsdtar->fd,
	    bsdtar->bytes_per_block != 0 ? bsdtar->bytes_per_block :
	    DEFAULT_BYTES_PER_BLOCK);

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

	if (bsdtar->option_totals) {
		fprintf(stderr, "Total bytes written: " BSDTAR_FILESIZE_PRINTF "\n",
		    (BSDTAR_FILESIZE_TYPE)archive_position_compressed(a));
	}

	archive_write_finish(a);
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
					return;
				}
			}
			set_chdir(bsdtar, arg);
		} else {
			if (*arg != '/' || (arg[0] == '@' && arg[1] != '/'))
				do_chdir(bsdtar); /* Handle a deferred -C */
			if (*arg == '@') {
				if (append_archive(bsdtar, a, arg + 1) != 0)
					break;
			} else
				write_hierarchy(bsdtar, a, arg);
		}
		bsdtar->argv++;
	}

	create_cleanup(bsdtar);
	archive_write_close(a);
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
 * Copy from specified archive to current archive.
 * Returns non-zero on fatal error (i.e., output errors).  Errors
 * reading the input archive set bsdtar->return_value, but this
 * function will still return zero.
 */
static int
append_archive(struct bsdtar *bsdtar, struct archive *a, const char *filename)
{
	struct archive *ina;
	struct archive_entry *in_entry;
	int bytes_read, bytes_written;
	char buff[8192];

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
		/* XXX handle/report errors XXX */
		if (archive_write_header(a, in_entry)) {
			bsdtar_warnc(bsdtar, 0, "%s",
			    archive_error_string(ina));
			bsdtar->return_value = 1;
			return (-1);
		}
		bytes_read = archive_read_data(ina, buff, sizeof(buff));
		while (bytes_read > 0) {
			bytes_written =
			    archive_write_data(a, buff, bytes_read);
			if (bytes_written < bytes_read) {
				bsdtar_warnc(bsdtar, archive_errno(a), "%s",
				    archive_error_string(a));
				return (-1);
			}
			bytes_read =
			    archive_read_data(ina, buff, sizeof(buff));
		}
		if (bsdtar->verbose)
			fprintf(stderr, "\n");

	}
	if (archive_errno(ina)) {
		bsdtar_warnc(bsdtar, 0, "Error reading archive %s: %s",
		    filename, archive_error_string(ina));
		bsdtar->return_value = 1;
	}

	return (0); /* TODO: Return non-zero on error */
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
#ifdef __linux
	int	 fd, r;
	unsigned long fflags;
#endif

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

		if (tree_ret == TREE_ERROR_DIR)
			bsdtar_warnc(bsdtar, errno, "%s: Couldn't visit directory", name);
		if (tree_ret != TREE_REGULAR)
			continue;
		lst = tree_current_lstat(tree);
		if (lst == NULL) {
			/* Couldn't lstat(); must not exist. */
			bsdtar_warnc(bsdtar, errno, "%s: Cannot stat", path);
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
#ifdef HAVE_CHFLAGS
		if (bsdtar->option_honor_nodump &&
		    (lst->st_flags & UF_NODUMP))
			continue;
#endif

#ifdef __linux
		/*
		 * Linux has a nodump flag too but to read it
		 * we have to open() the file/dir and do an ioctl on it...
		 */
		if (bsdtar->option_honor_nodump &&
		    ((fd = open(name, O_RDONLY|O_NONBLOCK)) >= 0) &&
		    ((r = ioctl(fd, EXT2_IOC_GETFLAGS, &fflags)),
			close(fd), r) >= 0 &&
		    (fflags & EXT2_NODUMP_FL))
			continue;
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
			/* 'H': First item (from command line) like 'L'. */
			lst = tree_current_stat(tree);
			/* 'H': After the first item, rest like 'P'. */
			symlink_mode = 'P';
			break;
		case 'L':
			/* 'L': Do descend through a symlink to dir. */
			/* 'L': Archive symlink to file as file. */
			lst = tree_current_stat(tree);
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
		 * In -u mode, we need to check whether this
		 * is newer than what's already in the archive.
		 * In all modes, we need to obey --newerXXX flags.
		 */
		if (new_enough(bsdtar, name, lst)) {
			write_entry(bsdtar, a, lst, name,
			    tree_current_pathlen(tree),
			    tree_current_access_path(tree));
		}
	}
	tree_close(tree);
}

/*
 * Add a single filesystem object to the archive.
 */
static void
write_entry(struct bsdtar *bsdtar, struct archive *a, const struct stat *st,
    const char *pathname, unsigned pathlen, const char *accpath)
{
	struct archive_entry	*entry;
	int			 e;
	int			 fd;
#ifdef __linux
	int			 r;
	unsigned long		 stflags;
#endif
	static char		 linkbuffer[PATH_MAX+1];

	(void)pathlen; /* UNUSED */

	fd = -1;
	entry = archive_entry_new();

	archive_entry_set_pathname(entry, pathname);

	/*
	 * Rewrite the pathname to be archived.  If rewrite
	 * fails, skip the entry.
	 */
	if (edit_pathname(bsdtar, entry))
		goto abort;

	if (!S_ISDIR(st->st_mode) && (st->st_nlink > 1))
		lookup_hardlink(bsdtar, entry, st);

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

#ifdef HAVE_CHFLAGS
	if (st->st_flags != 0)
		archive_entry_set_fflags(entry, st->st_flags, 0);
#endif

#ifdef __linux
	if ((S_ISREG(st->st_mode) || S_ISDIR(st->st_mode)) &&
	    ((fd = open(accpath, O_RDONLY|O_NONBLOCK)) >= 0) &&
	    ((r = ioctl(fd, EXT2_IOC_GETFLAGS, &stflags)), close(fd), (fd = -1), r) >= 0 &&
	    stflags) {
		archive_entry_set_fflags(entry, stflags, 0);
	}
#endif

	archive_entry_copy_stat(entry, st);
	setup_acls(bsdtar, entry, accpath);

	/*
	 * If it's a regular file (and non-zero in size) make sure we
	 * can open it before we start to write.  In particular, note
	 * that we can always archive a zero-length file, even if we
	 * can't read it.
	 */
	if (S_ISREG(st->st_mode) && st->st_size > 0) {
		fd = open(accpath, O_RDONLY);
		if (fd < 0) {
			if (!bsdtar->verbose)
				bsdtar_warnc(bsdtar, errno, "%s: could not open file", pathname);
			else
				fprintf(stderr, ": %s", strerror(errno));
			goto cleanup;
		}
	}

	/* Non-regular files get archived with zero size. */
	if (!S_ISREG(st->st_mode))
		archive_entry_set_size(entry, 0);

	e = archive_write_header(a, entry);
	if (e != ARCHIVE_OK) {
		if (!bsdtar->verbose)
			bsdtar_warnc(bsdtar, 0, "%s: %s", pathname,
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
	if (fd >= 0 && archive_entry_size(entry) > 0)
		write_file_data(bsdtar, a, fd);

cleanup:
	if (bsdtar->verbose)
		fprintf(stderr, "\n");

abort:
	if (fd >= 0)
		close(fd);

	if (entry != NULL)
		archive_entry_free(entry);
}


/* Helper function to copy file to archive, with stack-allocated buffer. */
static int
write_file_data(struct bsdtar *bsdtar, struct archive *a, int fd)
{
	char	buff[64*1024];
	ssize_t	bytes_read;
	ssize_t	bytes_written;

	/* XXX TODO: Allocate buffer on heap and store pointer to
	 * it in bsdtar structure; arrange cleanup as well. XXX */
	(void)bsdtar;

	bytes_read = read(fd, buff, sizeof(buff));
	while (bytes_read > 0) {
		bytes_written = archive_write_data(a, buff, bytes_read);
		if (bytes_written <= 0)
			return (-1); /* Write failed; this is bad */
		bytes_read = read(fd, buff, sizeof(buff));
	}
	return 0;
}


static void
create_cleanup(struct bsdtar *bsdtar)
{
	/* Free inode->pathname map used for hardlink detection. */
	if (bsdtar->links_cache != NULL) {
		free_buckets(bsdtar, bsdtar->links_cache);
		free(bsdtar->links_cache);
		bsdtar->links_cache = NULL;
	}

	free_cache(bsdtar->uname_cache);
	bsdtar->uname_cache = NULL;
	free_cache(bsdtar->gname_cache);
	bsdtar->gname_cache = NULL;
}


static void
free_buckets(struct bsdtar *bsdtar, struct links_cache *links_cache)
{
	size_t i;

	if (links_cache->buckets == NULL)
		return;

	for (i = 0; i < links_cache->number_buckets; i++) {
		while (links_cache->buckets[i] != NULL) {
			struct links_entry *lp = links_cache->buckets[i]->next;
			if (bsdtar->option_warn_links)
				bsdtar_warnc(bsdtar, 0, "Missing links to %s",
				    links_cache->buckets[i]->name);
			if (links_cache->buckets[i]->name != NULL)
				free(links_cache->buckets[i]->name);
			free(links_cache->buckets[i]);
			links_cache->buckets[i] = lp;
		}
	}
	free(links_cache->buckets);
	links_cache->buckets = NULL;
}

static void
lookup_hardlink(struct bsdtar *bsdtar, struct archive_entry *entry,
    const struct stat *st)
{
	struct links_cache	*links_cache;
	struct links_entry	*le, **new_buckets;
	int			 hash;
	size_t			 i, new_size;

	/* If necessary, initialize the links cache. */
	links_cache = bsdtar->links_cache;
	if (links_cache == NULL) {
		bsdtar->links_cache = malloc(sizeof(struct links_cache));
		if (bsdtar->links_cache == NULL)
			bsdtar_errc(bsdtar, 1, ENOMEM,
			    "No memory for hardlink detection.");
		links_cache = bsdtar->links_cache;
		memset(links_cache, 0, sizeof(struct links_cache));
		links_cache->number_buckets = links_cache_initial_size;
		links_cache->buckets = malloc(links_cache->number_buckets *
		    sizeof(links_cache->buckets[0]));
		if (links_cache->buckets == NULL) {
			bsdtar_errc(bsdtar, 1, ENOMEM,
			    "No memory for hardlink detection.");
		}
		for (i = 0; i < links_cache->number_buckets; i++)
			links_cache->buckets[i] = NULL;
	}

	/* If the links cache overflowed and got flushed, don't bother. */
	if (links_cache->buckets == NULL)
		return;

	/* If the links cache is getting too full, enlarge the hash table. */
	if (links_cache->number_entries > links_cache->number_buckets * 2)
	{
		int count;

		new_size = links_cache->number_buckets * 2;
		new_buckets = malloc(new_size * sizeof(struct links_entry *));

		count = 0;

		if (new_buckets != NULL) {
			memset(new_buckets, 0,
			    new_size * sizeof(struct links_entry *));
			for (i = 0; i < links_cache->number_buckets; i++) {
				while (links_cache->buckets[i] != NULL) {
					/* Remove entry from old bucket. */
					le = links_cache->buckets[i];
					links_cache->buckets[i] = le->next;

					/* Add entry to new bucket. */
					hash = (le->dev ^ le->ino) % new_size;

					if (new_buckets[hash] != NULL)
						new_buckets[hash]->previous =
						    le;
					le->next = new_buckets[hash];
					le->previous = NULL;
					new_buckets[hash] = le;
				}
			}
			free(links_cache->buckets);
			links_cache->buckets = new_buckets;
			links_cache->number_buckets = new_size;
		} else {
			free_buckets(bsdtar, links_cache);
			bsdtar_warnc(bsdtar, ENOMEM,
			    "No more memory for recording hard links");
			bsdtar_warnc(bsdtar, 0,
			    "Remaining links will be dumped as full files");
		}
	}

	/* Try to locate this entry in the links cache. */
	hash = ( st->st_dev ^ st->st_ino ) % links_cache->number_buckets;
	for (le = links_cache->buckets[hash]; le != NULL; le = le->next) {
		if (le->dev == st->st_dev && le->ino == st->st_ino) {
			archive_entry_copy_hardlink(entry, le->name);

			/*
			 * Decrement link count each time and release
			 * the entry if it hits zero.  This saves
			 * memory and is necessary for proper -l
			 * implementation.
			 */
			if (--le->links <= 0) {
				if (le->previous != NULL)
					le->previous->next = le->next;
				if (le->next != NULL)
					le->next->previous = le->previous;
				if (le->name != NULL)
					free(le->name);
				if (links_cache->buckets[hash] == le)
					links_cache->buckets[hash] = le->next;
				links_cache->number_entries--;
				free(le);
			}

			return;
		}
	}

	/* Add this entry to the links cache. */
	le = malloc(sizeof(struct links_entry));
	if (le != NULL)
		le->name = strdup(archive_entry_pathname(entry));
	if ((le == NULL) || (le->name == NULL)) {
		free_buckets(bsdtar, links_cache);
		bsdtar_warnc(bsdtar, ENOMEM,
		    "No more memory for recording hard links");
		bsdtar_warnc(bsdtar, 0,
		    "Remaining hard links will be dumped as full files");
		if (le != NULL)
			free(le);
		return;
	}
	if (links_cache->buckets[hash] != NULL)
		links_cache->buckets[hash]->previous = le;
	links_cache->number_entries++;
	le->next = links_cache->buckets[hash];
	le->previous = NULL;
	links_cache->buckets[hash] = le;
	le->dev = st->st_dev;
	le->ino = st->st_ino;
	le->links = st->st_nlink - 1;
}

#ifdef HAVE_POSIX_ACL
void			setup_acl(struct bsdtar *bsdtar,
			     struct archive_entry *entry, const char *accpath,
			     int acl_type, int archive_entry_acl_type);

void
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

void
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
void
setup_acls(struct bsdtar *bsdtar, struct archive_entry *entry,
    const char *accpath)
{
	(void)bsdtar;
	(void)entry;
	(void)accpath;
}
#endif

static void
free_cache(struct name_cache *cache)
{
	size_t i;

	if (cache != NULL) {
		for(i = 0; i < cache->size; i++) {
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
		/* Ignore leading './' when comparing names. */
		if (path[0] == '.' && path[1] == '/' && path[2] != '\0')
			path += 2;

		for (p = bsdtar->archive_dir->head; p != NULL; p = p->next) {
			if (strcmp(path, p->name)==0)
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

	if (path[0] == '.' && path[1] == '/' && path[2] != '\0')
		path += 2;

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

	if (*bsdtar->argv == NULL)
		bsdtar_errc(bsdtar, 1, 0, "no files or directories specified");
	if (bsdtar->filename == NULL)
		bsdtar_errc(bsdtar, 1, 0, "Cannot append to stdout.");

	if (bsdtar->create_compression != 0)
		bsdtar_errc(bsdtar, 1, 0,
		    "Cannot append to %s with compression", bsdtar->filename);

	if (stat(bsdtar->filename, &s) != 0)
		bsdtar_errc(bsdtar, 1, errno,
		    "Cannot stat %s", bsdtar->filename);

	if (!S_ISREG(s.st_mode))
		bsdtar_errc(bsdtar, 1, 0,
		    "Cannot append to %s: not a regular file.",
		    bsdtar->filename);
}
