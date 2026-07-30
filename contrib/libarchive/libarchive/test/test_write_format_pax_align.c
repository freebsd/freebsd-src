/*-
 * Copyright (c) 2024 libarchive contributors
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
#include "test.h"

/*
 * The pax "align" option must start each big-enough regular file's data on
 * an alignment boundary in the archive stream, so it can be reflinked out.
 */

#define ALIGN 4096

struct file {
	const char *name;
	size_t size;
	char fill;
	int add_xattr;		/* force a "natural" pax extended header */
};

/*
 * A mix of entries: small/large regular files, a directory, long-name and
 * xattr entries (which force a natural pax header), and an exactly-align file.
 */
static const struct file files[] = {
	{ "small",		100,		'a', 0 },
	{ "big1",		5000,		'b', 0 },
	{ "adir/",		0,		0,   0 },
	{ "big2",		ALIGN,		'c', 0 },
	{ "tiny",		1,		'd', 0 },
	{ "big3",		100000,		'e', 0 },
	{ "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/longname", 8192, 'f', 0 },
	{ "withxattr",		9000,		'g', 1 },
	{ NULL, 0, 0, 0 }
};

static void
verify_align_limit(void)
{
	struct archive *a;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_pax_restricted(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_options(a, "align=1048576"));
	assertEqualIntA(a, ARCHIVE_FAILED,
	    archive_write_set_options(a, "align=2097152"));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
}

static int
memory_contains(const char *memory, size_t memory_size, const char *needle)
{
	size_t needle_size = strlen(needle);
	size_t i;

	for (i = 0; i + needle_size <= memory_size; i++) {
		if (memcmp(memory + i, needle, needle_size) == 0)
			return (1);
	}
	return (0);
}

static void
verify_sparse_not_padded(void)
{
	struct archive *a;
	struct archive_entry *ae;
	char *buff, *data;
	size_t buffsize = 65536, used;

	buff = malloc(buffsize);
	data = calloc(1, 8192);
	assert(buff != NULL);
	assert(data != NULL);
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_pax_restricted(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_options(a, "align=4096"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, &used));

	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "sparse");
	archive_entry_set_mode(ae, S_IFREG | 0644);
	archive_entry_set_size(ae, 8192);
	archive_entry_sparse_add_entry(ae, 4096, 4096);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	assertEqualIntA(a, 8192, archive_write_data(a, data, 8192));
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	failure("Sparse entries must not get misleading alignment padding");
	assert(!memory_contains(buff, used, "LIBARCHIVE.pad"));
	free(data);
	free(buff);
}

static void
write_archive(char *buff, size_t buffsize, size_t *used, int gzip)
{
	struct archive *a;
	struct archive_entry *ae;
	const struct file *f;
	char *data;

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_pax_restricted(a));
	if (gzip)
		assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_gzip(a));
	else
		assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	/* Small blocks so the writer doesn't tail-pad past our offsets. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_bytes_per_block(a, 512));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_options(a, "align=4096"));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff, buffsize, used));

	assert((ae = archive_entry_new()) != NULL);
	for (f = files; f->name != NULL; f++) {
		archive_entry_clear(ae);
		archive_entry_set_pathname(ae, f->name);
		archive_entry_set_mtime(ae, 5, 0);
		if (f->name[strlen(f->name) - 1] == '/') {
			archive_entry_set_mode(ae, S_IFDIR | 0755);
		} else {
			archive_entry_set_mode(ae, S_IFREG | 0644);
			archive_entry_set_size(ae, f->size);
		}
		if (f->add_xattr)
			archive_entry_xattr_add_entry(ae, "user.test",
			    "value", 5);
		assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
		if (f->size > 0) {
			data = malloc(f->size);
			assert(data != NULL);
			memset(data, f->fill, f->size);
			assertEqualIntA(a, f->size,
			    archive_write_data(a, data, f->size));
			free(data);
		}
	}
	archive_entry_free(ae);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
}

static void
verify_archive(const char *buff, size_t used, int gzip)
{
	struct archive *a;
	struct archive_entry *ae;
	const struct file *f;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	if (gzip)
		assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_gzip(a));
	else
		assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, buff, used));

	for (f = files; f->name != NULL; f++) {
		int64_t off, size;
		assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
		assertEqualStringA(a, f->name, archive_entry_pathname(ae));
		size = archive_entry_size(ae);
		assertEqualIntA(a, f->size, size);
		/*
		 * archive_filter_bytes(a, 0) is the offset of the data in
		 * the *decompressed* archive stream, i.e. exactly where a
		 * reflink would read from an uncompressed archive.
		 */
		off = archive_filter_bytes(a, 0);
		if (archive_entry_filetype(ae) == AE_IFREG && size >= ALIGN) {
			failure("data for '%s' (size %jd) must start on a "
			    "%d-byte boundary but starts at %jd",
			    f->name, (intmax_t)size, ALIGN, (intmax_t)off);
			assertEqualInt(0, (int)(off % ALIGN));
		}
		/* Data must still round-trip intact through the padding. */
		if (size > 0) {
			char *data = malloc((size_t)size);
			assert(data != NULL);
			assertEqualIntA(a, size,
			    archive_read_data(a, data, (size_t)size));
			assertMemoryFilledWith(data, (size_t)size, f->fill);
			free(data);
		}
	}
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_write_format_pax_align)
{
	size_t buffsize = 2000000;
	char *buff;
	size_t used;

	verify_align_limit();
	verify_sparse_not_padded();

	buff = malloc(buffsize);
	assert(buff != NULL);

	/* Uncompressed: the archive itself is aligned on disk. */
	write_archive(buff, buffsize, &used, 0);
	verify_archive(buff, used, 0);

	/* Compressed: the *decompressed* stream is still aligned, which is
	 * what a reflink-from-decompressed-copy relies on. */
	if (canGzip()) {
		write_archive(buff, buffsize, &used, 1);
		verify_archive(buff, used, 1);
	} else {
		skipping("gzip unavailable; skipped compressed alignment check");
	}

	free(buff);
}
