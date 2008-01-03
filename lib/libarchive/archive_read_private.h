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

struct archive_read {
	struct archive	archive;

	struct archive_entry	*entry;

	/* Dev/ino of the archive being read/written. */
	dev_t		  skip_file_dev;
	ino_t		  skip_file_ino;

	/* Utility:  Pointer to a block of nulls. */
	const unsigned char	*nulls;
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
	archive_skip_callback	*client_skipper;
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

	/* File offset of beginning of most recently-read header. */
	off_t		  header_position;

	/*
	 * Decompressors have a very specific lifecycle:
	 *    public setup function initializes a slot in this table
	 *    'config' holds minimal configuration data
	 *    bid() examines a block of data and returns a bid [1]
	 *    init() is called for successful bidder
	 *    'data' is initialized by init()
	 *    read() returns a pointer to the next block of data
	 *    consume() indicates how much data is used
	 *    skip() ignores bytes of data
	 *    finish() cleans up and frees 'data' and 'config'
	 *
	 * [1] General guideline: bid the number of bits that you actually
	 * test, e.g., 16 if you test a 2-byte magic value.
	 */
	struct decompressor_t {
		void *config;
		void *data;
		int	(*bid)(const void *buff, size_t);
		int	(*init)(struct archive_read *,
			    const void *buff, size_t);
		int	(*finish)(struct archive_read *);
		ssize_t	(*read_ahead)(struct archive_read *,
			    const void **, size_t);
		ssize_t	(*consume)(struct archive_read *, size_t);
		off_t	(*skip)(struct archive_read *, off_t);
	}	decompressors[4];

	/* Pointer to current decompressor. */
	struct decompressor_t *decompressor;

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
	 * Pointers to format-specific functions for writing.  They're
	 * initialized by archive_write_set_format_XXX() calls.
	 */
	int	(*format_init)(struct archive *); /* Only used on write. */
	int	(*format_finish)(struct archive *);
	int	(*format_finish_entry)(struct archive *);
	int 	(*format_write_header)(struct archive *,
		    struct archive_entry *);
	ssize_t	(*format_write_data)(struct archive *,
		    const void *buff, size_t);

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

struct decompressor_t
	*__archive_read_register_compression(struct archive_read *a,
	    int (*bid)(const void *, size_t),
	    int (*init)(struct archive_read *, const void *, size_t));

const void
	*__archive_read_ahead(struct archive_read *, size_t);

#endif
