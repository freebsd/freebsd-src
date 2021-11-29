/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Alan Somers
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
 *
 * $FreeBSD$
 */

extern "C" {
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

/*
 * "Last Local Modify" bugs
 *
 * This file tests a class of race conditions caused by one thread fetching a
 * file's size with FUSE_LOOKUP while another thread simultaneously modifies it
 * with FUSE_SETATTR, FUSE_WRITE or similar.  It's
 * possible for the second thread to start later yet finish first. If that
 * happens, the first thread must not override the size set by the second
 * thread.
 *
 * FUSE_GETATTR is not vulnerable to the same race, because it is always called
 * with the vnode lock held.
 *
 * A few other operations like FUSE_LINK can also trigger the same race but
 * with the file's ctime instead of size.  However, the consequences of an
 * incorrect ctime are much less disastrous than an incorrect size, so fusefs
 * does not attempt to prevent such races.
 */

enum Mutator {
	VOP_SETATTR,
	VOP_WRITE,
};

/*
 * Translate a poll method's string representation to the enum value.
 * Using strings with ::testing::Values gives better output with
 * --gtest_list_tests
 */
enum Mutator writer_from_str(const char* s) {
	if (0 == strcmp("VOP_SETATTR", s))
		return VOP_SETATTR;
	else
		return VOP_WRITE;
}

uint32_t fuse_op_from_mutator(enum Mutator mutator) {
	switch(mutator) {
	case VOP_SETATTR:
		return(FUSE_SETATTR);
	case VOP_WRITE:
		return(FUSE_WRITE);
	}
}

class LastLocalModify: public FuseTest, public WithParamInterface<const char*> {
public:
virtual void SetUp() {
	m_init_flags = FUSE_EXPORT_SUPPORT;

	FuseTest::SetUp();
}
};

static void* setattr_th(void* arg) {
	int fd;
	ssize_t r;
	sem_t *sem = (sem_t*) arg;

	if (sem)
		sem_wait(sem);

	fd = open("mountpoint/some_file.txt", O_RDWR);
	if (fd < 0)
		return (void*)(intptr_t)errno;

	r = ftruncate(fd, 15);
	if (r >= 0)
		return 0;
	else
		return (void*)(intptr_t)errno;
}

static void* write_th(void* arg) {
	ssize_t r;
	int fd;
	sem_t *sem = (sem_t*) arg;
	const char BUF[] = "abcdefghijklmn";

	if (sem)
		sem_wait(sem);
	fd = open("mountpoint/some_file.txt", O_RDWR);
	if (fd < 0)
		return (void*)(intptr_t)errno;

	r = write(fd, BUF, sizeof(BUF));
	if (r >= 0) {
		LastLocalModify::leak(fd);
		return 0;
	} else
		return (void*)(intptr_t)errno;
}

/*
 * VOP_LOOKUP should discard attributes returned by the server if they were
 * modified by another VOP while the VOP_LOOKUP was in progress.
 *
 * Sequence of operations:
 * * Thread 1 calls a mutator like ftruncate, which acquires the vnode lock
 *   exclusively.
 * * Thread 2 calls stat, which does VOP_LOOKUP, which sends FUSE_LOOKUP to the
 *   server.  The server replies with the old file length.  Thread 2 blocks
 *   waiting for the vnode lock.
 * * Thread 1 sends the mutator operation like FUSE_SETATTR that changes the
 *   file's size and updates the attribute cache.  Then it releases the vnode
 *   lock.
 * * Thread 2 acquires the vnode lock.  At this point it must not add the
 *   now-stale file size to the attribute cache.
 *
 * Regression test for https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=259071
 */
TEST_P(LastLocalModify, lookup)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	Sequence seq;
	uint64_t ino = 3;
	uint64_t mutator_unique;
	const uint64_t oldsize = 10;
	const uint64_t newsize = 15;
	pthread_t th0;
	void *thr0_value;
	struct stat sb;
	static sem_t sem;
	Mutator mutator;
	uint32_t mutator_op;
	size_t mutator_size;

	mutator = writer_from_str(GetParam());
	mutator_op = fuse_op_from_mutator(mutator);

	ASSERT_EQ(0, sem_init(&sem, 0, 0)) << strerror(errno);

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.InSequence(seq)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		/* Called by the mutator, caches attributes but not entries */
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.nodeid = ino;
		out.body.entry.attr.size = oldsize;
		out.body.entry.nodeid = ino;
		out.body.entry.attr_valid_nsec = NAP_NS / 2;
		out.body.entry.attr.ino = ino;
		out.body.entry.attr.mode = S_IFREG | 0644;
	})));
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == mutator_op &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).InSequence(seq)
	.WillOnce(Invoke([&](auto in, auto &out __unused) {
		/*
		 * The mutator changes the file size, but in order to simulate
		 * a race, don't reply.  Instead, just save the unique for
		 * later.
		 */
		mutator_unique = in.header.unique;
		switch(mutator) {
		case VOP_WRITE:
			mutator_size = in.body.write.size;
			break;
		default:
			break;
		}
		/* Allow the lookup thread to proceed */
		sem_post(&sem);
	}));
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.InSequence(seq)
	.WillOnce(Invoke([&](auto in __unused, auto& out) {
		std::unique_ptr<mockfs_buf_out> out0(new mockfs_buf_out);
		std::unique_ptr<mockfs_buf_out> out1(new mockfs_buf_out);

		/* First complete the lookup request, returning the old size */
		out0->header.unique = in.header.unique;
		SET_OUT_HEADER_LEN(*out0, entry);
		out0->body.entry.attr.mode = S_IFREG | 0644;
		out0->body.entry.nodeid = ino;
		out0->body.entry.entry_valid = UINT64_MAX;
		out0->body.entry.attr_valid = UINT64_MAX;
		out0->body.entry.attr.size = oldsize;
		out.push_back(std::move(out0));

		/* Then, respond to the mutator request */
		out1->header.unique = mutator_unique;
		switch(mutator) {
		case VOP_SETATTR:
			SET_OUT_HEADER_LEN(*out1, attr);
			out1->body.attr.attr.ino = ino;
			out1->body.attr.attr.mode = S_IFREG | 0644;
			out1->body.attr.attr.size = newsize;	// Changed size
			out1->body.attr.attr_valid = UINT64_MAX;
			break;
		case VOP_WRITE:
			SET_OUT_HEADER_LEN(*out1, write);
			out1->body.write.size = mutator_size;
			break;
		}
		out.push_back(std::move(out1));
	}));

	/* Start the mutator thread */
	switch(mutator) {
	case VOP_SETATTR:
		ASSERT_EQ(0, pthread_create(&th0, NULL, setattr_th, NULL))
			<< strerror(errno);
		break;
	case VOP_WRITE:
		ASSERT_EQ(0, pthread_create(&th0, NULL, write_th, NULL))
			<< strerror(errno);
		break;
	}


	/* Wait for FUSE_SETATTR to be sent */
	sem_wait(&sem);

	/* Lookup again, which will race with setattr */
	ASSERT_EQ(0, stat(FULLPATH, &sb)) << strerror(errno);
	ASSERT_EQ((off_t)newsize, sb.st_size);

	/* ftruncate should've completed without error */
	pthread_join(th0, &thr0_value);
	EXPECT_EQ(0, (intptr_t)thr0_value);
}

/*
 * VFS_VGET should discard attributes returned by the server if they were
 * modified by another VOP while the VFS_VGET was in progress.
 *
 * Sequence of operations:
 * * Thread 1 calls fhstat, entering VFS_VGET, and issues FUSE_LOOKUP
 * * Thread 2 calls a mutator like ftruncate, which acquires the vnode lock
 *   exclusively and issues a FUSE op like FUSE_SETATTR.
 * * Thread 1's FUSE_LOOKUP returns with the old size, but the thread blocks
 *   waiting for the vnode lock.
 * * Thread 2's FUSE op returns, and that thread sets the file's new size
 *   in the attribute cache.  Finally it releases the vnode lock.
 * * The vnode lock acquired, thread 1 must not overwrite the attr cache's size
 *   with the old value.
 *
 * Regression test for https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=259071
 */
TEST_P(LastLocalModify, vfs_vget)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	Sequence seq;
	uint64_t ino = 3;
	uint64_t lookup_unique;
	const uint64_t oldsize = 10;
	const uint64_t newsize = 15;
	pthread_t th0;
	void *thr0_value;
	struct stat sb;
	static sem_t sem;
	fhandle_t fhp;
	Mutator mutator;
	uint32_t mutator_op;

	if (geteuid() != 0)
		GTEST_SKIP() << "This test requires a privileged user";

	mutator = writer_from_str(GetParam());
	mutator_op = fuse_op_from_mutator(mutator);

	ASSERT_EQ(0, sem_init(&sem, 0, 0)) << strerror(errno);

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.Times(1)
	.InSequence(seq)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out)
	{
		/* Called by getfh, caches attributes but not entries */
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.nodeid = ino;
		out.body.entry.attr.size = oldsize;
		out.body.entry.nodeid = ino;
		out.body.entry.attr_valid_nsec = NAP_NS / 2;
		out.body.entry.attr.ino = ino;
		out.body.entry.attr.mode = S_IFREG | 0644;
	})));
	EXPECT_LOOKUP(ino, ".")
	.InSequence(seq)
	.WillOnce(Invoke([&](auto in, auto &out __unused) {
		/* Called by fhstat.  Block to simulate a race */
		lookup_unique = in.header.unique;
		sem_post(&sem);
	}));

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.Times(1)
	.InSequence(seq)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto in __unused, auto& out)
	{
		/* Called by VOP_SETATTR, caches attributes but not entries */
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.nodeid = ino;
		out.body.entry.attr.size = oldsize;
		out.body.entry.nodeid = ino;
		out.body.entry.attr_valid_nsec = NAP_NS / 2;
		out.body.entry.attr.ino = ino;
		out.body.entry.attr.mode = S_IFREG | 0644;
	})));

	/* Called by the mutator thread */
	expect_open(ino, 0, 1);

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == mutator_op &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).InSequence(seq)
	.WillOnce(Invoke([&](auto in __unused, auto& out) {
		std::unique_ptr<mockfs_buf_out> out0(new mockfs_buf_out);
		std::unique_ptr<mockfs_buf_out> out1(new mockfs_buf_out);

		/* First complete the lookup request, returning the old size */
		out0->header.unique = lookup_unique;
		SET_OUT_HEADER_LEN(*out0, entry);
		out0->body.entry.attr.mode = S_IFREG | 0644;
		out0->body.entry.nodeid = ino;
		out0->body.entry.entry_valid = UINT64_MAX;
		out0->body.entry.attr_valid = UINT64_MAX;
		out0->body.entry.attr.size = oldsize;
		out.push_back(std::move(out0));

		/* Then, respond to the mutator request */
		out1->header.unique = in.header.unique;
		switch(mutator) {
		case VOP_SETATTR:
			SET_OUT_HEADER_LEN(*out1, attr);
			out1->body.attr.attr.ino = ino;
			out1->body.attr.attr.mode = S_IFREG | 0644;
			out1->body.attr.attr.size = newsize;	// Changed size
			out1->body.attr.attr_valid = UINT64_MAX;
			break;
		case VOP_WRITE:
			SET_OUT_HEADER_LEN(*out1, write);
			out1->body.write.size = in.body.write.size;
			break;
		}
		out.push_back(std::move(out1));
	}));

	/* First get a file handle */
	ASSERT_EQ(0, getfh(FULLPATH, &fhp)) << strerror(errno);

	/* Start the mutator thread */
	switch(mutator) {
	case VOP_SETATTR:
		ASSERT_EQ(0, pthread_create(&th0, NULL, setattr_th,
			(void*)&sem)) << strerror(errno);
		break;
	case VOP_WRITE:
		ASSERT_EQ(0, pthread_create(&th0, NULL, write_th, (void*)&sem))
			<< strerror(errno);
		break;
	}

	/* Lookup again, which will race with setattr */
	ASSERT_EQ(0, fhstat(&fhp, &sb)) << strerror(errno);

	ASSERT_EQ((off_t)newsize, sb.st_size);

	/* mutator should've completed without error */
	pthread_join(th0, &thr0_value);
	EXPECT_EQ(0, (intptr_t)thr0_value);
}


INSTANTIATE_TEST_CASE_P(LLM, LastLocalModify,
	Values("VOP_SETATTR", "VOP_WRITE")
);
