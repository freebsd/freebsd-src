/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 The FreeBSD Foundation
 *
 * This software was developed by BFF Storage Systems, LLC under sponsorship
 * from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Destroy: public FuseTest {};

/*
 * On unmount the kernel should send a FUSE_DESTROY operation.  It should also
 * send FUSE_FORGET operations for all inodes with lookup_count > 0.
 */
TEST_F(Destroy, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 2);
	expect_forget(FUSE_ROOT_ID, 1);
	expect_forget(ino, 2);
	expect_destroy(0);

	/*
	 * access(2) the file to force a lookup.  Access it twice to double its
	 * lookup count.
	 */
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);

	/*
	 * Unmount, triggering a FUSE_DESTROY and also causing a VOP_RECLAIM
	 * for every vnode on this mp, triggering FUSE_FORGET for each of them.
	 */
	m_mock->unmount();
}
