/*-
 * Copyright (c) 2001 Chris D. Faulhaber
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/acl.h>
#include <sys/queue.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "setfacl.h"

static void	add_filename(const char *filename);
static void	usage(void);

static void
add_filename(const char *filename)
{
	struct sf_file *file;

	if (strlen(filename) > PATH_MAX - 1) {
		warn("illegal filename");
		return;
	}
	file = zmalloc(sizeof(struct sf_file));
	file->filename = filename;
	TAILQ_INSERT_TAIL(&filelist, file, next);
}

static void
usage(void)
{

	fprintf(stderr, "usage: setfacl [-bdhkn] [-a position entries] "
	    "[-m entries] [-M file] [-x entries] [-X file] [file ...]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	acl_t acl;
	acl_type_t acl_type;
	char filename[PATH_MAX];
	int local_error, carried_error, ch, i, entry_number, ret;
	int h_flag;
	struct sf_file *file;
	struct sf_entry *entry;
	const char *fn_dup;
	char *end;
	struct stat sb;

	acl_type = ACL_TYPE_ACCESS;
	carried_error = local_error = 0;
	h_flag = have_mask = have_stdin = n_flag = need_mask = 0;

	TAILQ_INIT(&entrylist);
	TAILQ_INIT(&filelist);

	while ((ch = getopt(argc, argv, "M:X:a:bdhkm:nx:")) != -1)
		switch(ch) {
		case 'M':
			entry = zmalloc(sizeof(struct sf_entry));
			entry->acl = get_acl_from_file(optarg);
			if (entry->acl == NULL)
				err(1, "%s: get_acl_from_file() failed", optarg);
			entry->op = OP_MERGE_ACL;
			TAILQ_INSERT_TAIL(&entrylist, entry, next);
			break;
		case 'X':
			entry = zmalloc(sizeof(struct sf_entry));
			entry->acl = get_acl_from_file(optarg);
			entry->op = OP_REMOVE_ACL;
			TAILQ_INSERT_TAIL(&entrylist, entry, next);
			break;
		case 'a':
			entry = zmalloc(sizeof(struct sf_entry));

			entry_number = strtol(optarg, &end, 10);
			if (end - optarg != (int)strlen(optarg))
				errx(1, "%s: invalid entry number", optarg);
			if (entry_number < 0)
				errx(1, "%s: entry number cannot be less than zero", optarg);
			entry->entry_number = entry_number;

			if (argv[optind] == NULL)
				errx(1, "missing ACL");
			entry->acl = acl_from_text(argv[optind]);
			if (entry->acl == NULL)
				err(1, "%s", argv[optind]);
			optind++;
			entry->op = OP_ADD_ACL;
			TAILQ_INSERT_TAIL(&entrylist, entry, next);
			break;
		case 'b':
			entry = zmalloc(sizeof(struct sf_entry));
			entry->op = OP_REMOVE_EXT;
			TAILQ_INSERT_TAIL(&entrylist, entry, next);
			break;
		case 'd':
			acl_type = ACL_TYPE_DEFAULT;
			break;
		case 'h':
			h_flag = 1;
			break;
		case 'k':
			entry = zmalloc(sizeof(struct sf_entry));
			entry->op = OP_REMOVE_DEF;
			TAILQ_INSERT_TAIL(&entrylist, entry, next);
			break;
		case 'm':
			entry = zmalloc(sizeof(struct sf_entry));
			entry->acl = acl_from_text(optarg);
			if (entry->acl == NULL)
				err(1, "%s", optarg);
			entry->op = OP_MERGE_ACL;
			TAILQ_INSERT_TAIL(&entrylist, entry, next);
			break;
		case 'n':
			n_flag++;
			break;
		case 'x':
			entry = zmalloc(sizeof(struct sf_entry));
			entry_number = strtol(optarg, &end, 10);
			if (end - optarg == (int)strlen(optarg)) {
				if (entry_number < 0)
					errx(1, "%s: entry number cannot be less than zero", optarg);
				entry->entry_number = entry_number;
				entry->op = OP_REMOVE_BY_NUMBER;
			} else {
				entry->acl = acl_from_text(optarg);
				if (entry->acl == NULL)
					err(1, "%s", optarg);
				entry->op = OP_REMOVE_ACL;
			}
			TAILQ_INSERT_TAIL(&entrylist, entry, next);
			break;
		default:
			usage();
			break;
		}
	argc -= optind;
	argv += optind;

	if (n_flag == 0 && TAILQ_EMPTY(&entrylist))
		usage();

	/* take list of files from stdin */
	if (argc == 0 || strcmp(argv[0], "-") == 0) {
		if (have_stdin)
			err(1, "cannot have more than one stdin");
		have_stdin = 1;
		bzero(&filename, sizeof(filename));
		while (fgets(filename, (int)sizeof(filename), stdin)) {
			/* remove the \n */
			filename[strlen(filename) - 1] = '\0';
			fn_dup = strdup(filename);
			if (fn_dup == NULL)
				err(1, "strdup() failed");
			add_filename(fn_dup);
		}
	} else
		for (i = 0; i < argc; i++)
			add_filename(argv[i]);

	/* cycle through each file */
	TAILQ_FOREACH(file, &filelist, next) {
		local_error = 0;

		if (stat(file->filename, &sb) == -1) {
			warn("%s: stat() failed", file->filename);
			carried_error++;
			continue;
		}

		if (acl_type == ACL_TYPE_DEFAULT && S_ISDIR(sb.st_mode) == 0) {
			warnx("%s: default ACL may only be set on a directory",
			    file->filename);
			carried_error++;
			continue;
		}

		if (h_flag)
			ret = lpathconf(file->filename, _PC_ACL_NFS4);
		else
			ret = pathconf(file->filename, _PC_ACL_NFS4);
		if (ret > 0) {
			if (acl_type == ACL_TYPE_DEFAULT) {
				warnx("%s: there are no default entries "
			           "in NFSv4 ACLs", file->filename);
				carried_error++;
				continue;
			}
			acl_type = ACL_TYPE_NFS4;
		} else if (ret == 0) {
			if (acl_type == ACL_TYPE_NFS4)
				acl_type = ACL_TYPE_ACCESS;
		} else if (ret < 0 && errno != EINVAL) {
			warn("%s: pathconf(..., _PC_ACL_NFS4) failed",
			    file->filename);
		}

		if (h_flag)
			acl = acl_get_link_np(file->filename, acl_type);
		else
			acl = acl_get_file(file->filename, acl_type);
		if (acl == NULL) {
			if (h_flag)
				warn("%s: acl_get_link_np() failed",
				    file->filename);
			else
				warn("%s: acl_get_file() failed",
				    file->filename);
			carried_error++;
			continue;
		}

		/* cycle through each option */
		TAILQ_FOREACH(entry, &entrylist, next) {
			if (local_error)
				continue;

			switch(entry->op) {
			case OP_ADD_ACL:
				local_error += add_acl(entry->acl,
				    entry->entry_number, &acl, file->filename);
				break;
			case OP_MERGE_ACL:
				local_error += merge_acl(entry->acl, &acl,
				    file->filename);
				need_mask = 1;
				break;
			case OP_REMOVE_EXT:
				remove_ext(&acl, file->filename);
				need_mask = 0;
				break;
			case OP_REMOVE_DEF:
				if (acl_type == ACL_TYPE_NFS4) {
					warnx("%s: there are no default entries in NFSv4 ACLs; "
					    "cannot remove", file->filename);
					local_error++;
					break;
				}
				if (acl_delete_def_file(file->filename) == -1) {
					warn("%s: acl_delete_def_file() failed",
					    file->filename);
					local_error++;
				}
				if (acl_type == ACL_TYPE_DEFAULT)
					local_error += remove_default(&acl,
					    file->filename);
				need_mask = 0;
				break;
			case OP_REMOVE_ACL:
				local_error += remove_acl(entry->acl, &acl,
				    file->filename);
				need_mask = 1;
				break;
			case OP_REMOVE_BY_NUMBER:
				local_error += remove_by_number(entry->entry_number,
				    &acl, file->filename);
				need_mask = 1;
				break;
			}
		}

		/* don't bother setting the ACL if something is broken */
		if (local_error) {
			carried_error++;
			continue;
		}

		if (acl_type != ACL_TYPE_NFS4 && need_mask &&
		    set_acl_mask(&acl, file->filename) == -1) {
			warnx("%s: failed to set ACL mask", file->filename);
			carried_error++;
		} else if (h_flag) {
			if (acl_set_link_np(file->filename, acl_type,
			    acl) == -1) {
				carried_error++;
				warn("%s: acl_set_link_np() failed",
				    file->filename);
			}
		} else {
			if (acl_set_file(file->filename, acl_type,
			    acl) == -1) {
				carried_error++;
				warn("%s: acl_set_file() failed",
				    file->filename);
			}
		}

		acl_free(acl);
	}

	return (carried_error);
}
