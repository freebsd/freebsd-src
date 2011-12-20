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
 *
 * $FreeBSD: head/lib/libarchive/archive_read_private.h 201088 2009-12-28 02:18:55Z kientzle $
 */

#ifndef __LIBARCHIVE_BUILD
#error This header is only to be used internally to libarchive.
#endif

#ifndef ARCHIVE_READ_PRIVATE_H_INCLUDED
#define	ARCHIVE_READ_PRIVATE_H_INCLUDED

#include "archive.h"
#include "archive_string.h"
#include "archive_private.h"

struct archive_read;
struct archive_read_filter_bidder;
struct archive_read_filter;

/*
 * How bidding works for filters:
 *   * The bid manager reads the first block from the current source.
 *   * It shows that block to each registered bidder.
 *   * The bid manager creates a new filter structure for the winning
 *     bidder and gives the winning bidder a chance to initialize it.
 *   * The new filter becomes the top filter in the archive_read structure
 *     and we repeat the process.
 * This ends only when no bidder provides a non-zero bid.
 */
struct archive_read_filter_bidder {
	/* Configuration data for the bidder. */
	void *data;
	/* Taste the upstream filter to see if we handle this. */
	int (*bid)(struct archive_read_filter_bidder *,
	    struct archive_read_filter *);
	/* Initialize a newly-created filter. */
	int (*init)(struct archive_read_filter *);
	/* Set an option for the filter bidder. */
	int (*options)(struct archive_read_filter_bidder *,
	    const char *key, const char *value);
	/* Release the bidder's configuration data. */
	int (*free)(struct archive_read_filter_bidder *);
};

/*
 * This structure is allocated within the archive_read core
 * and initialized by archive_read and the init() method of the
 * corresponding bidder above.
 */
struct archive_read_filter {
	/* Essentially all filters will need these values, so
	 * just declare them here. */
	struct archive_read_filter_bidder *bidder; /* My bidder. */
	struct archive_read_filter *upstream; /* Who I read from. */
	struct archive_read *archive; /* Associated archive. */
	/* Return next block. */
	ssize_t (*read)(struct archive_read_filter *, const void **);
	/* Skip forward this many bytes. */
	int64_t (*skip)(struct archive_read_filter *self, int64_t request);
	/* Close (just this filter) and free(self). */
	int (*close)(struct archive_read_filter *self);
	/* My private data. */
	void *data;

	const char	*name;
	int		 code;

	/* Used by reblocking logic. */
	char		*buffer;
	size_t		 buffer_size;
	char		*next;		/* Current read location. */
	size_t		 avail;		/* Bytes in my buffer. */
	const void	*client_buff;	/* Client buffer information. */
	size_t		 client_total;
	const char	*client_next;
	size_t		 client_avail;
	int64_t		 position;
	char		 end_of_file;
	char		 fatal;
};

/*
 * The client looks a lot like a filter, so we just wrap it here.
 *
 * TODO: Make archive_read_filter and archive_read_client identical so
 * that users of the library can easily register their own
 * transformation filters.  This will probably break the API/ABI and
 * so should be deferred at least until libarchive 3.0.
 */
struct archive_read_client {
	archive_read_callback	*reader;
	archive_skip_callback	*skipper;
	archive_close_callback	*closer;
};

struct archive_read {
	struct archive	archive;

	struct archive_entry	*entry;

	/* Dev/ino of the archive being read/written. */
	dev_t		  skip_file_dev;
	ino_t		  skip_file_ino;

	/*
	 * Used by archive_read_data() to track blocks and copy
	 * data to client buffers, filling gaps with zero bytes.
	 */
	const char	 *read_data_block;
	off_t		  read_data_offset;
	off_t		  read_data_output_offset;
	size_t		  read_data_remaining;

	/* Callbacks to open/read/write/close client archive stream. */
	struct archive_read_client client;

	/* Registered filter bidders. */
	struct archive_read_filter_bidder bidders[8];

	/* Last filter in chain */
	struct archive_read_filter *filter;

	/* File offset of beginning of most recently-read header. */
	off_t		  header_position;

	/*
	 * Format detection is mostly the same as compression
	 * detection, with one significant difference: The bidders
	 * use the read_ahead calls above to examine the stream rather
	 * than having the supervisor hand them a block of data to
	 * examine.
	 */

	struct archive_format_descriptor {
		void	 *data;
		const char *name;
		int	(*bid)(struct archive_read *);
		int	(*options)(struct archive_read *, const char *key,
		    const char *value);
		int	(*read_header)(struct archive_read *, struct archive_entry *);
		int	(*read_data)(struct archive_read *, const void **, size_t *, off_t *);
		int	(*read_data_skip)(struct archive_read *);
		int	(*cleanup)(struct archive_read *);
	}	formats[9];
	struct archive_format_descriptor	*format; /* Active format. */

	/*
	 * Various information needed by archive_extract.
	 */
	struct extract		 *extract;
	int			(*cleanup_archive_extract)(struct archive_read *);
};

int	__archive_read_register_format(struct archive_read *a,
	    void *format_data,
	    const char *name,
	    int (*bid)(struct archive_read *),
	    int (*options)(struct archive_read *, const char *, const char *),
	    int (*read_header)(struct archive_read *, struct archive_entry *),
	    int (*read_data)(struct archive_read *, const void **, size_t *, off_t *),
	    int (*read_data_skip)(struct archive_read *),
	    int (*cleanup)(struct archive_read *));

struct archive_read_filter_bidder
	*__archive_read_get_bidder(struct archive_read *a);

const void *__archive_read_ahead(struct archive_read *, size_t, ssize_t *);
const void *__archive_read_filter_ahead(struct archive_read_filter *,
    size_t, ssize_t *);
ssize_t	__archive_read_consume(struct archive_read *, size_t);
ssize_t	__archive_read_filter_consume(struct archive_read_filter *, size_t);
int64_t	__archive_read_skip(struct archive_read *, int64_t);
int64_t	__archive_read_skip_lenient(struct archive_read *, int64_t);
int64_t	__archive_read_filter_skip(struct archive_read_filter *, int64_t);
int __archive_read_program(struct archive_read_filter *, const char *);
#endif
