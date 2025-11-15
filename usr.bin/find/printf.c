/*-
 * Copyright (c) 2023, Netflix, Inc
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "find.h"

/* translate \X to proper escape, or to itself if no special meaning */
static const char *esc = "\a\bcde\fghijklm\nopq\rs\tu\v";

static inline bool
isoct(char c)
{
	return (c >= '0' && c <= '7');
}

static inline bool
isesc(char c)
{
	return (c >= 'a' && c <= 'v' && esc[c - 'a'] != c);
}

static char *
escape(const char *str, bool *flush, bool *warned)
{
	char c;
	int value;
	char *tmpstr;
	size_t tmplen;
	FILE *fp;

	fp = open_memstream(&tmpstr, &tmplen);

	/*
	 * Copy the str string into a new struct sbuf and return that expanding
	 * the different ANSI escape sequences.
	 */
	*flush = false;
	for (c = *str++; c; c = *str++) {
		if (c != '\\') {
			putc(c, fp);
			continue;
		}
		c = *str++;

		/*
		 * User error \ at end of string
		 */
		if (c == '\0') {
			putc('\\', fp);
			break;
		}

		/*
		 * \c terminates output now and is supposed to flush the output
		 * too...
		 */
		if (c == 'c') {
			*flush = true;
			break;
		}

		/*
		 * Is it octal? If so, decode up to 3 octal characters.
		 */
		if (isoct(c)) {
			value = 0;
			for (int i = 3; i-- > 0 && isoct(c);
			     c = *str++) {
				value <<= 3;
				value += c - '0';
			}
			str--;
			putc((char)value, fp);
			continue;
		}

		/*
		 * It's an ANSI X3.159-1989 escape, use the mini-escape lookup
		 * table to translate.
		 */
		if (isesc(c)) {
			putc(esc[c - 'a'], fp);
			continue;
		}

		/*
		 * Otherwise, it's self inserting. gnu find specifically says
		 * not to rely on this behavior though. gnu find will issue
		 * a warning here, while printf(1) won't.
		 */
		if (!*warned) {
			warn("Unknown character %c after \\.", c);
			*warned = true;
		}
		putc(c, fp);
	}
	fclose(fp);

	return (tmpstr);
}

static void
fp_ctime(FILE *fp, time_t t)
{
	char s[26];

	ctime_r(&t, s);
	s[24] = '\0';	/* kill newline, though gnu find info silent on issue */
	fputs(s, fp);
}

/*
 * Assumes all times are displayed in UTC rather than local time, gnu find info
 * page silent on the issue.
 *
 * Also assumes that gnu find doesn't support multiple character escape sequences,
 * which it's info page is also silent on.
 */
static void
fp_strftime(FILE *fp, time_t t, char mod)
{
	struct tm tm;
	char buffer[128];
	char fmt[3] = "% ";

	/*
	 * Gnu libc extension we don't yet support -- seconds since epoch
	 * Used in Linux kernel build, so we kinda have to support it here
	 */
	if (mod == '@')	{
		fprintf(fp, "%ju", (uintmax_t)t);
		return;
	}

	gmtime_r(&t, &tm);
	fmt[1] = mod;
	if (strftime(buffer, sizeof(buffer), fmt, &tm) == 0)
		errx(1, "Format bad or data too long for buffer"); /* Can't really happen ??? */
	fputs(buffer, fp);
}

void
do_printf(PLAN *plan, FTSENT *entry, FILE *fout)
{
	char buf[4096];
	struct stat sb;
	struct stat *sp;
	const char *path, *pend;
	char *all, *fmt;
	ssize_t ret;
	int c;
	bool flush, warned;

	warned = (plan->flags & F_HAS_WARNED) != 0;
	all = fmt = escape(plan->c_data, &flush, &warned);
	if (warned)
		plan->flags |= F_HAS_WARNED;
	for (c = *fmt++; c; c = *fmt++) {
		sp = entry->fts_statp;
		if (c != '%') {
			putc(c, fout);
			continue;
		}
		c = *fmt++;
		/* Style(9) deviation: case order same as gnu find info doc */
		switch (c) {
		case '%':
			putc(c, fout);
			break;
		case 'p': /* Path to file */
			fputs(entry->fts_path, fout);
			break;
		case 'f': /* filename w/o dirs */
			fputs(entry->fts_name, fout);
			break;
		case 'h':
			/*
			 * path, relative to the starting point, of the file, or
			 * '.' if that's empty for some reason.
			 */
			path = entry->fts_path;
			pend = strrchr(path, '/');
			if (pend == NULL)
				putc('.', fout);
			else
				fwrite(path, pend - path, 1, fout);
			break;
		case 'P': /* file with command line arg rm'd -- HOW? fts_parent? */
			errx(1, "%%%c is unimplemented", c);
		case 'H': /* Command line arg -- HOW? */
			errx(1, "%%%c is unimplemented", c);
		case 'g': /* gid human readable */
			fputs(group_from_gid(sp->st_gid, 0), fout);
			break;
		case 'G': /* gid numeric */
			fprintf(fout, "%d", sp->st_gid);
			break;
		case 'u': /* uid human readable */
			fputs(user_from_uid(sp->st_uid, 0), fout);
			break;
		case 'U': /* uid numeric */
			fprintf(fout, "%d", sp->st_uid);
			break;
		case 'm': /* mode in octal */
			fprintf(fout, "%o", sp->st_mode & 07777);
			break;
		case 'M': /* Mode in ls-standard form */
			strmode(sp->st_mode, buf);
			fwrite(buf, 10, 1, fout);
			break;
		case 'k': /* kbytes used by file */
			fprintf(fout, "%jd", (intmax_t)sp->st_blocks / 2);
			break;
		case 'b': /* blocks used by file */
			fprintf(fout, "%jd", (intmax_t)sp->st_blocks);
			break;
		case 's': /* size in bytes of file */
			fprintf(fout, "%ju", (uintmax_t)sp->st_size);
			break;
		case 'S': /* sparseness of file */
			fprintf(fout, "%3.1f",
			    (float)sp->st_blocks * 512 / (float)sp->st_size);
			break;
		case 'd': /* Depth in tree */
			fprintf(fout, "%ld", entry->fts_level);
			break;
		case 'D': /* device number */
			fprintf(fout, "%ju", (uintmax_t)sp->st_dev);
			break;
		case 'F': /* Filesystem type */
			errx(1, "%%%c is unimplemented", c);
		case 'l': /* object of symbolic link */
			ret = readlink(entry->fts_accpath, buf, sizeof(buf));
			if (ret > 0)
				fwrite(buf, ret, 1, fout);
			break;
		case 'i': /* inode # */
			fprintf(fout, "%ju", (uintmax_t)sp->st_ino);
			break;
		case 'n': /* number of hard links */
			fprintf(fout, "%ju", (uintmax_t)sp->st_nlink);
			break;
		case 'Y': /* -type of file, following 'l' types L loop ? error */
			if (S_ISLNK(sp->st_mode)) {
				if (stat(entry->fts_accpath, &sb) != 0) {
					switch (errno) {
					case ELOOP:
						putc('L', fout);
						break;
					case ENOENT:
						putc('N', fout);
						break;
					default:
						putc('?', fout);
						break;
					}
					break;
				}
				sp = &sb;
			}
			/* FALLTHROUGH */
		case 'y': /* -type of file, incl 'l' */
			switch (sp->st_mode & S_IFMT) {
			case S_IFIFO:
				putc('p', fout);
				break;
			case S_IFCHR:
				putc('c', fout);
				break;
			case S_IFDIR:
				putc('d', fout);
				break;
			case S_IFBLK:
				putc('b', fout);
				break;
			case S_IFREG:
				putc('f', fout);
				break;
			case S_IFLNK:
				putc('l', fout);
				break;
			case S_IFSOCK:
				putc('s', fout);
				break;
			case S_IFWHT:
				putc('w', fout);
				break;
			default:
				putc('U', fout);
				break;
			}
			break;
		case 'a': /* access time ctime */
			fp_ctime(fout, sp->st_atime);
			break;
		case 'A': /* access time with next char strftime format */
			fp_strftime(fout, sp->st_atime, *fmt++);
			break;
		case 'B': /* birth time with next char strftime format */
#ifdef HAVE_STRUCT_STAT_ST_BIRTHTIME
			if (sp->st_birthtime != 0)
				fp_strftime(fout, sp->st_birthtime, *fmt);
#endif
			fmt++;
			break;	/* blank on systems that don't support it */
		case 'c': /* status change time ctime */
			fp_ctime(fout, sp->st_ctime);
			break;
		case 'C': /* status change time with next char strftime format */
			fp_strftime(fout, sp->st_ctime, *fmt++);
			break;
		case 't': /* modification change time ctime */
			fp_ctime(fout, sp->st_mtime);
			break;
		case 'T': /* modification time with next char strftime format */
			fp_strftime(fout, sp->st_mtime, *fmt++);
			break;
		case 'Z': /* empty string for compat SELinux context string */
			break;
		/* Modifier parsing here, but also need to modify above somehow */
		case '#': case '-': case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9': case '.':
			errx(1, "Format modifier %c not yet supported: '%s'", c, all);
		/* Any FeeeBSD-specific modifications here -- none yet */
		default:
			errx(1, "Unknown format %c '%s'", c, all);
		}
	}
	if (flush)
		fflush(fout);
	free(all);
}
