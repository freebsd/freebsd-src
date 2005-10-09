/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef LOGGING_P_H
#define LOGGING_P_H

typedef struct log_file_desc {
	char *name;
	size_t name_size;
	FILE *stream;
	unsigned int versions;
	unsigned long max_size;
	uid_t owner;
	gid_t group;
} log_file_desc;

typedef union log_output {
	int facility;
	log_file_desc file;
} log_output;

struct log_channel {
	int level;			/* don't log messages > level */
	log_channel_type type;
	log_output out;
	unsigned int flags;
	int references;
};

typedef struct log_channel_list {
	log_channel channel;
	struct log_channel_list *next;
} *log_channel_list;

#define LOG_BUFFER_SIZE 20480

struct log_context {
	int num_categories;
	char **category_names;
	log_channel_list *categories;
	int flags;
	int level;
	char buffer[LOG_BUFFER_SIZE];
};

#endif /* !LOGGING_P_H */
