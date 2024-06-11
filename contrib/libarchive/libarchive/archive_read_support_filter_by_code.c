/*-
 * Copyright (c) 2020 Martin Matuska
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
 */

#include "archive_platform.h"

#include "archive.h"
#include "archive_private.h"

int
archive_read_support_filter_by_code(struct archive *a, int filter_code)
{
	archive_check_magic(a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_read_support_filter_by_code");

	switch (filter_code) {
	case ARCHIVE_FILTER_NONE:
		return archive_read_support_filter_none(a);
		break;
	case ARCHIVE_FILTER_GZIP:
		return archive_read_support_filter_gzip(a);
		break;
	case ARCHIVE_FILTER_BZIP2:
		return archive_read_support_filter_bzip2(a);
		break;
	case ARCHIVE_FILTER_COMPRESS:
		return archive_read_support_filter_compress(a);
		break;
	case ARCHIVE_FILTER_LZMA:
		return archive_read_support_filter_lzma(a);
		break;
	case ARCHIVE_FILTER_XZ:
		return archive_read_support_filter_xz(a);
		break;
	case ARCHIVE_FILTER_UU:
		return archive_read_support_filter_uu(a);
		break;
	case ARCHIVE_FILTER_RPM:
		return archive_read_support_filter_rpm(a);
		break;
	case ARCHIVE_FILTER_LZIP:
		return archive_read_support_filter_lzip(a);
		break;
	case ARCHIVE_FILTER_LRZIP:
		return archive_read_support_filter_lrzip(a);
		break;
	case ARCHIVE_FILTER_LZOP:
		return archive_read_support_filter_lzop(a);
		break;
	case ARCHIVE_FILTER_GRZIP:
		return archive_read_support_filter_grzip(a);
		break;
	case ARCHIVE_FILTER_LZ4:
		return archive_read_support_filter_lz4(a);
		break;
	case ARCHIVE_FILTER_ZSTD:
		return archive_read_support_filter_zstd(a);
		break;
	}
	return (ARCHIVE_FATAL);
}
