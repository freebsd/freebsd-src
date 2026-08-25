/*-
 * Copyright (c) 2026 Tim Kientzle
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
#include <limits.h>

/*
 * Focused tests for POSIX.1e ACL text parsing and serialization.
 *
 * test_acl_text.c covers basic POSIX.1e round-trips but does not exercise:
 * the mask entry, the Solaris two-field format for mask and other, uppercase
 * mode letters (R/W/X), isint() overflow on the POSIX path, and various
 * invalid-input error paths.  These tests fill those gaps.
 *
 * Note: archive_entry_acl_reset(ae, TYPE_ACCESS) always returns the count
 * of extended ACL entries PLUS 3 synthesized base entries (USER_OBJ,
 * GROUP_OBJ, OTHER) derived from the mode bits.  Tests below account for
 * this by adding 3 to the expected count and skipping those 3 entries
 * before iterating over the named/mask entries of interest.
 */

/* ACCESS-only ACL including a mask entry. */
static const char *posix1e_access =
    "user::rwx\n"
    "group::r-x\n"
    "other::r-x\n"
    "user:alice:r-x\n"
    "group:staff:rwx\n"
    "mask::r-x";

/* Combined ACCESS + DEFAULT ACL, with mask in both sections. */
static const char *posix1e_full =
    "user::rwx\n"
    "group::r-x\n"
    "other::r-x\n"
    "user:alice:r-x\n"
    "group:staff:rwx\n"
    "mask::r-x\n"
    "default:user::r-x\n"
    "default:group::r-x\n"
    "default:other::---\n"
    "default:user:alice:r-x\n"
    "default:group:staff:--x\n"
    "default:mask::r-x";

/* posix1e_full with numeric IDs appended to named user/group entries. */
static const char *posix1e_extra_id =
    "user::rwx\n"
    "group::r-x\n"
    "other::r-x\n"
    "user:alice:r-x:77\n"
    "group:staff:rwx:78\n"
    "mask::r-x\n"
    "default:user::r-x\n"
    "default:group::r-x\n"
    "default:other::---\n"
    "default:user:alice:r-x:77\n"
    "default:group:staff:--x:78\n"
    "default:mask::r-x";

/*
 * Solaris-style ACCESS ACL: mask and other use one colon (no empty name
 * field between tag and permission string).
 */
static const char *posix1e_solaris =
    "user::rwx\n"
    "group::r-x\n"
    "other:r-x\n"
    "user:alice:r-x\n"
    "mask:r-x";

static wchar_t *
s_to_ws(const char *s)
{
	size_t len = strlen(s) + 1;
	wchar_t *ws = malloc(len * sizeof(wchar_t));
	assert(ws != NULL);
	assert(mbstowcs(ws, s, len) != (size_t)-1);
	return (ws);
}

/*
 * Skip the three base entries (USER_OBJ, GROUP_OBJ, OTHER) that
 * archive_entry_acl_reset() synthesises from mode bits and that
 * archive_entry_acl_next() always returns first for ACCESS ACLs.
 */
static void
skip_base_entries(struct archive_entry *ae)
{
	int type, permset, tag, qual;
	const char *name;
	int i;

	for (i = 0; i < 3; i++)
		archive_entry_acl_next(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
		    &type, &permset, &tag, &qual, &name);
}

/*
 * Round-trip: parse POSIX.1e text → serialize → compare.
 * Covers ACCESS-only, combined ACCESS+DEFAULT, and the EXTRA_ID style,
 * for both the narrow and wide serializers.
 */
DEFINE_TEST(test_acl_posix1e_text_roundtrip)
{
	struct archive_entry *ae;
	char *text;
	wchar_t *wtext, *ws;
	ssize_t len;

	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "file");
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* ACCESS-only with mask entry */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text(ae, posix1e_access,
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	text = archive_entry_acl_to_text(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
	assertEqualString(posix1e_access, text);
	free(text);
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* Combined ACCESS + DEFAULT — serializer auto-adds MARK_DEFAULT */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text(ae, posix1e_full,
	    ARCHIVE_ENTRY_ACL_TYPE_POSIX1E));
	text = archive_entry_acl_to_text(ae, &len, 0);
	assertEqualString(posix1e_full, text);
	free(text);
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* EXTRA_ID format */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text(ae, posix1e_extra_id,
	    ARCHIVE_ENTRY_ACL_TYPE_POSIX1E));
	text = archive_entry_acl_to_text(ae, &len,
	    ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID);
	assertEqualString(posix1e_extra_id, text);
	free(text);
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* Wide: ACCESS-only round-trip */
	ws = s_to_ws(posix1e_access);
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text_w(ae, ws,
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	wtext = archive_entry_acl_to_text_w(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
	assertEqualWString(ws, wtext);
	free(wtext);
	free(ws);
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* Wide: combined ACCESS + DEFAULT */
	ws = s_to_ws(posix1e_full);
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text_w(ae, ws,
	    ARCHIVE_ENTRY_ACL_TYPE_POSIX1E));
	wtext = archive_entry_acl_to_text_w(ae, &len, 0);
	assertEqualWString(ws, wtext);
	free(wtext);
	free(ws);

	archive_entry_free(ae);
}

/*
 * Solaris two-field format: "mask:rwx" and "other:rwx" omit the empty
 * name field, so each has only two colon-separated tokens instead of three.
 * The parser recognises this form; the serializer reproduces it when
 * ARCHIVE_ENTRY_ACL_STYLE_SOLARIS is set.
 */
DEFINE_TEST(test_acl_posix1e_text_solaris)
{
	struct archive_entry *ae;
	char *text;
	int type, permset, tag, qual;
	const char *name;
	ssize_t len;

	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "file");
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/*
	 * Parse Solaris-format text.  Extended entries are: named user alice
	 * and mask — 2 extended + 3 base = 5 total.
	 */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text(ae, posix1e_solaris,
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(5,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	skip_base_entries(ae);
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_next(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
	    &type, &permset, &tag, &qual, &name));
	assertEqualInt(ARCHIVE_ENTRY_ACL_USER, tag);
	assertEqualString("alice", name);
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_next(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
	    &type, &permset, &tag, &qual, &name));
	assertEqualInt(ARCHIVE_ENTRY_ACL_MASK, tag);
	assertEqualInt(ARCHIVE_ENTRY_ACL_READ | ARCHIVE_ENTRY_ACL_EXECUTE,
	    permset);

	/* Serialize with STYLE_SOLARIS and compare */
	text = archive_entry_acl_to_text(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS | ARCHIVE_ENTRY_ACL_STYLE_SOLARIS);
	assertEqualString(posix1e_solaris, text);
	free(text);
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* Solaris style with non-empty name on other field → ARCHIVE_WARN */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae,
	    "user::rwx\ngroup::r-x\nother:nobody:r-x\n",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	archive_entry_acl_clear(ae);

	archive_entry_free(ae);
}

/*
 * ismode() accepts uppercase R, W, X as well as lowercase; the serializer
 * always emits lowercase.  Both narrow and wide parsers share this
 * behaviour.
 */
DEFINE_TEST(test_acl_posix1e_text_ismode)
{
	struct archive_entry *ae;
	char *text;
	int type, permset, tag, qual;
	const char *name;
	ssize_t len;

	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "file");
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/*
	 * Uppercase R/W/X treated identically to r/w/x.
	 * 1 extended entry (alice) + 3 base = 4 total.
	 */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text(ae,
	    "user::RWX\ngroup::R-X\nother::---\nuser:alice:RwX",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(4,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	skip_base_entries(ae);
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_next(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
	    &type, &permset, &tag, &qual, &name));
	assertEqualInt(ARCHIVE_ENTRY_ACL_USER, tag);
	assertEqualString("alice", name);
	assertEqualInt(
	    ARCHIVE_ENTRY_ACL_READ | ARCHIVE_ENTRY_ACL_WRITE |
	    ARCHIVE_ENTRY_ACL_EXECUTE, permset);

	/* Serializer always emits lowercase */
	text = archive_entry_acl_to_text(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
	assertEqualString(
	    "user::rwx\ngroup::r-x\nother::---\nuser:alice:rwx", text);
	free(text);
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* Same via the wide parser */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text_w(ae,
	    L"user::RWX\ngroup::R-X\nother::---\nuser:alice:RwX",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(4,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	skip_base_entries(ae);
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_next(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
	    &type, &permset, &tag, &qual, &name));
	assertEqualInt(
	    ARCHIVE_ENTRY_ACL_READ | ARCHIVE_ENTRY_ACL_WRITE |
	    ARCHIVE_ENTRY_ACL_EXECUTE, permset);

	archive_entry_free(ae);
}

/*
 * Numeric IDs in the name field and the extra-ID override field,
 * including the isint() overflow boundary now enforced on the POSIX path.
 */
DEFINE_TEST(test_acl_posix1e_text_numeric_id)
{
	struct archive_entry *ae;
	char *text;
	ssize_t len;
	int type, permset, tag, qual;
	const char *name;

	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "file");
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* 1. Numeric-only name → id derived from name field */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text(ae,
	    "user::rwx\ngroup::r-x\nother::r-x\nuser:1000:rwx",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	/* 1 extended + 3 base = 4 */
	assertEqualInt(4,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	skip_base_entries(ae);
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_next(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
	    &type, &permset, &tag, &qual, &name));
	assertEqualInt(1000, qual);
	assertEqualString("1000", name);
	/* Without EXTRA_ID the id is carried by the name string */
	text = archive_entry_acl_to_text(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
	assert(strstr(text, "user:1000:rwx") != NULL);
	free(text);
	/* With EXTRA_ID the id is also appended */
	text = archive_entry_acl_to_text(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS | ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID);
	assert(strstr(text, "user:1000:rwx:1000") != NULL);
	free(text);
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* 2. Trailing extra-ID field overrides name-derived id */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text(ae,
	    "user::rwx\ngroup::r-x\nother::r-x\nuser:alice:r-x:42",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(4,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	skip_base_entries(ae);
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_next(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
	    &type, &permset, &tag, &qual, &name));
	assertEqualInt(42, qual);
	assertEqualString("alice", name);
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* 3. Overflow in extra-ID field → extended entry rejected, no ext entries */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae,
	    "user::rwx\ngroup::r-x\nother::r-x\nuser:alice:r-x:99999999999",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* 4. INT_MAX (2147483647) in the extra-ID field → also rejected */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae,
	    "user::rwx\ngroup::r-x\nother::r-x\nuser:alice:r-x:2147483647",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* 5. INT_MAX - 1 (2147483646) is the largest accepted id */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text(ae,
	    "user::rwx\ngroup::r-x\nother::r-x\nuser:alice:r-x:2147483646",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(4,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	skip_base_entries(ae);
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_next(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
	    &type, &permset, &tag, &qual, &name));
	assertEqualInt(2147483646, qual);
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* 6. Overflow in the name field (all-digit name too large) → rejected */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae,
	    "user::rwx\ngroup::r-x\nother::r-x\nuser:99999999999:r-x",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	archive_entry_acl_clear(ae);

	archive_entry_free(ae);
}

/*
 * Malformed POSIX.1e ACL entries — each should return ARCHIVE_WARN without
 * crashing and without storing invalid data.
 */
DEFINE_TEST(test_acl_posix1e_text_invalid)
{
	struct archive_entry *ae;
	int type, permset, tag, qual;
	const char *name;

	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "file");
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* Invalid character in permission field */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae,
	    "user:alice:rqx",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* Unknown tag */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae,
	    "bogus:alice:rwx",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* NFSv4 "everyone@" tag is not recognised by the POSIX parser */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae,
	    "everyone@:r-----a-R-c--s:-------:allow",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* Bare tag with no colon-separated fields */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae, "user",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/*
	 * Valid entry followed by invalid — valid entry must be stored.
	 * 1 extended (alice) + 3 base = 4 total.
	 */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae,
	    "user::rwx\ngroup::r-x\nother::r-x\n"
	    "user:alice:r-x\n"
	    "bogus:bob:rwx",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(4,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	skip_base_entries(ae);
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_next(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
	    &type, &permset, &tag, &qual, &name));
	assertEqualInt(ARCHIVE_ENTRY_ACL_USER, tag);
	assertEqualString("alice", name);
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* Wide parser: invalid permission character */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text_w(ae,
	    L"user:alice:rqx",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* Wide parser: unknown tag */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text_w(ae,
	    L"bogus:alice:rwx",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	archive_entry_acl_clear(ae);
	archive_entry_set_mode(ae, AE_IFREG | 0755);

	/* Wide parser: NFSv4 tag rejected by POSIX parser */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text_w(ae,
	    L"everyone@:r-----a-R-c--s:-------:allow",
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));

	archive_entry_free(ae);
}
