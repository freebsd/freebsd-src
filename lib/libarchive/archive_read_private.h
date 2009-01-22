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
 * $FreeBSD$
 */

#ifndef ARCHIVE_READ_PRIVATE_H_INCLUDED
#define	ARCHIVE_READ_PRIVATE_H_INCLUDED

#include "archive.h"
#include "archive_string.h"
#include "archive_private.h"

struct archive_read;
struct archive_reader;
struct archive_read_source;

/*
 * A "reader" knows how to provide blocks.  That can include something
 * that reads blocks from disk or socket or a transformation layer
 * that reads blocks from another source and transforms them.  This
 * includes decompression and decryption filters.
 *
 * How bidding works:
 *   * The bid manager reads the first block from the current source.
 *   * It shows that block to each registered bidder.
 *   * The winning bidder is initialized (with the block and information
 *     about the source)
 *   * The winning bidder becomes the new source and the process repeats
 * This ends only when no reader provides a non-zero bid.
 */
struct archive_reader {
	/* Configuration data for the reader. */
	void *data;
	/* Bidder is handed the initial block from its source. */
	int (*bid)(struct archive_reader *, const void *buff, size_t);
	/* Init() is given the archive, upstream source, and the initial
	 * block above.  It returns a populated source structure. */
	struct archive_read_source *(*init)(struct archive_read *,
	    struct archive_reader *, struct archive_read_source *source,
	    const void *, size_t);
	/* Release the reader and any configuration data it allocated. */
	int (*free)(struct archive_reader *);
};

/*
 * A "source" is an instance of a reader.  This structure is
 * allocated and initialized by the init() method of a reader
 * above.
 */
struct archive_read_source {
	/* Essentially all sources will need these values, so
	 * just declare them here. */
	struct archive_reader *reader; /* Reader that I'm an instance of. */
	struct archive_read_source *upstream; /* Who I get blocks from. */
	struct archive_read *archive; /* associated archive. */
	/* Return next block. */
	ssize_t (*read)(struct archive_read_source *, const void **);
	/* Skip forward this many bytes. */
	int64_t (*skip)(struct archive_read_source *self, int64_t request);
	/* Close (recursively) and free(self). */
	int (*close)(struct archive_read_source *self);
	/* My private data. */
	void *data;
};

/*
 * The client source is almost the same as an internal source.
 *
 * TODO: Make archive_read_source and archive_read_client identical so
 * that users of the library can easily register their own
 * transformation filters.  This will probably break the API/ABI and
 * so should be deferred until libarchive 3.0.
 */
struct archive_read_client {
	archive_open_callback	*opener;
	archive_read_callback	*reader;
	archive_skip_callback	*skipper;
	archive_close_callback	*closer;
	void			*data;
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

	/* Registered readers. */
	struct archive_reader readers[8];

	/* Source */
	struct archive_read_source *source;

	/* File offset of beginning of most recently-read header. */
	off_t		  header_position;


	/* Used by reblocking logic. */
	char		*buffer;
	size_t		 buffer_size;
	char		*next;		/* Current read location. */
	size_t		 avail;		/* Bytes in my buffer. */
	const void	*client_buff;	/* Client buffer information. */
	size_t		 client_total;
	const char	*client_next;
	size_t		 client_avail;
	char		 end_of_file;
	char		 fatal;

	/*
	 * Format detection is mostly the same as compression
	 * detection, with one significant difference: The bidders
	 * use the read_ahead calls above to examine the stream rather
	 * than having the supervisor hand them a block of data to
	 * examine.
	 */

	struct archive_format_descriptor {
		void	 *data;
		int	(*bid)(struct archive_read *);
		int	(*read_header)(struct archive_read *, struct archive_entry *);
		int	(*read_data)(struct archive_read *, const void **, size_t *, off_t *);
		int	(*read_data_skip)(struct archive_read *);
		int	(*cleanup)(struct archive_read *);
	}	formats[8];
	struct archive_format_descriptor	*format; /* Active format. */

	/*
	 * Various information needed by archive_extract.
	 */
	struct extract		 *extract;
	int			(*cleanup_archive_extract)(struct archive_read *);
};

int	__archive_read_register_format(struct archive_read *a,
	    void *format_data,
	    int (*bid)(struct archive_read *),
	    int (*read_header)(struct archive_read *, struct archive_entry *),
	    int (*read_data)(struct archive_read *, const void **, size_t *, off_t *),
	    int (*read_data_skip)(struct archive_read *),
	    int (*cleanup)(struct archive_read *));

struct archive_reader
	*__archive_read_get_reader(struct archive_read *a);

const void
	*__archive_read_ahead(struct archive_read *, size_t, ssize_t *);
ssize_t
	__archive_read_consume(struct archive_read *, size_t);
int64_t
	__archive_read_skip(struct archive_read *, int64_t);
#endif
