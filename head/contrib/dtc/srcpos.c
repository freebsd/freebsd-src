/*
 * Copyright 2007 Jon Loeliger, Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *                                                                   USA
 */

#define _GNU_SOURCE

#include <stdio.h>

#include "dtc.h"
#include "srcpos.h"


/*
 * Like yylineno, this is the current open file pos.
 */
struct dtc_file *srcpos_file;

/*
 * The empty source position.
 */

struct dtc_file dtc_empty_file = {
	.dir = NULL,
	.name = "<no file>",
	.file = NULL
};

srcpos srcpos_empty = {
	.first_line = 0,
	.first_column = 0,
	.last_line = 0,
	.last_column = 0,
	.file = &dtc_empty_file
};


static int
dtc_open_one(struct dtc_file *file, const char *search, const char *fname)
{
	char *fullname;

	if (search) {
		fullname = xmalloc(strlen(search) + strlen(fname) + 2);

		strcpy(fullname, search);
		strcat(fullname, "/");
		strcat(fullname, fname);
	} else {
		fullname = xstrdup(fname);
	}

	file->file = fopen(fullname, "r");
	if (!file->file) {
		free(fullname);
		return 0;
	}

	file->name = fullname;
	return 1;
}


struct dtc_file *
dtc_open_file(const char *fname, const struct search_path *search)
{
	static const struct search_path default_search = { NULL, NULL, NULL };

	struct dtc_file *file;
	const char *slash;

	file = xmalloc(sizeof(struct dtc_file));

	slash = strrchr(fname, '/');
	if (slash) {
		char *dir = xmalloc(slash - fname + 1);

		memcpy(dir, fname, slash - fname);
		dir[slash - fname] = 0;
		file->dir = dir;
	} else {
		file->dir = NULL;
	}

	if (streq(fname, "-")) {
		file->name = "stdin";
		file->file = stdin;
		return file;
	}

	if (fname[0] == '/') {
		file->file = fopen(fname, "r");
		if (!file->file)
			goto fail;

		file->name = xstrdup(fname);
		return file;
	}

	if (!search)
		search = &default_search;

	while (search) {
		if (dtc_open_one(file, search->dir, fname))
			return file;

		if (errno != ENOENT)
			goto fail;

		search = search->next;
	}

fail:
	die("Couldn't open \"%s\": %s\n", fname, strerror(errno));
}


void
dtc_close_file(struct dtc_file *file)
{
	if (fclose(file->file))
		die("Error closing \"%s\": %s\n", file->name, strerror(errno));
}


srcpos *
srcpos_copy(srcpos *pos)
{
	srcpos *pos_new;

	pos_new = xmalloc(sizeof(srcpos));
	memcpy(pos_new, pos, sizeof(srcpos));

	return pos_new;
}



void
srcpos_dump(srcpos *pos)
{
	printf("file        : \"%s\"\n",
	       pos->file ? (char *) pos->file : "<no file>");
	printf("first_line  : %d\n", pos->first_line);
	printf("first_column: %d\n", pos->first_column);
	printf("last_line   : %d\n", pos->last_line);
	printf("last_column : %d\n", pos->last_column);
	printf("file        : %s\n", pos->file->name);
}


char *
srcpos_string(srcpos *pos)
{
	const char *fname;
	char col_buf[100];
	char *pos_str;

	if (!pos) {
		fname = "<no-file>";
	} else if (pos->file->name) {
		fname = pos->file->name;
		if (strcmp(fname, "-") == 0)
			fname = "stdin";
	} else {
		fname = "<no-file>";
	}

	if (pos->first_line == pos->last_line) {
		if (pos->first_column == pos->last_column) {
			snprintf(col_buf, sizeof(col_buf),
				 "%d:%d",
				 pos->first_line, pos->first_column);
		} else {
			snprintf(col_buf, sizeof(col_buf),
				 "%d:%d-%d",
				 pos->first_line,
				 pos->first_column, pos->last_column);
		}

	} else {
		snprintf(col_buf, sizeof(col_buf),
			 "%d:%d - %d:%d",
			 pos->first_line, pos->first_column,
			 pos->last_line, pos->last_column);
	}

	if (asprintf(&pos_str, "%s %s", fname, col_buf) == -1)
		return "<unknown source position?";

	return pos_str;
}


void
srcpos_error(srcpos *pos, char const *fmt, ...)
{
	const char *srcstr;
	va_list va;
	va_start(va, fmt);

	srcstr = srcpos_string(pos);

	fprintf(stderr, "Error: %s ", srcstr);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");

	va_end(va);
}


void
srcpos_warn(srcpos *pos, char const *fmt, ...)
{
	const char *srcstr;
	va_list va;
	va_start(va, fmt);

	srcstr = srcpos_string(pos);

	fprintf(stderr, "Warning: %s ", srcstr);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");

	va_end(va);
}
