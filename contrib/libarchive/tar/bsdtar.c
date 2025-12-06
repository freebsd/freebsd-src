/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2008 Tim Kientzle
 * All rights reserved.
 */

#include "bsdtar_platform.h"

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_COPYFILE_H
#include <copyfile.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "bsdtar.h"
#include "lafe_err.h"

#if ARCHIVE_VERSION_NUMBER < 4000000 && !defined(_PATH_DEFTAPE)
// Libarchive 4.0 and later will NOT define _PATH_DEFTAPE
// but will honor it if it's set in the build.
// Until then, we'll continue to set it by default on certain platforms:
#if defined(__linux)
#define _PATH_DEFTAPE "/dev/st0"
#elif defined(_WIN32) && !defined(__CYGWIN__)
#define _PATH_DEFTAPE "\\\\.\\tape0"
#elif !defined(__APPLE__)
#define _PATH_DEFTAPE "/dev/tape"
#endif
#endif

#define _PATH_STDIO "-"

#ifdef __MINGW32__
int _CRT_glob = 0; /* Disable broken CRT globbing. */
#endif

#if defined(HAVE_SIGACTION) && (defined(SIGINFO) || defined(SIGUSR1))
static volatile int siginfo_occurred;

static void
siginfo_handler(int sig)
{
	(void)sig; /* UNUSED */
	siginfo_occurred = 1;
}

int
need_report(void)
{
	int r = siginfo_occurred;
	siginfo_occurred = 0;
	return (r);
}
#else
int
need_report(void)
{
	return (0);
}
#endif

static __LA_NORETURN void		 long_help(void);
static void		 only_mode(struct bsdtar *, const char *opt,
			     const char *valid);
static void		 set_mode(struct bsdtar *, int opt);
static __LA_NORETURN void		 version(void);

/* A basic set of security flags to request from libarchive. */
#define	SECURITY					\
	(ARCHIVE_EXTRACT_SECURE_SYMLINKS		\
	 | ARCHIVE_EXTRACT_SECURE_NODOTDOT)

static char const * const vcs_files[] = {
  /* CVS */
  "CVS", ".cvsignore",
  /* RCS */
  "RCS",
  /* SCCS */
  "SCCS",
  /* SVN */
  ".svn",
  /* git */
  ".git", ".gitignore", ".gitattributes", ".gitmodules",
  /* Arch */
  ".arch-ids", "{arch}", "=RELEASE-ID", "=meta-update", "=update",
  /* Bazaar */
  ".bzr", ".bzrignore", ".bzrtags",
  /* Mercurial */
  ".hg", ".hgignore", ".hgtags",
  /* darcs */
  "_darcs",
  NULL
};

int
main(int argc, char **argv)
{
	struct bsdtar		*bsdtar, bsdtar_storage;
	int			 opt, t;
	int			 compression, compression2;
	const char		*compression_name, *compression2_name;
	const char		*compress_program;
	char			*tptr, *uptr;
	char			 possible_help_request;
	char			 buff[16];
	long			 l;
	time_t			now;

	/*
	 * Use a pointer for consistency, but stack-allocated storage
	 * for ease of cleanup.
	 */
	bsdtar = &bsdtar_storage;
	memset(bsdtar, 0, sizeof(*bsdtar));
	bsdtar->fd = -1; /* Mark as "unused" */
	bsdtar->gid = -1;
	bsdtar->uid = -1;
	bsdtar->flags = 0;
	compression = compression2 = '\0';
	compression_name = compression2_name = NULL;
	compress_program = NULL;
	time(&now);

#if defined(HAVE_SIGACTION)
	{ /* Set up signal handling. */
		struct sigaction sa;
		sa.sa_handler = siginfo_handler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
#ifdef SIGINFO
		if (sigaction(SIGINFO, &sa, NULL))
			lafe_errc(1, errno, "sigaction(SIGINFO) failed");
#endif
#ifdef SIGUSR1
		/* ... and treat SIGUSR1 the same way as SIGINFO. */
		if (sigaction(SIGUSR1, &sa, NULL))
			lafe_errc(1, errno, "sigaction(SIGUSR1) failed");
#endif
#ifdef SIGPIPE
		/* Ignore SIGPIPE signals. */
		sa.sa_handler = SIG_IGN;
		sigaction(SIGPIPE, &sa, NULL);
#endif
#ifdef SIGCHLD
		/* Do not ignore SIGCHLD. */
		sa.sa_handler = SIG_DFL;
		sigaction(SIGCHLD, &sa, NULL);
#endif
	}
#endif

	/* Set lafe_progname before calling lafe_warnc. */
	lafe_setprogname(*argv, "bsdtar");

#if HAVE_SETLOCALE
	if (setlocale(LC_ALL, "") == NULL)
		lafe_warnc(0, "Failed to set default locale");
#endif
#if defined(HAVE_NL_LANGINFO) && defined(HAVE_D_MD_ORDER)
	bsdtar->day_first = (*nl_langinfo(D_MD_ORDER) == 'd');
#endif
	possible_help_request = 0;

	/* Look up uid of current user for future reference */
	bsdtar->user_uid = geteuid();

	/* Default: open tape drive. */
	bsdtar->filename = getenv("TAPE");
#if defined(_PATH_DEFTAPE)
	if (bsdtar->filename == NULL) {
#if defined(_WIN32) && !defined(__CYGWIN__)
		int tapeExists = !_access(_PATH_DEFTAPE, 0);
#else
		int tapeExists = !access(_PATH_DEFTAPE, F_OK);
#endif
		if (tapeExists) {
			bsdtar->filename = _PATH_DEFTAPE;
		}
	}
#endif
	if (bsdtar->filename == NULL) {
		bsdtar->filename = _PATH_STDIO;
	}

	/* Default block size settings. */
	bsdtar->bytes_per_block = DEFAULT_BYTES_PER_BLOCK;
	/* Allow library to default this unless user specifies -b. */
	bsdtar->bytes_in_last_block = -1;

	/* Default: preserve mod time on extract */
	bsdtar->extract_flags = ARCHIVE_EXTRACT_TIME;

	/* Default: Perform basic security checks. */
	bsdtar->extract_flags |= SECURITY;

#ifndef _WIN32
	/* On POSIX systems, assume --same-owner and -p when run by
	 * the root user.  This doesn't make any sense on Windows. */
	if (bsdtar->user_uid == 0) {
		/* --same-owner */
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_OWNER;
		/* -p */
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_PERM;
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_ACL;
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_XATTR;
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_FFLAGS;
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_MAC_METADATA;
	}
#endif

	/*
	 * Enable Mac OS "copyfile()" extension by default.
	 * This has no effect on other platforms.
	 */
	bsdtar->readdisk_flags |= ARCHIVE_READDISK_MAC_COPYFILE;
#ifdef COPYFILE_DISABLE_VAR
	if (getenv(COPYFILE_DISABLE_VAR))
		bsdtar->readdisk_flags &= ~ARCHIVE_READDISK_MAC_COPYFILE;
#endif
#if defined(__APPLE__)
	/*
	 * On Mac OS ACLs are archived with copyfile() (--mac-metadata)
	 * Translation to NFSv4 ACLs has to be requested explicitly with --acls
	 */
	bsdtar->readdisk_flags |= ARCHIVE_READDISK_NO_ACL;
#endif

	bsdtar->matching = archive_match_new();
	if (bsdtar->matching == NULL)
		lafe_errc(1, errno, "Out of memory");
	bsdtar->cset = cset_new();
	if (bsdtar->cset == NULL)
		lafe_errc(1, errno, "Out of memory");

	bsdtar->argv = argv;
	bsdtar->argc = argc;

	/*
	 * Comments following each option indicate where that option
	 * originated:  SUSv2, POSIX, GNU tar, star, etc.  If there's
	 * no such comment, then I don't know of anyone else who
	 * implements that option.
	 */
	while ((opt = bsdtar_getopt(bsdtar)) != -1) {
		switch (opt) {
		case 'a': /* GNU tar */
			bsdtar->flags |= OPTFLAG_AUTO_COMPRESS;
			break;
		case OPTION_ACLS: /* GNU tar */
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_ACL;
			bsdtar->readdisk_flags &= ~ARCHIVE_READDISK_NO_ACL;
			bsdtar->flags |= OPTFLAG_ACLS;
			break;
		case 'B': /* GNU tar */
			/* libarchive doesn't need this; just ignore it. */
			break;
		case 'b': /* SUSv2 */
			tptr = NULL;
			l = strtol(bsdtar->argument, &tptr, 10);
			if (l <= 0 || l > 8192L ||
			    *(bsdtar->argument) == '\0' || tptr == NULL ||
			    *tptr != '\0') {
				lafe_errc(1, 0, "Invalid or out of range "
				    "(1..8192) argument to -b");
			}
			bsdtar->bytes_per_block = 512 * (int)l;
			/* Explicit -b forces last block size. */
			bsdtar->bytes_in_last_block = bsdtar->bytes_per_block;
			break;
		case OPTION_B64ENCODE:
			if (compression2 != '\0')
				lafe_errc(1, 0,
				    "Can't specify both --uuencode and "
				    "--b64encode");
			compression2 = opt;
			compression2_name = "b64encode";
			break;
		case 'C': /* GNU tar */
			if (strlen(bsdtar->argument) == 0)
				lafe_errc(1, 0,
				    "Meaningless option: -C ''");

			set_chdir(bsdtar, bsdtar->argument);
			break;
		case 'c': /* SUSv2 */
			set_mode(bsdtar, opt);
			break;
		case OPTION_CHECK_LINKS: /* GNU tar */
			bsdtar->flags |= OPTFLAG_WARN_LINKS;
			break;
		case OPTION_CHROOT: /* NetBSD */
			bsdtar->flags |= OPTFLAG_CHROOT;
			break;
		case OPTION_CLEAR_NOCHANGE_FFLAGS:
			bsdtar->extract_flags |=
			    ARCHIVE_EXTRACT_CLEAR_NOCHANGE_FFLAGS;
			break;
		case OPTION_EXCLUDE: /* GNU tar */
			if (archive_match_exclude_pattern(
			    bsdtar->matching, bsdtar->argument) != ARCHIVE_OK)
				lafe_errc(1, 0,
				    "Couldn't exclude %s", bsdtar->argument);
			break;
		case OPTION_EXCLUDE_VCS: /* GNU tar */
			for(t=0; vcs_files[t]; t++) {
				if (archive_match_exclude_pattern(
				    bsdtar->matching,
				    vcs_files[t]) != ARCHIVE_OK)
					lafe_errc(1, 0, "Couldn't "
					    "exclude %s", vcs_files[t]);
			}
			break;
		case OPTION_FFLAGS:
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_FFLAGS;
			bsdtar->readdisk_flags &= ~ARCHIVE_READDISK_NO_FFLAGS;
			bsdtar->flags |= OPTFLAG_FFLAGS;
			break;
		case OPTION_FORMAT: /* GNU tar, others */
			cset_set_format(bsdtar->cset, bsdtar->argument);
			break;
		case 'f': /* SUSv2 */
			bsdtar->filename = bsdtar->argument;
			break;
		case OPTION_GID: /* cpio */
			tptr = NULL;
			l = strtol(bsdtar->argument, &tptr, 10);
			if (l < 0 || l >= INT_MAX || *(bsdtar->argument) == '\0' ||
			    tptr == NULL || *tptr != '\0') {
				lafe_errc(1, 0, "Invalid argument to --gid");
			}
			bsdtar->gid = (int)l;
			break;
		case OPTION_GNAME: /* cpio */
			bsdtar->gname = bsdtar->argument;
			break;
		case OPTION_GROUP: /* GNU tar */
			tptr = NULL;

			uptr = strchr(bsdtar->argument, ':');
			if (uptr != NULL) {
				if (uptr[1] == '\0') {
					lafe_errc(1, 0, "Invalid argument to --group (missing id after :)");
				}
				uptr[0] = 0;
				uptr++;
				l = strtol(uptr, &tptr, 10);
				if (l < 0 || l >= INT_MAX || *uptr == '\0' ||
				    tptr == NULL || *tptr != '\0') {
					lafe_errc(1, 0, "Invalid argument to --group (%s is not a number)", uptr);
				} else {
					bsdtar->gid = (int)l;
				}
				bsdtar->gname = bsdtar->argument;
			} else {
				l = strtol(bsdtar->argument, &tptr, 10);
				if (l < 0 || l >= INT_MAX || *(bsdtar->argument) == '\0' ||
				    tptr == NULL || *tptr != '\0') {
					bsdtar->gname = bsdtar->argument;
				} else {
					bsdtar->gid = (int)l;
					bsdtar->gname = "";
				}
			}
			break;
		case OPTION_GRZIP:
			if (compression != '\0')
				lafe_errc(1, 0,
				    "Can't specify both -%c and -%c", opt,
				    compression);
			compression = opt;
			compression_name = "grzip";
			break;
		case 'H': /* BSD convention */
			bsdtar->symlink_mode = 'H';
			break;
		case 'h': /* Linux Standards Base, gtar; synonym for -L */
			bsdtar->symlink_mode = 'L';
			/* Hack: -h by itself is the "help" command. */
			possible_help_request = 1;
			break;
		case OPTION_HELP: /* GNU tar, others */
			long_help();
			/* NOTREACHED*/
		case OPTION_HFS_COMPRESSION: /* Mac OS X v10.6 or later */
			bsdtar->extract_flags |=
			    ARCHIVE_EXTRACT_HFS_COMPRESSION_FORCED;
			break;
		case OPTION_IGNORE_ZEROS:
			bsdtar->flags |= OPTFLAG_IGNORE_ZEROS;
			break;
		case 'I': /* GNU tar */
			/*
			 * TODO: Allow 'names' to come from an archive,
			 * not just a text file.  Design a good UI for
			 * allowing names and mode/owner to be read
			 * from an archive, with contents coming from
			 * disk.  This can be used to "refresh" an
			 * archive or to design archives with special
			 * permissions without having to create those
			 * permissions on disk.
			 */
			bsdtar->names_from_file = bsdtar->argument;
			break;
		case OPTION_INCLUDE:
			/*
			 * No one else has the @archive extension, so
			 * no one else needs this to filter entries
			 * when transforming archives.
			 */
			if (archive_match_include_pattern(bsdtar->matching,
			    bsdtar->argument) != ARCHIVE_OK)
				lafe_errc(1, 0,
				    "Failed to add %s to inclusion list",
				    bsdtar->argument);
			break;
		case 'j': /* GNU tar */
			if (compression != '\0')
				lafe_errc(1, 0,
				    "Can't specify both -%c and -%c", opt,
				    compression);
			compression = opt;
			compression_name = "bzip2";
			break;
		case 'J': /* GNU tar 1.21 and later */
			if (compression != '\0')
				lafe_errc(1, 0,
				    "Can't specify both -%c and -%c", opt,
				    compression);
			compression = opt;
			compression_name = "xz";
			break;
		case 'k': /* GNU tar */
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_NO_OVERWRITE;
			break;
		case OPTION_KEEP_NEWER_FILES: /* GNU tar */
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_NO_OVERWRITE_NEWER;
			break;
		case 'L': /* BSD convention */
			bsdtar->symlink_mode = 'L';
			break;
	        case 'l': /* SUSv2 and GNU tar beginning with 1.16 */
			/* GNU tar 1.13  used -l for --one-file-system */
			bsdtar->flags |= OPTFLAG_WARN_LINKS;
			break;
		case OPTION_LRZIP:
		case OPTION_LZ4:
		case OPTION_LZIP: /* GNU tar beginning with 1.23 */
		case OPTION_LZMA: /* GNU tar beginning with 1.20 */
		case OPTION_LZOP: /* GNU tar beginning with 1.21 */
		case OPTION_ZSTD:
			if (compression != '\0')
				lafe_errc(1, 0,
				    "Can't specify both -%c and -%c", opt,
				    compression);
			compression = opt;
			switch (opt) {
			case OPTION_LRZIP: compression_name = "lrzip"; break;
			case OPTION_LZ4:  compression_name = "lz4"; break;
			case OPTION_LZIP: compression_name = "lzip"; break;
			case OPTION_LZMA: compression_name = "lzma"; break;
			case OPTION_LZOP: compression_name = "lzop"; break;
			case OPTION_ZSTD: compression_name = "zstd"; break;
			}
			break;
		case 'm': /* SUSv2 */
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_TIME;
			break;
		case OPTION_MAC_METADATA: /* Mac OS X */
			bsdtar->readdisk_flags |= ARCHIVE_READDISK_MAC_COPYFILE;
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_MAC_METADATA;
			bsdtar->flags |= OPTFLAG_MAC_METADATA;
			break;
		case 'n': /* GNU tar */
			bsdtar->flags |= OPTFLAG_NO_SUBDIRS;
			break;
	        /*
		 * Selecting files by time:
		 *    --newer-?time='date' Only files newer than 'date'
		 *    --newer-?time-than='file' Only files newer than time
		 *         on specified file (useful for incremental backups)
		 */
		case OPTION_NEWER_CTIME: /* GNU tar */
			if (archive_match_include_date(bsdtar->matching,
			    ARCHIVE_MATCH_CTIME | ARCHIVE_MATCH_NEWER,
			    bsdtar->argument) != ARCHIVE_OK)
				lafe_errc(1, 0, "Error : %s",
				    archive_error_string(bsdtar->matching));
			break;
		case OPTION_NEWER_CTIME_THAN:
			if (archive_match_include_file_time(bsdtar->matching,
			    ARCHIVE_MATCH_CTIME | ARCHIVE_MATCH_NEWER,
			    bsdtar->argument) != ARCHIVE_OK)
				lafe_errc(1, 0, "Error : %s",
				    archive_error_string(bsdtar->matching));
			break;
		case OPTION_NEWER_MTIME: /* GNU tar */
			if (archive_match_include_date(bsdtar->matching,
			    ARCHIVE_MATCH_MTIME | ARCHIVE_MATCH_NEWER,
			    bsdtar->argument) != ARCHIVE_OK)
				lafe_errc(1, 0, "Error : %s",
				    archive_error_string(bsdtar->matching));
			break;
		case OPTION_NEWER_MTIME_THAN:
			if (archive_match_include_file_time(bsdtar->matching,
			    ARCHIVE_MATCH_MTIME | ARCHIVE_MATCH_NEWER,
			    bsdtar->argument) != ARCHIVE_OK)
				lafe_errc(1, 0, "Error : %s",
				    archive_error_string(bsdtar->matching));
			break;
		case OPTION_NODUMP: /* star */
			bsdtar->readdisk_flags |= ARCHIVE_READDISK_HONOR_NODUMP;
			break;
		case OPTION_NOPRESERVE_HFS_COMPRESSION:
			/* Mac OS X v10.6 or later */
			bsdtar->extract_flags |=
			    ARCHIVE_EXTRACT_NO_HFS_COMPRESSION;
			break;
		case OPTION_NO_ACLS: /* GNU tar */
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_ACL;
			bsdtar->readdisk_flags |= ARCHIVE_READDISK_NO_ACL;
			bsdtar->flags |= OPTFLAG_NO_ACLS;
			break;
		case OPTION_NO_FFLAGS:
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_FFLAGS;
			bsdtar->readdisk_flags |= ARCHIVE_READDISK_NO_FFLAGS;
			bsdtar->flags |= OPTFLAG_NO_FFLAGS;
			break;
		case OPTION_NO_MAC_METADATA: /* Mac OS X */
			bsdtar->readdisk_flags &= ~ARCHIVE_READDISK_MAC_COPYFILE;
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_MAC_METADATA;
			bsdtar->flags |= OPTFLAG_NO_MAC_METADATA;
			break;
		case OPTION_NO_READ_SPARSE:
			bsdtar->readdisk_flags |= ARCHIVE_READDISK_NO_SPARSE;
			bsdtar->flags |= OPTFLAG_NO_READ_SPARSE;
			break;
		case OPTION_NO_SAFE_WRITES:
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_SAFE_WRITES;
			break;
		case OPTION_NO_SAME_OWNER: /* GNU tar */
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_OWNER;
			break;
		case OPTION_NO_SAME_PERMISSIONS: /* GNU tar */
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_PERM;
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_ACL;
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_XATTR;
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_FFLAGS;
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_MAC_METADATA;
			break;
		case OPTION_NO_XATTRS: /* GNU tar */
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_XATTR;
			bsdtar->readdisk_flags |= ARCHIVE_READDISK_NO_XATTR;
			bsdtar->flags |= OPTFLAG_NO_XATTRS;
			break;
		case OPTION_NULL: /* GNU tar */
			bsdtar->flags |= OPTFLAG_NULL;
			break;
		case OPTION_NUMERIC_OWNER: /* GNU tar */
			bsdtar->uname = "";
			bsdtar->gname = "";
			bsdtar->flags |= OPTFLAG_NUMERIC_OWNER;
			break;
		case 'O': /* GNU tar */
			bsdtar->flags |= OPTFLAG_STDOUT;
			break;
		case 'o': /* SUSv2 and GNU conflict here, but not fatally */
			bsdtar->flags |= OPTFLAG_O;
			break;
	        /*
		 * Selecting files by time:
		 *    --older-?time='date' Only files older than 'date'
		 *    --older-?time-than='file' Only files older than time
		 *         on specified file
		 */
		case OPTION_OLDER_CTIME:
			if (archive_match_include_date(bsdtar->matching,
			    ARCHIVE_MATCH_CTIME | ARCHIVE_MATCH_OLDER,
			    bsdtar->argument) != ARCHIVE_OK)
				lafe_errc(1, 0, "Error : %s",
				    archive_error_string(bsdtar->matching));
			break;
		case OPTION_OLDER_CTIME_THAN:
			if (archive_match_include_file_time(bsdtar->matching,
			    ARCHIVE_MATCH_CTIME | ARCHIVE_MATCH_OLDER,
			    bsdtar->argument) != ARCHIVE_OK)
				lafe_errc(1, 0, "Error : %s",
				    archive_error_string(bsdtar->matching));
			break;
		case OPTION_OLDER_MTIME:
			if (archive_match_include_date(bsdtar->matching,
			    ARCHIVE_MATCH_MTIME | ARCHIVE_MATCH_OLDER,
			    bsdtar->argument) != ARCHIVE_OK)
				lafe_errc(1, 0, "Error : %s",
				    archive_error_string(bsdtar->matching));
			break;
		case OPTION_OLDER_MTIME_THAN:
			if (archive_match_include_file_time(bsdtar->matching,
			    ARCHIVE_MATCH_MTIME | ARCHIVE_MATCH_OLDER,
			    bsdtar->argument) != ARCHIVE_OK)
				lafe_errc(1, 0, "Error : %s",
				    archive_error_string(bsdtar->matching));
			break;
		case OPTION_ONE_FILE_SYSTEM: /* GNU tar */
			bsdtar->readdisk_flags |=
			    ARCHIVE_READDISK_NO_TRAVERSE_MOUNTS;
			break;
		case OPTION_OPTIONS:
			if (bsdtar->option_options != NULL) {
				lafe_warnc(0,
				    "Ignoring previous option '%s', separate multiple options with commas",
				    bsdtar->option_options);
			}
			bsdtar->option_options = bsdtar->argument;
			break;
		case OPTION_OWNER: /* GNU tar */
			tptr = NULL;

			uptr = strchr(bsdtar->argument, ':');
			if (uptr != NULL) {
				if (uptr[1] == 0) {
					lafe_errc(1, 0, "Invalid argument to --owner (missing id after :)");
				}
				uptr[0] = 0;
				uptr++;
				l = strtol(uptr, &tptr, 10);
				if (l < 0 || l >= INT_MAX || *uptr == '\0' ||
				    tptr == NULL || *tptr != '\0') {
					lafe_errc(1, 0, "Invalid argument to --owner (%s is not a number)", uptr);
				} else {
					bsdtar->uid = (int)l;
				}
				bsdtar->uname = bsdtar->argument;
			} else {
				l = strtol(bsdtar->argument, &tptr, 10);
				if (l < 0 || l >= INT_MAX || *(bsdtar->argument) == '\0' ||
				    tptr == NULL || *tptr != '\0') {
					bsdtar->uname = bsdtar->argument;
				} else {
					bsdtar->uid = (int)l;
					bsdtar->uname = "";
				}
			}
			break;
		case OPTION_MTIME: /* GNU tar */
			bsdtar->has_mtime = 1;
			bsdtar->mtime = archive_parse_date(now, bsdtar->argument);
			if (bsdtar->mtime == (time_t)-1) {
				lafe_errc(1, 0, "Invalid argument to --mtime (bad date string)");
			}
			break;
		case OPTION_CLAMP_MTIME: /* GNU tar */
			bsdtar->clamp_mtime = 1;
			break;
#if 0
		/*
		 * The common BSD -P option is not necessary, since
		 * our default is to archive symlinks, not follow
		 * them.  This is convenient, as -P conflicts with GNU
		 * tar anyway.
		 */
		case 'P': /* BSD convention */
			/* Default behavior, no option necessary. */
			break;
#endif
		case 'P': /* GNU tar */
			bsdtar->extract_flags &= ~SECURITY;
			bsdtar->flags |= OPTFLAG_ABSOLUTE_PATHS;
			break;
		case 'p': /* GNU tar, star */
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_PERM;
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_ACL;
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_XATTR;
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_FFLAGS;
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_MAC_METADATA;
			break;
		case OPTION_PASSPHRASE:
			bsdtar->passphrase = bsdtar->argument;
			break;
		case OPTION_POSIX: /* GNU tar */
			cset_set_format(bsdtar->cset, "pax");
			break;
		case 'q': /* FreeBSD GNU tar --fast-read, NetBSD -q */
			bsdtar->flags |= OPTFLAG_FAST_READ;
			break;
		case 'r': /* SUSv2 */
			set_mode(bsdtar, opt);
			break;
		case OPTION_READ_SPARSE:
			bsdtar->readdisk_flags &= ~ARCHIVE_READDISK_NO_SPARSE;
			bsdtar->flags |= OPTFLAG_READ_SPARSE;
			break;
		case 'S': /* NetBSD pax-as-tar */
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_SPARSE;
			break;
		case 's': /* NetBSD pax-as-tar */
#if defined(HAVE_REGEX_H) || defined(HAVE_PCREPOSIX_H) || defined(HAVE_PCRE2POSIX_H)
			add_substitution(bsdtar, bsdtar->argument);
#else
			lafe_warnc(0,
			    "-s is not supported by this version of bsdtar");
			usage();
#endif
			break;
		case OPTION_SAFE_WRITES:
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_SAFE_WRITES;
			break;
		case OPTION_SAME_OWNER: /* GNU tar */
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_OWNER;
			break;
		case OPTION_STRIP_COMPONENTS: /* GNU tar 1.15 */
			tptr = NULL;
			l = strtol(bsdtar->argument, &tptr, 10);
			if (l < 0 || l > 100000L || *(bsdtar->argument) == '\0' ||
			    tptr == NULL || *tptr != '\0') {
				lafe_errc(1, 0, "Invalid argument to "
				    "--strip-components");
			}
			bsdtar->strip_components = (int)l;
			break;
		case 'T': /* GNU tar */
			if (bsdtar->names_from_file)
				lafe_errc(1, 0, "Multiple --files-from/-T options are not supported");
			bsdtar->names_from_file = bsdtar->argument;
			break;
		case 't': /* SUSv2 */
			set_mode(bsdtar, opt);
			bsdtar->verbose++;
			break;
		case OPTION_TOTALS: /* GNU tar */
			bsdtar->flags |= OPTFLAG_TOTALS;
			break;
		case 'U': /* GNU tar */
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_UNLINK;
			bsdtar->flags |= OPTFLAG_UNLINK_FIRST;
			break;
		case 'u': /* SUSv2 */
			set_mode(bsdtar, opt);
			break;
		case OPTION_UID: /* cpio */
			tptr = NULL;
			l = strtol(bsdtar->argument, &tptr, 10);
			if (l < 0 || l >= INT_MAX || *(bsdtar->argument) == '\0' ||
			    tptr == NULL || *tptr != '\0') {
				lafe_errc(1, 0, "Invalid argument to --uid");
			}
			bsdtar->uid = (int)l;
			break;
		case OPTION_UNAME: /* cpio */
			bsdtar->uname = bsdtar->argument;
			break;
		case OPTION_UUENCODE:
			if (compression2 != '\0')
				lafe_errc(1, 0,
				    "Can't specify both --uuencode and "
				    "--b64encode");
			compression2 = opt;
			compression2_name = "uuencode";
			break;
		case 'v': /* SUSv2 */
			bsdtar->verbose++;
			break;
		case OPTION_VERSION: /* GNU convention */
			version();
			/* NOTREACHED */
#if 0
		/*
		 * The -W longopt feature is handled inside of
		 * bsdtar_getopt(), so -W is not available here.
		 */
		case 'W': /* Obscure GNU convention. */
			break;
#endif
		case 'w': /* SUSv2 */
			bsdtar->flags |= OPTFLAG_INTERACTIVE;
			break;
		case 'X': /* GNU tar */
			if (archive_match_exclude_pattern_from_file(
			    bsdtar->matching, bsdtar->argument, 0)
			    != ARCHIVE_OK)
				lafe_errc(1, 0, "Error : %s",
				    archive_error_string(bsdtar->matching));
			break;
		case 'x': /* SUSv2 */
			set_mode(bsdtar, opt);
			break;
		case OPTION_XATTRS: /* GNU tar */
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_XATTR;
			bsdtar->readdisk_flags &= ~ARCHIVE_READDISK_NO_XATTR;
			bsdtar->flags |= OPTFLAG_XATTRS;
			break;
		case 'y': /* FreeBSD version of GNU tar */
			if (compression != '\0')
				lafe_errc(1, 0,
				    "Can't specify both -%c and -%c", opt,
				    compression);
			compression = opt;
			compression_name = "bzip2";
			break;
		case 'Z': /* GNU tar */
			if (compression != '\0')
				lafe_errc(1, 0,
				    "Can't specify both -%c and -%c", opt,
				    compression);
			compression = opt;
			compression_name = "compress";
			break;
		case 'z': /* GNU tar, star, many others */
			if (compression != '\0')
				lafe_errc(1, 0,
				    "Can't specify both -%c and -%c", opt,
				    compression);
			compression = opt;
			compression_name = "gzip";
			break;
		case OPTION_USE_COMPRESS_PROGRAM:
			compress_program = bsdtar->argument;
			break;
		default:
			usage();
		}
	}

	/*
	 * Sanity-check options.
	 */

	/* If no "real" mode was specified, treat -h as --help. */
	if ((bsdtar->mode == '\0') && possible_help_request) {
		long_help();
	}

	/* Otherwise, a mode is required. */
	if (bsdtar->mode == '\0')
		lafe_errc(1, 0,
		    "Must specify one of -c, -r, -t, -u, -x");

	/* Check boolean options only permitted in certain modes. */
	if (bsdtar->flags & OPTFLAG_AUTO_COMPRESS) {
		only_mode(bsdtar, "-a", "cx");
		if (bsdtar->mode == 'x') {
			bsdtar->flags &= ~OPTFLAG_AUTO_COMPRESS;
			lafe_warnc(0,
			    "Ignoring option -a in mode -x");
		}
	}
	if (bsdtar->readdisk_flags & ARCHIVE_READDISK_NO_TRAVERSE_MOUNTS)
		only_mode(bsdtar, "--one-file-system", "cru");
	if (bsdtar->flags & OPTFLAG_FAST_READ)
		only_mode(bsdtar, "--fast-read", "xt");
	if (bsdtar->extract_flags & ARCHIVE_EXTRACT_HFS_COMPRESSION_FORCED)
		only_mode(bsdtar, "--hfsCompression", "x");
	if (bsdtar->extract_flags & ARCHIVE_EXTRACT_NO_HFS_COMPRESSION)
		only_mode(bsdtar, "--nopreserveHFSCompression", "x");
	if (bsdtar->readdisk_flags & ARCHIVE_READDISK_HONOR_NODUMP)
		only_mode(bsdtar, "--nodump", "cru");
	if (bsdtar->flags & OPTFLAG_ACLS)
		only_mode(bsdtar, "--acls", "crux");
	if (bsdtar->flags & OPTFLAG_NO_ACLS)
		only_mode(bsdtar, "--no-acls", "crux");
	if (bsdtar->flags & OPTFLAG_XATTRS)
		only_mode(bsdtar, "--xattrs", "crux");
	if (bsdtar->flags & OPTFLAG_NO_XATTRS)
		only_mode(bsdtar, "--no-xattrs", "crux");
	if (bsdtar->flags & OPTFLAG_FFLAGS)
		only_mode(bsdtar, "--fflags", "crux");
	if (bsdtar->flags & OPTFLAG_NO_FFLAGS)
		only_mode(bsdtar, "--no-fflags", "crux");
	if (bsdtar->flags & OPTFLAG_MAC_METADATA)
		only_mode(bsdtar, "--mac-metadata", "crux");
	if (bsdtar->flags & OPTFLAG_NO_MAC_METADATA)
		only_mode(bsdtar, "--no-mac-metadata", "crux");
	if (bsdtar->flags & OPTFLAG_O) {
		switch (bsdtar->mode) {
		case 'c':
			/*
			 * In GNU tar, -o means "old format."  The
			 * "ustar" format is the closest thing
			 * supported by libarchive.
			 */
			cset_set_format(bsdtar->cset, "ustar");
			/* TODO: bsdtar->create_format = "v7"; */
			break;
		case 'x':
			/* POSIX-compatible behavior. */
			bsdtar->flags |= OPTFLAG_NO_OWNER;
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_OWNER;
			break;
		default:
			only_mode(bsdtar, "-o", "xc");
			break;
		}
	}
	if (bsdtar->flags & OPTFLAG_STDOUT)
		only_mode(bsdtar, "-O", "xt");
	if (bsdtar->flags & OPTFLAG_UNLINK_FIRST)
		only_mode(bsdtar, "-U", "x");
	if (bsdtar->flags & OPTFLAG_WARN_LINKS)
		only_mode(bsdtar, "--check-links", "cr");

	if ((bsdtar->flags & OPTFLAG_AUTO_COMPRESS) &&
	    cset_auto_compress(bsdtar->cset, bsdtar->filename)) {
		/* Ignore specified compressions if auto-compress works. */
		compression = '\0';
		compression2 = '\0';
	}
	/* Check other parameters only permitted in certain modes. */
	if (compress_program != NULL) {
		only_mode(bsdtar, "--use-compress-program", "cxt");
		cset_add_filter_program(bsdtar->cset, compress_program);
		/* Ignore specified compressions. */
		compression = '\0';
		compression2 = '\0';
	}
	if (compression != '\0') {
		switch (compression) {
		case 'J': case 'j': case 'y': case 'Z': case 'z':
			strcpy(buff, "-?");
			buff[1] = (char)compression;
			break;
		default:
			strcpy(buff, "--");
			strcat(buff, compression_name);
			break;
		}
		only_mode(bsdtar, buff, "cxt");
		cset_add_filter(bsdtar->cset, compression_name);
	}
	if (compression2 != '\0') {
		strcpy(buff, "--");
		strcat(buff, compression2_name);
		only_mode(bsdtar, buff, "cxt");
		cset_add_filter(bsdtar->cset, compression2_name);
	}
	if (cset_get_format(bsdtar->cset) != NULL)
		only_mode(bsdtar, "--format", "cru");
	if (bsdtar->symlink_mode != '\0') {
		strcpy(buff, "-?");
		buff[1] = bsdtar->symlink_mode;
		only_mode(bsdtar, buff, "cru");
	}

	if (!bsdtar->has_mtime && bsdtar->clamp_mtime)
		lafe_errc(1, 0,
		    "--clamp-mtime is not valid without --mtime <date>");

	/*
	 * When creating an archive from a directory tree, the directory
	 * walking code will already avoid entering directories when
	 * recursive inclusion of directory content is disabled, therefore
	 * changing the matching behavior has no effect for creation modes.
	 * It is relevant for extraction or listing.
	 */
	archive_match_set_inclusion_recursion(bsdtar->matching,
					      !(bsdtar->flags & OPTFLAG_NO_SUBDIRS));

	/* Filename "-" implies stdio. */
	if (strcmp(bsdtar->filename, "-") == 0)
		bsdtar->filename = NULL;

	switch(bsdtar->mode) {
	case 'c':
		tar_mode_c(bsdtar);
		break;
	case 'r':
		tar_mode_r(bsdtar);
		break;
	case 't':
		tar_mode_t(bsdtar);
		break;
	case 'u':
		tar_mode_u(bsdtar);
		break;
	case 'x':
		tar_mode_x(bsdtar);
		break;
	}

	archive_match_free(bsdtar->matching);
#if defined(HAVE_REGEX_H) || defined(HAVE_PCREPOSIX_H) || defined(HAVE_PCRE2POSIX_H)
	cleanup_substitution(bsdtar);
#endif
	cset_free(bsdtar->cset);
	passphrase_free(bsdtar->ppbuff);

	if (bsdtar->return_value != 0)
		lafe_warnc(0,
		    "Error exit delayed from previous errors.");
	return (bsdtar->return_value);
}

static void
set_mode(struct bsdtar *bsdtar, int opt)
{
	if (bsdtar->mode != '\0' && bsdtar->mode != opt)
		lafe_errc(1, 0,
		    "Can't specify both -%c and -%c", opt, bsdtar->mode);
	bsdtar->mode = opt;
}

/*
 * Verify that the mode is correct.
 */
static void
only_mode(struct bsdtar *bsdtar, const char *opt, const char *valid_modes)
{
	if (strchr(valid_modes, bsdtar->mode) == NULL)
		lafe_errc(1, 0,
		    "Option %s is not permitted in mode -%c",
		    opt, bsdtar->mode);
}


void
usage(void)
{
	const char	*p;

	p = lafe_getprogname();

	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  List:    %s -tf <archive-filename>\n", p);
	fprintf(stderr, "  Extract: %s -xf <archive-filename>\n", p);
	fprintf(stderr, "  Create:  %s -cf <archive-filename> [filenames...]\n", p);
	fprintf(stderr, "  Help:    %s --help\n", p);
	exit(1);
}

static void
version(void)
{
	printf("bsdtar %s - %s \n",
	    BSDTAR_VERSION_STRING,
	    archive_version_details());
	exit(0);
}

static const char *long_help_msg =
	"First option must be a mode specifier:\n"
	"  -c Create  -r Add/Replace  -t List  -u Update  -x Extract\n"
	"Common Options:\n"
	"  -b #  Use # 512-byte records per I/O block\n"
	"  -f <filename>  Location of archive (default " _PATH_DEFTAPE ")\n"
	"  -v    Verbose\n"
	"  -w    Interactive\n"
	"Create: %p -c [options] [<file> | <dir> | @<archive> | -C <dir> ]\n"
	"  <file>, <dir>  add these items to archive\n"
	"  -z, -j, -J, --lzma  Compress archive with gzip/bzip2/xz/lzma\n"
	"  --format {ustar|pax|cpio|shar}  Select archive format\n"
	"  --exclude <pattern>  Skip files that match pattern\n"
	"  --mtime <date>  Set modification times for added files\n"
	"  --clamp-mtime   Only set modification times for files newer than --mtime\n"
	"  -C <dir>  Change to <dir> before processing remaining files\n"
	"  @<archive>  Add entries from <archive> to output\n"
	"List: %p -t [options] [<patterns>]\n"
	"  <patterns>  If specified, list only entries that match\n"
	"Extract: %p -x [options] [<patterns>]\n"
	"  <patterns>  If specified, extract only entries that match\n"
	"  -k    Keep (don't overwrite) existing files\n"
	"  -m    Don't restore modification times\n"
	"  -O    Write entries to stdout, don't restore to disk\n"
	"  -p    Restore permissions (including ACLs, owner, file flags)\n";


/*
 * Note that the word 'bsdtar' will always appear in the first line
 * of output.
 *
 * In particular, /bin/sh scripts that need to test for the presence
 * of bsdtar can use the following template:
 *
 * if (tar --help 2>&1 | grep bsdtar >/dev/null 2>&1 ) then \
 *          echo bsdtar; else echo not bsdtar; fi
 */
static void
long_help(void)
{
	const char	*prog;
	const char	*p;

	prog = lafe_getprogname();

	fflush(stderr);

	p = (strcmp(prog,"bsdtar") != 0) ? "(bsdtar)" : "";
	printf("%s%s: manipulate archive files\n", prog, p);

	for (p = long_help_msg; *p != '\0'; p++) {
		if (*p == '%') {
			if (p[1] == 'p') {
				fputs(prog, stdout);
				p++;
			} else
				putchar('%');
		} else
			putchar(*p);
	}
	version();
}
