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

#include <sys/param.h>
#include <sys/stat.h>
#include <archive.h>
#include <archive_entry.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif
#include <locale.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "bsdtar.h"

static void		 long_help(void);
static void		 only_mode(char mode, const char *opt,
			     const char *valid);
static const char	*progname;
static char **		 rewrite_argv(int *argc, char ** src_argv,
			     const char *optstring);

const char *tar_opts = "b:C:cF:f:HhjkLlmnOoPprtT:UuvwXxyZz";

#ifdef HAVE_GETOPT_LONG
/*
 * These long options are deliberately not documented.  They are
 * provided only to make life easier for people using GNU tar.  The
 * only long options documented in the manual page are the ones with
 * no corresponding short option (currently, --exclude, --nodump, and
 * --fast-read).
 *
 * XXX TODO: Provide short options for --exclude, --nodump and --fast-read
 * so that bsdtar is usable on systems that do not have (or do not want
 * to use) getopt_long().
 */

#define	OPTION_EXCLUDE 1
#define	OPTION_FAST_READ 2
#define OPTION_NODUMP 3
#define OPTION_HELP 4

const struct option tar_longopts[] = {
        { "absolute-paths",     no_argument,       NULL, 'P' },
        { "append",             no_argument,       NULL, 'r' },
        { "block-size",         required_argument, NULL, 'b' },
        { "bunzip2",            no_argument,       NULL, 'j' },
        { "bzip",               no_argument,       NULL, 'j' },
        { "bzip2",              no_argument,       NULL, 'j' },
        { "cd",                 required_argument, NULL, 'C' },
        { "confirmation",       no_argument,       NULL, 'w' },
        { "create",             no_argument,       NULL, 'c' },
        { "directory",          required_argument, NULL, 'C' },
        { "exclude",            required_argument, NULL, OPTION_EXCLUDE },
        { "extract",            no_argument,       NULL, 'x' },
        { "fast-read",          no_argument,       NULL, OPTION_FAST_READ },
        { "file",               required_argument, NULL, 'f' },
        { "format",             required_argument, NULL, 'F' },
        { "gunzip",             no_argument,       NULL, 'z' },
        { "gzip",               no_argument,       NULL, 'z' },
        { "help",               no_argument,       NULL, OPTION_HELP },
        { "interactive",        no_argument,       NULL, 'w' },
        { "keep-old-files",     no_argument,       NULL, 'k' },
        { "list",               no_argument,       NULL, 't' },
        { "modification-time",  no_argument,       NULL, 'm' },
        { "nodump",             no_argument,       NULL, OPTION_NODUMP },
        { "norecurse",          no_argument,       NULL, 'n' },
        { "preserve-permissions", no_argument,     NULL, 'p' },
        { "same-permissions",   no_argument,       NULL, 'p' },
        { "to-stdout",          no_argument,       NULL, 'O' },
        { "update",             no_argument,       NULL, 'u' },
        { "verbose",            no_argument,       NULL, 'v' },
        { NULL, 0, NULL, 0 }
};
#endif

int
main(int argc, char **argv)
{
	struct bsdtar		*bsdtar, bsdtar_storage;
	struct passwd		*pwent;
	int			 opt;
	char			 mode;
	char			 buff[16];

	if (setlocale(LC_ALL, "") == NULL)
		bsdtar_warnc(0, "Failed to set default locale");
	mode = '\0';

	/*
	 * Use a pointer for consistency, but stack-allocated storage
	 * for ease of cleanup.
	 */
	bsdtar = &bsdtar_storage;
	memset(bsdtar, 0, sizeof(*bsdtar));
	bsdtar->fd = -1; /* Mark as "unused" */

	/* Look up uid/uname of current user for future reference */
	bsdtar->user_uid = geteuid();
	bsdtar->user_uname = NULL;
	if ((pwent = getpwuid(bsdtar->user_uid))) {
		bsdtar->user_uname = (char *)malloc(strlen(pwent->pw_name)+1);
		if (bsdtar->user_uname)
			strcpy(bsdtar->user_uname, pwent->pw_name);
	}

	/* Default: open tape drive. */
	bsdtar->filename = getenv("TAPE");
	if (bsdtar->filename == NULL)
		bsdtar->filename = _PATH_DEFTAPE;

	bsdtar->bytes_per_block = 10240;

	/* Default: preserve mod time on extract */
	bsdtar->extract_flags = ARCHIVE_EXTRACT_TIME;

	if (bsdtar->user_uid == 0)
		bsdtar->extract_flags = ARCHIVE_EXTRACT_OWNER;

	progname = *argv;

	/* Rewrite traditional-style tar arguments, if used. */
	argv = rewrite_argv(&argc, argv, tar_opts);

	bsdtar->argv = argv;
	bsdtar->argc = argc;

	/* Process all remaining arguments now. */
#ifdef HAVE_GETOPT_LONG
        while ((opt = getopt_long(bsdtar->argc, bsdtar->argv,
	    tar_opts, tar_longopts, NULL)) != -1) {
#else
	while ((opt = getopt(bsdtar->argc, bsdtar->argv, tar_opts)) != -1) {
#endif
		/* XXX TODO: Augment the compatibility notes below. */
		switch (opt) {
		case 'b': /* SUSv2 */
			bsdtar->bytes_per_block = 512 * atoi(optarg);
			break;
		case 'C': /* GNU tar */
			/* XXX How should multiple -C options be handled? */
			bsdtar->start_dir = optarg;
			break;
		case 'c': /* SUSv2 */
			if (mode != '\0')
				bsdtar_errc(1, 0,
				    "Can't specify both -%c and -%c",
				    opt, mode);
			mode = opt;
			break;
#ifdef HAVE_GETOPT_LONG
		case OPTION_EXCLUDE: /* GNU tar */
			exclude(bsdtar, optarg);
			break;
#endif
		case 'F':
			bsdtar->create_format = optarg;
			break;
		case 'f': /* SUSv2 */
			bsdtar->filename = optarg;
			if (strcmp(bsdtar->filename, "-") == 0)
				bsdtar->filename = NULL;
			break;
#ifdef HAVE_GETOPT_LONG
		case OPTION_FAST_READ: /* GNU tar */
			bsdtar->option_fast_read = 1;
			break;
#endif
		case 'H': /* BSD convention */
			bsdtar->symlink_mode = 'H';
			break;
		case 'h': /* Linux LSB for 'tar'; synonym for -H */
			bsdtar->symlink_mode = 'H';
			break;
		case OPTION_HELP:
			long_help();
			break;
		case 'j': /* GNU tar */
			if (bsdtar->create_compression != '\0')
				bsdtar_errc(1, 0,
				    "Can't specify both -%c and -%c", opt,
				    bsdtar->create_compression);
			bsdtar->create_compression = opt;
			break;
		case 'k': /* GNU tar */
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_NO_OVERWRITE;
			break;
		case 'L': /* BSD convention */
			bsdtar->symlink_mode = 'L';
			break;
	        case 'l': /* SUSv2 */
			bsdtar->option_warn_links = 1;
			break;
		case 'm': /* SUSv2 */
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_TIME;
			break;
		case 'n': /* GNU tar */
			bsdtar->option_no_subdirs = 1;
			break;
#ifdef HAVE_GETOPT_LONG
		case OPTION_NODUMP: /* star */
			bsdtar->option_honor_nodump = 1;
			break;
#endif
		case 'O': /* GNU tar */
			bsdtar->option_stdout = 1;
			break;
		case 'o': /* SUSv2 */
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_OWNER;
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
			bsdtar->option_absolute_paths = 1;
			break;
		case 'p': /* GNU tar, star */
			umask(0);
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_PERM;
			break;
		case 'r': /* SUSv2 */
			if (mode != '\0')
				bsdtar_errc(1, 0,
				    "Can't specify both -%c and -%c",
				    opt, mode);
			mode = opt;
			break;
		case 't': /* SUSv2 */
			if (mode != '\0')
				bsdtar_errc(1, 0,
				    "Can't specify both -%c and -%c",
				    opt, mode);
			mode = opt;
			bsdtar->verbose++;
			break;
		case 'T': /* GNU tar */
			bsdtar->names_from_file = optarg;
			break;
		case 'U': /* GNU tar */
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_UNLINK;
			break;
		case 'u': /* SUSv2 */
			if (mode != '\0')
				bsdtar_errc(1, 0,
				    "Can't specify both -%c and -%c",
				    opt, mode);
			mode = opt;
			break;
		case 'v': /* SUSv2 */
			bsdtar->verbose++;
			break;
		case 'w': /* SUSv2 */
			bsdtar->option_interactive = 1;
			break;
		case 'X': /* -l in GNU tar */
			bsdtar->option_dont_traverse_mounts = 1;
			break;
		case 'x': /* SUSv2 */
			if (mode != '\0')
				bsdtar_errc(1, 0,
				    "Can't specify both -%c and -%c",
				    opt, mode);
			mode = opt;
			break;
		case 'y': /* FreeBSD version of GNU tar */
			if (bsdtar->create_compression != '\0')
				bsdtar_errc(1, 0,
				    "Can't specify both -%c and -%c", opt,
				    bsdtar->create_compression);
			bsdtar->create_compression = opt;
			break;
		case 'Z': /* GNU tar */
			bsdtar_warnc(0, ".Z compression not supported");
			usage();
			break;
		case 'z': /* GNU tar, star */
			if (bsdtar->create_compression != '\0')
				bsdtar_errc(1, 0,
				    "Can't specify both -%c and -%c", opt,
				    bsdtar->create_compression);
			bsdtar->create_compression = opt;
			break;
		default:
			usage();
		}
	}

	/*
	 * Sanity-check options.
	 */
	if (mode == '\0')
		bsdtar_errc(1, 0, "Must specify one of -c, -r, -t, -u, -x");

	/* Check boolean options only permitted in certain modes. */
	if (bsdtar->option_absolute_paths)
		only_mode(mode, "-P", "xcru");
	if (bsdtar->option_dont_traverse_mounts)
		only_mode(mode, "-X", "cru");
	if (bsdtar->option_fast_read)
		only_mode(mode, "--fast-read", "xt");
	if (bsdtar->option_honor_nodump)
		only_mode(mode, "--nodump", "cru");
	if (bsdtar->option_no_subdirs)
		only_mode(mode, "-n", "cru");
	if (bsdtar->option_stdout)
		only_mode(mode, "-O", "x");
	if (bsdtar->option_warn_links)
		only_mode(mode, "-l", "cr");

	/* Check other parameters only permitted in certain modes. */
	if (bsdtar->create_compression != '\0') {
		strcpy(buff, "-?");
		buff[1] = bsdtar->create_compression;
		only_mode(mode, buff, "cxt");
	}
	if (bsdtar->create_format != NULL)
		only_mode(mode, "-F", "c");
	if (bsdtar->names_from_file != NULL)
		only_mode(mode, "-T", "cru");
	if (bsdtar->symlink_mode != '\0') {
		strcpy(buff, "-X");
		buff[1] = bsdtar->symlink_mode;
		only_mode(mode, buff, "cru");
	}

        bsdtar->argc -= optind;
        bsdtar->argv += optind;

	switch(mode) {
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

	if (bsdtar->user_uname != NULL)
		free(bsdtar->user_uname);

	return 0;
}

/*
 * Verify that the mode is correct.
 */
static void
only_mode(char mode, const char *opt, const char *valid_modes)
{
	if (strchr(valid_modes, mode) == NULL)
		bsdtar_errc(1, 0, "Option %s is not permitted in mode -%c",
		    opt, mode);
}


/*-
 * Convert traditional tar arguments into new-style.
 * For example,
 *     tar tvfb file.tar 32 --exclude FOO
 * must be converted to
 *     tar -t -v -f file.tar -b 32 --exclude FOO
 *
 * This requires building a new argv array.  The initial bundled word
 * gets expanded into a new string that looks like "-t\0-v\0-f\0-b\0".
 * The new argv array has pointers into this string intermingled with
 * pointers to the existing arguments.  Arguments are moved to
 * immediately follow their options.
 *
 * The optstring argument here is the same one passed to getopt(3).
 * It is used to determine which option letters have trailing arguments.
 */
char **
rewrite_argv(int *argc, char ** src_argv, const char *optstring)
{
	char **new_argv, **dest_argv;
	const char *p;
	char *src, *dest;

	if (src_argv[1] == NULL || src_argv[1][0] == '-')
		return (src_argv);

	*argc += strlen(src_argv[1]) - 1;
	new_argv = malloc((*argc + 1) * sizeof(new_argv[0]));
	if (new_argv == NULL)
		bsdtar_errc(1, errno, "No Memory");

	dest_argv = new_argv;
	*dest_argv++ = *src_argv++;

	dest = malloc(strlen(*src_argv) * 3);
	if (dest == NULL)
		bsdtar_errc(1, errno, "No memory");
	for (src = *src_argv++; *src != '\0'; src++) {
		*dest_argv++ = dest;
		*dest++ = '-';
		*dest++ = *src;
		*dest++ = '\0';
		/* If option takes an argument, insert that into the list. */
		for (p = optstring; p != NULL && *p != '\0'; p++) {
			if (*p != *src)
				continue;
			if (p[1] != ':')	/* No arg required, done. */
				break;
			if (*src_argv == NULL)	/* No arg available? Error. */
				bsdtar_errc(1, 0,
				    "Option %c requires an argument",
				    *src);
			*dest_argv++ = *src_argv++;
			break;
		}
	}

	/* Copy remaining arguments, including trailing NULL. */
	while ((*dest_argv++ = *src_argv++) != NULL)
		;

	return (new_argv);
}

void
usage(void)
{
	const char	*p;

	p = strrchr(progname, '/');
	if (p != NULL)
	    p++;
	else
	    p = progname;

	printf("Basic Usage:\n");
	printf("  List:    %s -tf [archive-filename]\n", p);
	printf("  Extract: %s -xf [archive-filename]\n", p);
	printf("  Create:  %s -cf [archive-filename] [filenames...]\n", p);
	printf("  Help:    %s -h\n", p);
	exit(1);
}

static const char *long_help_msg[] = {
	"First option must be a mode specifier:\n",
	"  -c Create  -r Add/Replace  -t List  -u Update  -x Extract\n",
	"Common Options:\n",
	"  -b #  Use # 512-byte records per I/O block\n",
	"  -f <filename>  Location of archive (default " _PATH_DEFTAPE ")\n",
	"  -v    Verbose\n",
	"  -w    Interactive\n",
	"Create: %p -c [options] [<file> | <dir> | @<archive> | C=<dir> ]\n",
	"  <file>, <dir>  add these items to archive\n",
	"  -z, -j  Compress archive with gzip/bzip2\n",
	"  -F {ustar|pax|cpio|shar}  Select archive format\n",
	"  --exclude <pattern>  Skip files that match pattern\n",
	"  C=<dir>  Change to <dir> before processing remaining files\n",
	"  @<archive>  Add entries from <archive> to output\n",
	"List: %p -t [options] [<patterns>]\n",
	"  <patterns>  If specified, list only entries that match\n",
	"Extract: %p -x [options] [<patterns>]\n",
	"  <patterns>  If specified, extract only entries that match\n",
	"  -k  	 Keep (don't overwrite) existing files\n",
	"  -m    Don't restore modification times\n",
	"  -O    Write entries to stdout, don't restore to disk\n",
	"  -p    Restore permissions (including ACLs, owner, file flags)\n",
	NULL
};


static void
long_help(void)
{
	const char	*prog;
	const char	*p;
	const char	**msg;

	prog = strrchr(progname, '/');
	if (prog != NULL)
	    prog++;
	else
	    prog = progname;

	printf("%s: manipulate archive files\n", prog);

	for (msg = long_help_msg; *msg != NULL; msg++) {
		for (p = *msg; p != NULL; p++) {
			if (*p == '\0')
				break;
			else if (*p == '%') {
				if (p[1] == 'p') {
					fputs(prog, stdout);
					p++;
				} else
					putchar('%');
			} else
				putchar(*p);
		}
	}
}

const char *
bsdtar_progname(void)
{
	return (progname);
}
