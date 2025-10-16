/*-
 * Copyright (c) 2025 Nicholas Vinson
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

#define __LIBARCHIVE_BUILD 1
#include "archive_digest_private.h"

struct expected_digests {
	unsigned char md5[16];
	unsigned char rmd160[20];
	unsigned char sha1[20];
	unsigned char sha256[32];
	unsigned char sha384[48];
	unsigned char sha512[64];
};

archive_md5_ctx expectedMd5Ctx;
archive_rmd160_ctx expectedRmd160Ctx;
archive_sha1_ctx expectedSha1Ctx;
archive_sha256_ctx expectedSha256Ctx;
archive_sha384_ctx expectedSha384Ctx;
archive_sha512_ctx expectedSha512Ctx;

DEFINE_TEST(test_write_format_mtree_digests_no_digests_set_no_data)
{
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
}

DEFINE_TEST(test_write_format_mtree_digests_no_digests_set_empty_data)
{
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_write_data(a, "", 0);
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
}

DEFINE_TEST(test_write_format_mtree_digests_no_digests_set_non_empty_data)
{
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;
	char *data = "abcd";

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, data, 4));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, data, 4));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, data, 4));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, data, 4));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, data, 4));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, data, 4));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_write_data(a, data, 4);
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
}

DEFINE_TEST(test_write_format_mtree_digests_md5_digest_set_no_data)
{
#ifdef ARCHIVE_HAS_MD5
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.md5, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
	}), sizeof(ed.md5));

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5, ed.md5);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
#else
	skipping("This platform does not support MD5");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_md5_digest_set_empty_data)
{
#ifdef ARCHIVE_HAS_MD5
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.md5, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
	}), sizeof(ed.md5));

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5, ed.md5);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_write_data(a, "", 0);
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) != 0);

	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
#else
	skipping("This platform does not support MD5");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_md5_digest_set_non_empty_data)
{
#ifdef ARCHIVE_HAS_MD5
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.md5, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
	}), sizeof(ed.md5));

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5, ed.md5);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_write_data(a, "abcd", 4);
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) != 0);

	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
#else
	skipping("This platform does not support MD5");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_rmd160_digest_set_no_data)
{
#ifdef ARCHIVE_HAS_RMD160
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
#else
	skipping("This platform does not support RMD160");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_rmd160_digest_set_empty_data)
{
#ifdef ARCHIVE_HAS_RMD160
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.rmd160, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed
	}), sizeof(ed.rmd160));

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160, ed.rmd160);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_write_data(a, "", 0);
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) != 0);

	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
#else
	skipping("This platform does not support RMD160");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_rmd160_digest_set_non_empty_data)
{
#ifdef ARCHIVE_HAS_RMD160
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.rmd160, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed
	}), sizeof(ed.rmd160));

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160, ed.rmd160);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_write_data(a, "abcd", 4);
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) != 0);

	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
#else
	skipping("This platform does not support RMD160");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_sha1_digest_set_no_data)
{
#ifdef ARCHIVE_HAS_SHA1
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.sha1, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed
	}), sizeof(ed.sha1));

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1, ed.sha1);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
#else
	skipping("This platform does not support SHA1");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_sha1_digest_set_empty_data)
{
#ifdef ARCHIVE_HAS_SHA1
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.sha1, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed
	}), sizeof(ed.sha1));

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1, ed.sha1);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_write_data(a, "", 0);
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) != 0);

	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
#else
	skipping("This platform does not support SHA1");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_sha1_digest_set_non_empty_data)
{
#ifdef ARCHIVE_HAS_SHA1
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.sha1, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed
	}), sizeof(ed.sha1));

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1, ed.sha1);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_write_data(a, "abcd", 4);
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) != 0);

	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
#else
	skipping("This platform does not support SHA1");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_sha256_digest_set_no_data)
{
#ifdef ARCHIVE_HAS_SHA256
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.sha256, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed
	}), sizeof(ed.sha256));

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256, ed.sha256);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
#else
	skipping("This platform does not support SHA256");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_sha256_digest_set_empty_data)
{
#ifdef ARCHIVE_HAS_SHA256
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.sha256, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed
	}), sizeof(ed.sha256));

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256, ed.sha256);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_write_data(a, "", 0);
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) != 0);

	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
#else
	skipping("This platform does not support SHA256");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_sha256_digest_set_non_empty_data)
{
#ifdef ARCHIVE_HAS_SHA256
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.sha256, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed
	}), sizeof(ed.sha256));

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256, ed.sha256);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_write_data(a, "abcd", 4);
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) != 0);

	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
#else
	skipping("This platform does not support SHA256");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_sha384_digest_set_no_data)
{
#ifdef ARCHIVE_HAS_SHA384
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.sha384, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed
	}), sizeof(ed.sha384));

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384, ed.sha384);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
#else
	skipping("This platform does not support SHA384");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_sha384_digest_set_empty_data)
{
#ifdef ARCHIVE_HAS_SHA384
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.sha384, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed
	}), sizeof(ed.sha384));

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384, ed.sha384);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_write_data(a, "", 0);
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) != 0);

	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
#else
	skipping("This platform does not support SHA384");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_sha384_digest_set_non_empty_data)
{
#ifdef ARCHIVE_HAS_SHA384
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.sha384, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed
	}), sizeof(ed.sha384));

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA512
	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384, ed.sha384);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_write_data(a, "abcd", 4);
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) != 0);

	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);

#ifdef ARCHIVE_HAS_SHA512
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);
#endif
	archive_entry_free(entry);
#else
	skipping("This platform does not support SHA384");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_sha512_digest_set_no_data)
{
#ifdef ARCHIVE_HAS_SHA512
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.sha512, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed
	}), sizeof(ed.sha512));

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512, ed.sha512);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);

	archive_entry_free(entry);
#else
	skipping("This platform does not support SHA512");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_sha512_digest_set_empty_data)
{
#ifdef ARCHIVE_HAS_SHA512
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.sha512, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed
	}), sizeof(ed.sha512));

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512, ed.sha512);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_write_data(a, "", 0);
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) != 0);

	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "", 0));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);

	archive_entry_free(entry);
#else
	skipping("This platform does not support SHA512");
	return;
#endif
}

DEFINE_TEST(test_write_format_mtree_digests_sha512_digest_set_non_empty_data)
{
#ifdef ARCHIVE_HAS_SHA512
	char buff[4096] = {0};
	size_t used = 0;
	struct archive *a;
	struct archive_entry *entry;
	struct expected_digests ed;

	memcpy(ed.sha512, ((unsigned char[]) {
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed, 0xfe, 0xed,
		0xfe, 0xed, 0xfe, 0xed
	}), sizeof(ed.sha512));

#ifdef ARCHIVE_HAS_MD5
	assertEqualInt(ARCHIVE_OK, archive_md5_init(&expectedMd5Ctx));
	assertEqualInt(ARCHIVE_OK, archive_md5_update(&expectedMd5Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_md5_final(&expectedMd5Ctx, ed.md5));
#endif

#ifdef ARCHIVE_HAS_RMD160
	assertEqualInt(ARCHIVE_OK, archive_rmd160_init(&expectedRmd160Ctx));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_update(&expectedRmd160Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_rmd160_final(&expectedRmd160Ctx, ed.rmd160));
#endif

#ifdef ARCHIVE_HAS_SHA1
	assertEqualInt(ARCHIVE_OK, archive_sha1_init(&expectedSha1Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha1_update(&expectedSha1Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha1_final(&expectedSha1Ctx, ed.sha1));
#endif

#ifdef ARCHIVE_HAS_SHA256
	assertEqualInt(ARCHIVE_OK, archive_sha256_init(&expectedSha256Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha256_update(&expectedSha256Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha256_final(&expectedSha256Ctx, ed.sha256));
#endif

#ifdef ARCHIVE_HAS_SHA384
	assertEqualInt(ARCHIVE_OK, archive_sha384_init(&expectedSha384Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha384_update(&expectedSha384Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha384_final(&expectedSha384Ctx, ed.sha384));
#endif

	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_mtree(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_options(a, "all"));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff) - 1, &used));
	assert((entry = archive_entry_new()) != NULL);
	archive_entry_set_pathname(entry, "test.data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_size(entry, 4);
	archive_entry_set_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512, ed.sha512);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, entry));
	archive_write_data(a, "abcd", 4);
	archive_entry_free(entry);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assert((a = archive_read_new()) != NULL);
	assert((entry = archive_entry_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &entry));

#ifdef ARCHIVE_HAS_MD5
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_MD5), ed.md5, sizeof(ed.md5)) == 0);
#endif

#ifdef ARCHIVE_HAS_RMD160
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_RMD160), ed.rmd160, sizeof(ed.rmd160)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA1
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA1), ed.sha1, sizeof(ed.sha1)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA256
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA256), ed.sha256, sizeof(ed.sha256)) == 0);
#endif

#ifdef ARCHIVE_HAS_SHA384
	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA384), ed.sha384, sizeof(ed.sha384)) == 0);
#endif

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) != 0);

	assertEqualInt(ARCHIVE_OK, archive_sha512_init(&expectedSha512Ctx));
	assertEqualInt(ARCHIVE_OK, archive_sha512_update(&expectedSha512Ctx, "abcd", 4));
	assertEqualInt(ARCHIVE_OK, archive_sha512_final(&expectedSha512Ctx, ed.sha512));

	assert(memcmp(archive_entry_digest(entry, ARCHIVE_ENTRY_DIGEST_SHA512), ed.sha512, sizeof(ed.sha512)) == 0);

	archive_entry_free(entry);
#else
	skipping("This platform does not support SHA512");
	return;
#endif
}
