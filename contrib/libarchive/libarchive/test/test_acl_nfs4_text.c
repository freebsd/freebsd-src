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
 * Focused tests for NFSv4 ACL text parsing and serialization.
 *
 * test_acl_text.c already covers basic round-trips with well-formed input.
 * These tests probe areas not reached there: all four ACL types, numeric IDs,
 * extra-ID field override, isint() overflow, malformed entries, compact mode,
 * and the wide-char path.
 */

/* NFSv4 text in standard (no extra ID), extra-ID, and compact+extra-ID forms.
 * These are the same strings used in test_acl_text.c acltext[9..11]. */
static const char *nfs4_std =
    "user:user77:rw-p--a-R-c-o-:-------:allow\n"
    "user:user101:-w-pdD--------:fdin---:deny\n"
    "group:group78:r-----a-R-c---:------I:allow\n"
    "owner@:rwxp--aARWcCo-:-------:allow\n"
    "group@:rw-p--a-R-c---:-------:allow\n"
    "everyone@:r-----a-R-c--s:-------:allow";

static const char *nfs4_extra_id =
    "user:user77:rw-p--a-R-c-o-:-------:allow:77\n"
    "user:user101:-w-pdD--------:fdin---:deny:101\n"
    "group:group78:r-----a-R-c---:------I:allow:78\n"
    "owner@:rwxp--aARWcCo-:-------:allow\n"
    "group@:rw-p--a-R-c---:-------:allow\n"
    "everyone@:r-----a-R-c--s:-------:allow";

static const char *nfs4_compact =
    "user:user77:rwpaRco::allow:77\n"
    "user:user101:wpdD:fdin:deny:101\n"
    "group:group78:raRc:I:allow:78\n"
    "owner@:rwxpaARWcCo::allow\n"
    "group@:rwpaRc::allow\n"
    "everyone@:raRcs::allow";

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
 * Round-trip: parse NFS4 text → serialize → compare.
 * Covers standard, extra-ID, and compact+extra-ID formats,
 * for both the narrow and wide serializers.
 */
DEFINE_TEST(test_acl_nfs4_text_roundtrip)
{
	struct archive_entry *ae;
	char *text;
	wchar_t *wtext, *ws;
	ssize_t len;

	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "file");
	archive_entry_set_mode(ae, AE_IFREG | 0644);

	/* Standard format */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text(ae, nfs4_std,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	text = archive_entry_acl_to_text(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4);
	assertEqualString(nfs4_std, text);
	free(text);
	archive_entry_acl_clear(ae);

	/* Extra-ID format: trailing :id must be preserved */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text(ae, nfs4_extra_id,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	text = archive_entry_acl_to_text(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4 | ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID);
	assertEqualString(nfs4_extra_id, text);
	free(text);
	archive_entry_acl_clear(ae);

	/* Compact format: empty perm/flag fields must round-trip */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text(ae, nfs4_compact,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	text = archive_entry_acl_to_text(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4 |
	    ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID |
	    ARCHIVE_ENTRY_ACL_STYLE_COMPACT);
	assertEqualString(nfs4_compact, text);
	free(text);
	archive_entry_acl_clear(ae);

	/* Wide: standard format */
	ws = s_to_ws(nfs4_std);
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text_w(ae, ws,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	wtext = archive_entry_acl_to_text_w(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4);
	assertEqualWString(ws, wtext);
	free(wtext);
	free(ws);
	archive_entry_acl_clear(ae);

	/* Wide: compact+extra-ID format */
	ws = s_to_ws(nfs4_compact);
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text_w(ae, ws,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	wtext = archive_entry_acl_to_text_w(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4 |
	    ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID |
	    ARCHIVE_ENTRY_ACL_STYLE_COMPACT);
	assertEqualWString(ws, wtext);
	free(wtext);
	free(ws);
	archive_entry_acl_clear(ae);

	archive_entry_free(ae);
}

/*
 * All four ACL entry types: allow, deny, audit, alarm.
 * test_acl_text.c only exercises allow and deny.
 */
DEFINE_TEST(test_acl_nfs4_text_types)
{
	struct archive_entry *ae;
	char *text;
	wchar_t *wtext, *ws;
	ssize_t len;
	int type, permset, tag, qual;
	const char *name;
	static const char *audit_alarm_text =
	    "user:alice:rw-p--aARWcCos:-------:audit\n"
	    "group:staff:rw-p--aARWcCos:-------:alarm";

	ae = archive_entry_new();
	assert(ae != NULL);

	/* Parse and verify both audit and alarm entries are stored */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text(ae, audit_alarm_text,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(2,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_next(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4,
	    &type, &permset, &tag, &qual, &name));
	assertEqualInt(ARCHIVE_ENTRY_ACL_TYPE_AUDIT, type);
	assertEqualString("alice", name);
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_next(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4,
	    &type, &permset, &tag, &qual, &name));
	assertEqualInt(ARCHIVE_ENTRY_ACL_TYPE_ALARM, type);
	assertEqualString("staff", name);

	/* Round-trip: serialize then compare */
	text = archive_entry_acl_to_text(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4);
	assertEqualString(audit_alarm_text, text);

	/* Verify same round-trip via wide path */
	ws = s_to_ws(audit_alarm_text);
	wtext = archive_entry_acl_to_text_w(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4);
	assertEqualWString(ws, wtext);
	free(ws);
	free(wtext);
	free(text);

	archive_entry_free(ae);
}

/*
 * Numeric IDs in the name field and the extra-ID override field.
 *
 * 1. "user:1000:..." — the name field is a pure number; id is set from it.
 * 2. "user:name:...:42" — the trailing id field overrides any id derived
 *    from the name.
 * 3. "user:1000:...:42" — trailing id overrides even a numeric name.
 * 4. isint() overflow: a field with more than 10 digits is clamped to INT_MAX.
 */
DEFINE_TEST(test_acl_nfs4_text_numeric_id)
{
	struct archive_entry *ae;
	char *text;
	ssize_t len;
	int type, permset, tag, qual;
	const char *name;

	ae = archive_entry_new();
	assert(ae != NULL);

	/* 1. Numeric-only name → id derived from name field */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text(ae,
	    "user:1000:rw-p--a-R-c-o-:-------:allow",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(1,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_next(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4,
	    &type, &permset, &tag, &qual, &name));
	assertEqualInt(1000, qual);
	assertEqualString("1000", name);
	/* Without EXTRA_ID the id is carried by the name string, not :id */
	text = archive_entry_acl_to_text(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4);
	assertEqualString("user:1000:rw-p--a-R-c-o-:-------:allow", text);
	free(text);
	/* With EXTRA_ID the id is appended */
	text = archive_entry_acl_to_text(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4 | ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID);
	assertEqualString("user:1000:rw-p--a-R-c-o-:-------:allow:1000", text);
	free(text);
	archive_entry_acl_clear(ae);

	/* 2. Trailing extra-ID field overrides name-derived id */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text(ae,
	    "user:user77:rw-p--a-R-c-o-:-------:allow:42",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(1,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_next(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4,
	    &type, &permset, &tag, &qual, &name));
	assertEqualInt(42, qual);
	assertEqualString("user77", name);
	archive_entry_acl_clear(ae);

	/* 3. Trailing extra-ID also overrides a numeric name */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text(ae,
	    "user:1000:rw-p--a-R-c-o-:-------:allow:42",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(1,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_next(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4,
	    &type, &permset, &tag, &qual, &name));
	assertEqualInt(42, qual);
	archive_entry_acl_clear(ae);

	/* 4. isint() overflow: extra-ID field too large → entry rejected */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae,
	    "user:name:rw-p--a-R-c-o-:-------:allow:99999999999",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	archive_entry_acl_clear(ae);

	/* 5. INT_MAX (2147483647) in the extra-ID field → also rejected */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae,
	    "user:name:rw-p--a-R-c-o-:-------:allow:2147483647",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	archive_entry_acl_clear(ae);

	/* 6. INT_MAX - 1 (2147483646) is the largest accepted id */
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_from_text(ae,
	    "user:name:rw-p--a-R-c-o-:-------:allow:2147483646",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(1,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_next(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4,
	    &type, &permset, &tag, &qual, &name));
	assertEqualInt(2147483646, qual);
	archive_entry_acl_clear(ae);

	/* 7. Overflow in the name field (numeric name too large) → rejected */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae,
	    "user:99999999999:rw-p--a-R-c-o-:-------:allow",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	archive_entry_acl_clear(ae);

	archive_entry_free(ae);
}

/*
 * Malformed NFS4 ACL entries — each should return ARCHIVE_WARN without
 * crashing.  Also verifies that valid entries before an invalid one are
 * still stored.
 */
DEFINE_TEST(test_acl_nfs4_text_invalid)
{
	struct archive_entry *ae;
	int type, permset, tag, qual;
	const char *name;

	ae = archive_entry_new();
	assert(ae != NULL);

	/* Unknown tag */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae,
	    "bogus:name:rw-p--a-R-c-o-:-------:allow",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	archive_entry_acl_clear(ae);

	/* Unknown type string */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae,
	    "user:name:rw-p--a-R-c-o-:-------:bogus",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	archive_entry_acl_clear(ae);

	/* Invalid character in perms field */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae,
	    "user:name:rq-p--a-R-c-o-:-------:allow",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	archive_entry_acl_clear(ae);

	/* Invalid character in flags field */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae,
	    "user:name:rw-p--a-R-c-o-:q------:allow",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	archive_entry_acl_clear(ae);

	/* Just a tag with no colon-separated fields */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae, "user",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	archive_entry_acl_clear(ae);

	/* Valid entry followed by an invalid one: valid entry must be stored
	 * and ARCHIVE_WARN returned (not ARCHIVE_FATAL). */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text(ae,
	    "user:alice:rw-p--a-R-c-o-:-------:allow\n"
	    "bogus:bob:rw-p--a-R-c-o-:-------:deny",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(1,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(ARCHIVE_OK,
	    archive_entry_acl_next(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4,
	    &type, &permset, &tag, &qual, &name));
	assertEqualInt(ARCHIVE_ENTRY_ACL_TYPE_ALLOW, type);
	assertEqualString("alice", name);
	archive_entry_acl_clear(ae);

	/* Same tests via the wide parser */
	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text_w(ae,
	    L"bogus:name:rw-p--a-R-c-o-:-------:allow",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	archive_entry_acl_clear(ae);

	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text_w(ae,
	    L"user:name:rw-p--a-R-c-o-:-------:bogus",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	archive_entry_acl_clear(ae);

	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text_w(ae,
	    L"user:name:rq-p--a-R-c-o-:-------:allow",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	archive_entry_acl_clear(ae);

	assertEqualInt(ARCHIVE_WARN,
	    archive_entry_acl_from_text_w(ae,
	    L"user:name:rw-p--a-R-c-o-:q------:allow",
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	assertEqualInt(0,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	archive_entry_acl_clear(ae);

	archive_entry_free(ae);
}
