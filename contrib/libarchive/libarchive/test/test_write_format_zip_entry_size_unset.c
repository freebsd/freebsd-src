/*-
 * Copyright (c) 2021 Jia Cheong Tan
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
 */

#include "test.h"

/* File data */
static const char file_name[] = "file";
static const char file_data1[] = {'a', 'b', 'c', 'd', 'e'};
static const char file_data2[] = {'f', 'g', 'h', 'i', 'j'};
static const int file_perm = 00644;
static const short file_uid = 10;
static const short file_gid = 20;

/* Folder data */
static const char folder_name[] = "folder/";
static const int folder_perm = 00755;
static const short folder_uid = 30;
static const short folder_gid = 40;

#define ZIP_ENTRY_FLAG_LENGTH_AT_END (1 << 3)

/* Quick and dirty: Read 2-byte and 4-byte integers from Zip file. */
static unsigned i2(const char *p) { return ((p[0] & 0xff) | ((p[1] & 0xff) << 8)); }
static unsigned i4(const char *p) { return (i2(p) | (i2(p + 2) << 16)); }

static unsigned long
bitcrc32(unsigned long c, const void *_p, size_t s)
{
	/* This is a drop-in replacement for crc32() from zlib.
	 * Libarchive should be able to correctly generate
	 * uncompressed zip archives (including correct CRCs) even
	 * when zlib is unavailable, and this function helps us verify
	 * that.  Yes, this is very, very slow and unsuitable for
	 * production use, but it's correct, compact, and works well
	 * enough for this particular usage.  Libarchive internally
	 * uses a much more efficient implementation.  */
	const unsigned char *p = _p;
	int bitctr;

	if (p == NULL)
		return (0);

	for (; s > 0; --s)
	{
		c ^= *p++;
		for (bitctr = 8; bitctr > 0; --bitctr)
		{
			if (c & 1)
				c = (c >> 1);
			else
				c = (c >> 1) ^ 0xedb88320;
			c ^= 0x80000000;
		}
	}
	return (c);
}

static void write_archive(struct archive *a)
{
	struct archive_entry *entry = archive_entry_new();
	assert(entry != NULL);

	/* Does not set size for file entry */
	archive_entry_set_pathname(entry, file_name);
	archive_entry_set_mode(entry, S_IFREG | 0644);
	archive_entry_set_uid(entry, file_uid);
	archive_entry_set_gid(entry, file_gid);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	assertEqualIntA(a, sizeof(file_data1), archive_write_data(a, file_data1, sizeof(file_data1)));
	assertEqualIntA(a, sizeof(file_data2), archive_write_data(a, file_data2, sizeof(file_data2)));
	archive_entry_free(entry);

	/* Folder */
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, folder_name);
	archive_entry_set_mode(entry, S_IFDIR | folder_perm);
	archive_entry_set_size(entry, 0);
	archive_entry_set_uid(entry, folder_uid);
	archive_entry_set_gid(entry, folder_gid);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);
}

static void verify_contents(const char *zip_buff, size_t size)
{
	unsigned long crc = bitcrc32(0, file_data1, sizeof(file_data1));
	crc = bitcrc32(crc, file_data2, sizeof(file_data2));

	const char *zip_end = zip_buff + size;
	/* Since there are no comments, the end of central directory
    *  is 22 bytes from the end of content */
	const char *end_of_central_dir = zip_end - 22;
	/* Check for end of central directory signature */
	assertEqualMem(end_of_central_dir, "PK\x5\x6", 4);
	/* Check for number of disk */
	assertEqualInt(i2(end_of_central_dir + 4), 0);
	/* Check for disk where central directory starts */
	assertEqualInt(i2(end_of_central_dir + 6), 0);
	/* Check for number of central directory records on disk */
	assertEqualInt(i2(end_of_central_dir + 8), 2);
	/* Check for total number of central directory records */
	assertEqualInt(i2(end_of_central_dir + 10), 2);
	/* Check for size of central directory and offset
    *  The size + offset must equal the end of the central directory */
	assertEqualInt(i4(end_of_central_dir + 12) + i4(end_of_central_dir + 16), end_of_central_dir - zip_buff);
	/* Check for empty comment length */
	assertEqualInt(i2(end_of_central_dir + 20), 0);

	/* Get address of central directory */
	const char *central_directory = zip_buff + i4(end_of_central_dir + 16);

	/* Check for entry in central directory signature */
	assertEqualMem(central_directory, "PK\x1\x2", 4);
	/* Check for version used to write entry */
	assertEqualInt(i2(central_directory + 4), 3 * 256 + 10);
	/* Check for version needed to extract entry */
	assertEqualInt(i2(central_directory + 6), 10);
	/* Check flags */
	assertEqualInt(i2(central_directory + 8), ZIP_ENTRY_FLAG_LENGTH_AT_END);
	/* Check compression method */
	assertEqualInt(i2(central_directory + 10), 0);
	/* Check crc value */
	assertEqualInt(i4(central_directory + 16), crc);
	/* Check compressed size*/
	assertEqualInt(i4(central_directory + 20), sizeof(file_data1) + sizeof(file_data2));
	/* Check uncompressed size */
	assertEqualInt(i4(central_directory + 24), sizeof(file_data1) + sizeof(file_data2));
	/* Check file name length */
	assertEqualInt(i2(central_directory + 28), strlen(file_name));
	/* Check extra field length */
	assertEqualInt(i2(central_directory + 30), 15);
	/* Check file comment length */
	assertEqualInt(i2(central_directory + 32), 0);
	/* Check disk number where file starts */
	assertEqualInt(i2(central_directory + 34), 0);
	/* Check internal file attrs */
	assertEqualInt(i2(central_directory + 36), 0);
	/* Check external file attrs */
	assertEqualInt(i4(central_directory + 38) >> 16 & 01777, file_perm);
	/* Check offset of local header */
	assertEqualInt(i4(central_directory + 42), 0);
	/* Check for file name contents */
	assertEqualMem(central_directory + 46, file_name, strlen(file_name));

	/* Get address of local file entry */
	const char *local_file_header = zip_buff;

	/* Check local file header signature */
	assertEqualMem(local_file_header, "PK\x3\x4", 4);
	/* Check version needed to extract */
	assertEqualInt(i2(local_file_header + 4), 10);
	/* Check flags */
	assertEqualInt(i2(local_file_header + 6), 8);
	/* Check compression method */
	assertEqualInt(i2(local_file_header + 8), 0);
	/* Check crc */
	assertEqualInt(i4(local_file_header + 14), 0);
	/* Check compressed size
    *  0 because it was unknown at time of writing */
	assertEqualInt(i4(local_file_header + 18), 0);
	/* Check uncompressed size
    *  0 because it was unknown at time of writing */
	assertEqualInt(i4(local_file_header + 22), 0);
	/* Check pathname length */
	assertEqualInt(i2(local_file_header + 26), strlen(file_name));
	/* Check extra field length */
	assertEqualInt(i2(local_file_header + 28), 15);
	/* Check path name match */
	assertEqualMem(local_file_header + 30, file_name, strlen(file_name));

	/* Start of data */
	const char *data = local_file_header + i2(local_file_header + 28) + strlen(file_name) + 30;
	/* Check for file data match */
	assertEqualMem(data, file_data1, sizeof(file_data1));
	assertEqualMem(data + sizeof(file_data1), file_data2, sizeof(file_data2));

	/* Start of data descriptor */
	const char *data_descriptor = data + sizeof(file_data1) + sizeof(file_data2);
	/* Check data descriptor signature */
	assertEqualMem(data_descriptor, "PK\x7\x8", 4);
	/* Check crc value */
	assertEqualInt(i4(data_descriptor + 4), crc);
	/* Check compressed size */
	assertEqualInt(i4(data_descriptor + 8), sizeof(file_data1) + sizeof(file_data2));
	/* Chcek uncompresed size */
	assertEqualInt(i4(data_descriptor + 12), sizeof(file_data1) + sizeof(file_data2));

	/* Get folder entry in central directory */
	const char *central_directory_folder_entry = central_directory + 46 + i2(local_file_header + 28) + strlen(file_name);

	/* Get start of folder entry */
	const char *local_folder_header = data_descriptor + 16;

	/* Check for entry in central directory signature */
	assertEqualMem(central_directory_folder_entry, "PK\x1\x2", 4);
	/* Check version made by */
	assertEqualInt(i2(central_directory_folder_entry + 4), 3 * 256 + 20);
	/* Check version needed to extract */
	assertEqualInt(i2(central_directory_folder_entry + 6), 20);
	/* Check flags */
	assertEqualInt(i2(central_directory_folder_entry + 8), 0);
	/* Check compression method */
	assertEqualInt(i2(central_directory_folder_entry + 10), 0);
	/* Check crc */
	assertEqualInt(i2(central_directory_folder_entry + 16), 0);
	/* Check compressed size */
	assertEqualInt(i4(central_directory_folder_entry + 20), 0);
	/* Check uncompressed size */
	assertEqualInt(i4(central_directory_folder_entry + 24), 0);
	/* Check path name length */
	assertEqualInt(i2(central_directory_folder_entry + 28), strlen(folder_name));
	/* Check extra field length */
	assertEqualInt(i2(central_directory_folder_entry + 30), 15);
	/* Check file comment length */
	assertEqualInt(i2(central_directory_folder_entry + 32), 0);
	/* Check disk number start */
	assertEqualInt(i2(central_directory_folder_entry + 34), 0);
	/* Check internal file attrs */
	assertEqualInt(i2(central_directory_folder_entry + 36), 0);
	/* Check external file attrs */
	assertEqualInt(i4(central_directory_folder_entry + 38) >> 16 & 01777, folder_perm);
	/* Check offset of local header*/
	assertEqualInt(i4(central_directory_folder_entry + 42), local_folder_header - zip_buff);
	/* Check path name */
	assertEqualMem(central_directory_folder_entry + 46, folder_name, strlen(folder_name));

	/* Check local header */
	assertEqualMem(local_folder_header, "PK\x3\x4", 4);
	/* Check version to extract */
	assertEqualInt(i2(local_folder_header + 4), 20);
	/* Check flags */
	assertEqualInt(i2(local_folder_header + 6), 0);
	/* Check compression method */
	assertEqualInt(i2(local_folder_header + 8), 0);
	/* Check crc */
	assertEqualInt(i4(local_folder_header + 14), 0);
	/* Check compressed size */
	assertEqualInt(i2(local_folder_header + 18), 0);
	/* Check uncompressed size */
	assertEqualInt(i4(local_folder_header + 22), 0);
	/* Check path name length */
	assertEqualInt(i2(local_folder_header + 26), strlen(folder_name));
	/* Check extra field length */
	assertEqualInt(i2(local_folder_header + 28), 15);
	/* Check path name */
	assertEqualMem(local_folder_header + 30, folder_name, strlen(folder_name));

	const char *post_local_folder = local_folder_header + 30 + i2(local_folder_header + 28) + strlen(folder_name);
	assertEqualMem(post_local_folder, central_directory, 4);
}

DEFINE_TEST(test_write_format_zip_size_unset)
{
	struct archive *a;
	char zip_buffer[100000];
	size_t size;

	/* Use compression=store to disable compression. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "zip:compression=store"));
	/* Disable zip64 explicitly since it is automatically enabled if no size is set */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "zip:zip64="));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_bytes_per_block(a, 1));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_bytes_in_last_block(a, 1));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, zip_buffer, sizeof(zip_buffer), &size));

	write_archive(a);

	/* Close the archive . */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	dumpfile("constructed_size_unset.zip", zip_buffer, size);

	verify_contents(zip_buffer, size);

	/* Use compression-level=0 to disable compression. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "zip:compression-level=0"));
	/* Disable zip64 explicitly since it is automatically enabled if no size is set */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "zip:zip64="));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_bytes_per_block(a, 1));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_bytes_in_last_block(a, 1));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, zip_buffer, sizeof(zip_buffer), &size));

	write_archive(a);

	/* Close the archive . */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));
	dumpfile("constructed_size_unset.zip", zip_buffer, size);

	verify_contents(zip_buffer, size);
}
