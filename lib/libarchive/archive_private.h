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
 *
 * $FreeBSD$
 */

#ifndef ARCHIVE_PRIVATE_H_INCLUDED
#define	ARCHIVE_PRIVATE_H_INCLUDED

#include "archive.h"
#include "archive_string.h"

#define	ARCHIVE_WRITE_MAGIC	(0xb0c5c0deU)
#define	ARCHIVE_READ_MAGIC	(0xdeb0c5U)

struct archive {
	/*
	 * The magic/state values are used to sanity-check the
	 * client's usage.  If an API function is called at a
	 * rediculous time, or the client passes us an invalid
	 * pointer, these values allow me to catch that.
	 */
	unsigned	  magic;
	unsigned	  state;

	struct archive_entry	*entry;
	uid_t		  user_uid;	/* UID of current user. */

	/* Dev/ino of the archive being read/written. */
	dev_t		  skip_file_dev;
	ino_t		  skip_file_ino;

	/* Utility:  Pointer to a block of nulls. */
	const char 		*nulls;
	size_t			 null_length;

	/*
	 * Used by archive_read_data() to track blocks and copy
	 * data to client buffers, filling gaps with zero bytes.
	 */
	const char	 *read_data_block;
	off_t		  read_data_offset;
	off_t		  read_data_output_offset;
	size_t		  read_data_remaining;

	/* Callbacks to open/read/write/close archive stream. */
	archive_open_callback	*client_opener;
	archive_read_callback	*client_reader;
	archive_write_callback	*client_writer;
	archive_close_callback	*client_closer;
	void			*client_data;

	/*
	 * Blocking information.  Note that bytes_in_last_block is
	 * misleadingly named; I should find a better name.  These
	 * control the final output from all compressors, including
	 * compression_none.
	 */
	int		  bytes_per_block;
	int		  bytes_in_last_block;

	/*
	 * These control whether data within a gzip/bzip2 compressed
	 * stream gets padded or not.  If pad_uncompressed is set,
	 * the data will be padded to a full block before being
	 * compressed.  The pad_uncompressed_byte determines the value
	 * that will be used for padding.  Note that these have no
	 * effect on compression "none."
	 */
	int		  pad_uncompressed;
	int		  pad_uncompressed_byte; /* TODO: Support this. */

	/* Position in UNCOMPRESSED data stream. */
	off_t		  file_position;
	/* Position in COMPRESSED data stream. */
	off_t		  raw_position;
	/* File offset of beginning of most recently-read header. */
	off_t		  header_position;

	/*
	 * Detection functions for decompression: bid functions are
	 * given a block of data from the beginning of the stream and
	 * can bid on whether or not they support the data stream.
	 * General guideline: bid the number of bits that you actually
	 * test, e.g., 16 if you test a 2-byte magic value.  The
	 * highest bidder will have their init function invoked, which
	 * can set up pointers to specific handlers.
	 *
	 * On write, the client just invokes an archive_write_set function
	 * which sets up the data here directly.
	 */
	int	  compression_code;	/* Currently active compression. */
	const char *compression_name;
	struct {
		int	(*bid)(const void *buff, size_t);
		int	(*init)(struct archive *, const void *buff, size_t);
	}	decompressors[4];
	/* Read/write data stream (with compression). */
	void	 *compression_data;		/* Data for (de)compressor. */
	int	(*compression_init)(struct archive *);	/* Initialize. */
	int	(*compression_finish)(struct archive *);
	int	(*compression_write)(struct archive *, const void *, size_t);
	/*
	 * Read uses a peek/consume I/O model: the decompression code
	 * returns a pointer to the requested block and advances the
	 * file position only when requested by a consume call.  This
	 * reduces copying and also simplifies look-ahead for format
	 * detection.
	 */
	ssize_t	(*compression_read_ahead)(struct archive *,
		    const void **, size_t request);
	ssize_t	(*compression_read_consume)(struct archive *, size_t);

	/*
	 * Format detection is mostly the same as compression
	 * detection, with two significant differences: The bidders
	 * use the read_ahead calls above to examine the stream rather
	 * than having the supervisor hand them a block of data to
	 * examine, and the auction is repeated for every header.
	 * Winning bidders should set the archive_format and
	 * archive_format_name appropriately.  Bid routines should
	 * check archive_format and decline to bid if the format of
	 * the last header was incompatible.
	 *
	 * Again, write support is considerably simpler because there's
	 * no need for an auction.
	 */
	int		  archive_format;
	const char	 *archive_format_name;

	struct archive_format_descriptor {
		int	(*bid)(struct archive *);
		int	(*read_header)(struct archive *, struct archive_entry *);
		int	(*read_data)(struct archive *, const void **, size_t *, off_t *);
		int	(*read_data_skip)(struct archive *);
		int	(*cleanup)(struct archive *);
		void	 *format_data;	/* Format-specific data for readers. */
	}	formats[4];
	struct archive_format_descriptor	*format; /* Active format. */

	/*
	 * Storage for format-specific data.  Note that there can be
	 * multiple format readers active at one time, so we need to
	 * allow for multiple format readers to have their data
	 * available.  The pformat_data slot here is the solution: on
	 * read, it is gauranteed to always point to a void* variable
	 * that the format can use.
	 */
	void	**pformat_data;		/* Pointer to current format_data. */
	void	 *format_data;		/* Used by writers. */

	/*
	 * Pointers to format-specific functions for writing.  They're
	 * initialized by archive_write_set_format_XXX() calls.
	 */
	int	(*format_init)(struct archive *); /* Only used on write. */
	int	(*format_finish)(struct archive *);
	int	(*format_finish_entry)(struct archive *);
	int 	(*format_write_header)(struct archive *,
		    struct archive_entry *);
	int	(*format_write_data)(struct archive *,
		    const void *buff, size_t);

	/*
	 * Various information needed by archive_extract.
	 */
	struct extract		 *extract;
	void			(*extract_progress)(void *);
	void			 *extract_progress_user_data;
	void			(*cleanup_archive_extract)(struct archive *);

	int		  archive_error_number;
	const char	 *error;
	struct archive_string	error_string;
};


#define	ARCHIVE_STATE_ANY	0xFFFFU
#define	ARCHIVE_STATE_NEW	1U
#define	ARCHIVE_STATE_HEADER	2U
#define	ARCHIVE_STATE_DATA	4U
#define	ARCHIVE_STATE_EOF	8U
#define	ARCHIVE_STATE_CLOSED	0x10U
#define	ARCHIVE_STATE_FATAL	0x8000U

/* Check magic value and state; exit if it isn't valid. */
void	__archive_check_magic(struct archive *, unsigned magic,
	    unsigned state, const char *func);


int	__archive_read_register_format(struct archive *a,
	    void *format_data,
	    int (*bid)(struct archive *),
	    int (*read_header)(struct archive *, struct archive_entry *),
	    int (*read_data)(struct archive *, const void **, size_t *, off_t *),
	    int (*read_data_skip)(struct archive *),
	    int (*cleanup)(struct archive *));

int	__archive_read_register_compression(struct archive *a,
	    int (*bid)(const void *, size_t),
	    int (*init)(struct archive *, const void *,	size_t));

void	__archive_errx(int retvalue, const char *msg);

#define	err_combine(a,b)	((a) < (b) ? (a) : (b))


/*
 * Utility function to format a USTAR header into a buffer.  If
 * "strict" is set, this tries to create the absolutely most portable
 * version of a ustar header.  If "strict" is set to 0, then it will
 * relax certain requirements.
 *
 * Generally, format-specific declarations don't belong in this
 * header; this is a rare example of a function that is shared by
 * two very similar formats (ustar and pax).
 */
int
__archive_write_format_header_ustar(struct archive *, char buff[512],
    struct archive_entry *, int tartype, int strict);

#endif
