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

#include "../cpio.h"

DEFINE_TEST(test_owner_parse)
{
	int uid, gid;

	cpio_progname = "Ignore this message";

	assertEqualInt(0, owner_parse("root", &uid, &gid));
	assertEqualInt(0, uid);
	assertEqualInt(-1, gid);


	assertEqualInt(0, owner_parse("root:", &uid, &gid));
	assertEqualInt(0, uid);
	assertEqualInt(0, gid);

	assertEqualInt(0, owner_parse("root.", &uid, &gid));
	assertEqualInt(0, uid);
	assertEqualInt(0, gid);

	/*
	 * TODO: Lookup current user/group name, build strings and
	 * use those to verify username/groupname lookups for ordinary
	 * users.
	 */

	/*
	 * TODO: Rework owner_parse to either return a char * pointing
	 * to an error message or accept a function pointer to an
	 * error-reporting routine so that the following tests don't
	 * generate any output.
	 *
	 * Alternatively, redirect stderr temporarily to suppress the output.
	 */

	assertEqualInt(1, owner_parse(":nonexistentgroup", &uid, &gid));
	assertEqualInt(1, owner_parse("root:nonexistentgroup", &uid, &gid));
	assertEqualInt(1,
	    owner_parse("nonexistentuser:nonexistentgroup", &uid, &gid));
}
