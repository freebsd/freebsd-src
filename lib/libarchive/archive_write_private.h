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

#ifndef ARCHIVE_WRITE_PRIVATE_H_INCLUDED
#define	ARCHIVE_WRITE_PRIVATE_H_INCLUDED

#include "archive.h"
#include "archive_string.h"
#include "archive_private.h"

struct archive_write {
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

	/*
	 * On write, the client just invokes an archive_write_set function
	 * which sets up the data here directly.
	 */
	void	 *compression_data;		/* Data for (de)compressor. */
	int	(*compression_init)(struct archive_write *);	/* Initialize. */
	int	(*compression_finish)(struct archive_write *);
	int	(*compression_write)(struct archive_write *, const void *, size_t);
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
	off_t (*compression_skip)(struct archive *, off_t);

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
	}	formats[8];
	struct archive_format_descriptor	*format; /* Active format. */

	/*
	 * Storage for format-specific data.  Note that there can be
	 * multiple format readers active at one time, so we need to
	 * allow for multiple format readers to have their data
	 * available.  The pformat_data slot here is the solution: on
	 * read, it is guaranteed to always point to a void* variable
	 * that the format can use.
	 */
	void	**pformat_data;		/* Pointer to current format_data. */
	void	 *format_data;		/* Used by writers. */

	/*
	 * Pointers to format-specific functions for writing.  They're
	 * initialized by archive_write_set_format_XXX() calls.
	 */
	int	(*format_init)(struct archive_write *);
	int	(*format_finish)(struct archive_write *);
	int	(*format_destroy)(struct archive_write *);
	int	(*format_finish_entry)(struct archive_write *);
	int 	(*format_write_header)(struct archive_write *,
		    struct archive_entry *);
	ssize_t	(*format_write_data)(struct archive_write *,
		    const void *buff, size_t);

	/*
	 * Various information needed by archive_extract.
	 */
	struct extract		 *extract;
	int			(*cleanup_archive_extract)(struct archive *);
};

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
__archive_write_format_header_ustar(struct archive_write *, char buff[512],
    struct archive_entry *, int tartype, int strict);

#endif
