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
 */
#include "test.h"
__FBSDID("$FreeBSD$");

DEFINE_TEST(test_archive_api_feature)
{
	assertEqualInt(ARCHIVE_API_FEATURE, archive_api_feature());
	assertEqualInt(ARCHIVE_API_VERSION, archive_api_version());
	/*
	 * Even though ARCHIVE_VERSION_STAMP only appears in
	 * archive.h after 1.9.0 and 2.2.3, the macro is synthesized
	 * in test.h, so this test is always valid.
	 */
	assertEqualInt(ARCHIVE_VERSION_STAMP / 1000, ARCHIVE_API_VERSION * 1000 + ARCHIVE_API_FEATURE);
	/*
	 * The function, however, isn't always available.  It appeared
	 * sometime in the middle of 2.2.3, but the synthesized value
	 * never has a release version, so the following conditional
	 * exactly determines whether the current library has the
	 * function.
	 */
#if ARCHIVE_VERSION_STAMP / 1000 == 1009 || ARCHIVE_VERSION_STAMP > 2002000
	assertEqualInt(ARCHIVE_VERSION_STAMP, archive_version_stamp());
#else
	skipping("archive_version_stamp()");
#endif
	assertEqualString(ARCHIVE_LIBRARY_VERSION, archive_version());
}
