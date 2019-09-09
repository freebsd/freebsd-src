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
 *
 * $FreeBSD$
 */

extern "C" {
#include <sys/types.h>
#include <sys/extattr.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

/* Initial size of files used by these tests */
const off_t FILESIZE = 1000;
/* Access mode used by all directories in these tests */
const mode_t MODE = 0755;
const char FULLDIRPATH0[] = "mountpoint/some_dir";
const char RELDIRPATH0[] = "some_dir";
const char FULLDIRPATH1[] = "mountpoint/other_dir";
const char RELDIRPATH1[] = "other_dir";

static sem_t *blocked_semaphore;
static sem_t *signaled_semaphore;

static bool killer_should_sleep = false;

/* Don't do anything; all we care about is that the syscall gets interrupted */
void sigusr2_handler(int __unused sig) {
	if (verbosity > 1) {
		printf("Signaled!  thread %p\n", pthread_self());
	}

}

void* killer(void* target) {
	/* Wait until the main thread is blocked in fdisp_wait_answ */
	if (killer_should_sleep)
		nap();
	else
		sem_wait(blocked_semaphore);
	if (verbosity > 1)
		printf("Signalling!  thread %p\n", target);
	pthread_kill((pthread_t)target, SIGUSR2);
	if (signaled_semaphore != NULL)
		sem_post(signaled_semaphore);

	return(NULL);
}

class Interrupt: public FuseTest {
public:
pthread_t m_child;

Interrupt(): m_child(NULL) {};

void expect_lookup(const char *relpath, uint64_t ino)
{
	FuseTest::expect_lookup(relpath, ino, S_IFREG | 0644, FILESIZE, 1);
}

/* 
 * Expect a FUSE_MKDIR but don't reply.  Instead, just record the unique value
 * to the provided pointer
 */
void expect_mkdir(uint64_t *mkdir_unique)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_MKDIR);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([=](auto in, auto &out __unused) {
		*mkdir_unique = in.header.unique;
		sem_post(blocked_semaphore);
	}));
}

/* 
 * Expect a FUSE_READ but don't reply.  Instead, just record the unique value
 * to the provided pointer
 */
void expect_read(uint64_t ino, uint64_t *read_unique)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READ &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([=](auto in, auto &out __unused) {
		*read_unique = in.header.unique;
		sem_post(blocked_semaphore);
	}));
}

/* 
 * Expect a FUSE_WRITE but don't reply.  Instead, just record the unique value
 * to the provided pointer
 */
void expect_write(uint64_t ino, uint64_t *write_unique)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_WRITE &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([=](auto in, auto &out __unused) {
		*write_unique = in.header.unique;
		sem_post(blocked_semaphore);
	}));
}

void setup_interruptor(pthread_t target, bool sleep = false)
{
	ASSERT_NE(SIG_ERR, signal(SIGUSR2, sigusr2_handler)) << strerror(errno);
	killer_should_sleep = sleep;
	ASSERT_EQ(0, pthread_create(&m_child, NULL, killer, (void*)target))
		<< strerror(errno);
}

void SetUp() {
	const int mprot = PROT_READ | PROT_WRITE;
	const int mflags = MAP_ANON | MAP_SHARED;

	signaled_semaphore = NULL;

	blocked_semaphore = (sem_t*)mmap(NULL, sizeof(*blocked_semaphore),
		mprot, mflags, -1, 0);
	ASSERT_NE(MAP_FAILED, blocked_semaphore) << strerror(errno);
	ASSERT_EQ(0, sem_init(blocked_semaphore, 1, 0)) << strerror(errno);
	ASSERT_EQ(0, siginterrupt(SIGUSR2, 1));

	FuseTest::SetUp();
}

void TearDown() {
	struct sigaction sa;

	if (m_child != NULL) {
		pthread_join(m_child, NULL);
	}
	bzero(&sa, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sigaction(SIGUSR2, &sa, NULL);

	sem_destroy(blocked_semaphore);
	munmap(blocked_semaphore, sizeof(*blocked_semaphore));

	FuseTest::TearDown();
}
};

class Intr: public Interrupt {};

class Nointr: public Interrupt {
	void SetUp() {
		m_nointr = true;
		Interrupt::SetUp();
	}
};

static void* mkdir0(void* arg __unused) {
	ssize_t r;

	r = mkdir(FULLDIRPATH0, MODE);
	if (r >= 0)
		return 0;
	else
		return (void*)(intptr_t)errno;
}

static void* read1(void* arg) {
	const size_t bufsize = FILESIZE;
	char buf[bufsize];
	int fd = (int)(intptr_t)arg;
	ssize_t r;

	r = read(fd, buf, bufsize);
	if (r >= 0)
		return 0;
	else
		return (void*)(intptr_t)errno;
}

/* 
 * An interrupt operation that gets received after the original command is
 * complete should generate an EAGAIN response.
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236530 */
TEST_F(Intr, already_complete)
{
	uint64_t ino = 42;
	pthread_t self;
	uint64_t mkdir_unique = 0;
	Sequence seq;

	self = pthread_self();

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH0)
	.InSequence(seq)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_mkdir(&mkdir_unique);
	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in.header.opcode == FUSE_INTERRUPT &&
				in.body.interrupt.unique == mkdir_unique);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([&](auto in, auto &out) {
		// First complete the mkdir request
		std::unique_ptr<mockfs_buf_out> out0(new mockfs_buf_out);
		out0->header.unique = mkdir_unique;
		SET_OUT_HEADER_LEN(*out0, entry);
		out0->body.create.entry.attr.mode = S_IFDIR | MODE;
		out0->body.create.entry.nodeid = ino;
		out.push_back(std::move(out0));

		// Then, respond EAGAIN to the interrupt request
		std::unique_ptr<mockfs_buf_out> out1(new mockfs_buf_out);
		out1->header.unique = in.header.unique;
		out1->header.error = -EAGAIN;
		out1->header.len = sizeof(out1->header);
		out.push_back(std::move(out1));
	}));
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH0)
	.InSequence(seq)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFDIR | MODE;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.nlink = 2;
	})));

	setup_interruptor(self);
	EXPECT_EQ(0, mkdir(FULLDIRPATH0, MODE)) << strerror(errno);
	/* 
	 * The final syscall simply ensures that the test's main thread doesn't
	 * end before the daemon finishes responding to the FUSE_INTERRUPT.
	 */
	EXPECT_EQ(0, access(FULLDIRPATH0, F_OK)) << strerror(errno);
}

/*
 * If a FUSE file system returns ENOSYS for a FUSE_INTERRUPT operation, the
 * kernel should not attempt to interrupt any other operations on that mount
 * point.
 */
TEST_F(Intr, enosys)
{
	uint64_t ino0 = 42, ino1 = 43;;
	uint64_t mkdir_unique;
	pthread_t self, th0;
	sem_t sem0, sem1;
	void *thr0_value;
	Sequence seq;

	self = pthread_self();
	ASSERT_EQ(0, sem_init(&sem0, 0, 0)) << strerror(errno);
	ASSERT_EQ(0, sem_init(&sem1, 0, 0)) << strerror(errno);

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH1)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH0)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_mkdir(&mkdir_unique);
	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in.header.opcode == FUSE_INTERRUPT &&
				in.body.interrupt.unique == mkdir_unique);
		}, Eq(true)),
		_)
	).InSequence(seq)
	.WillOnce(Invoke([&](auto in, auto &out) {
		// reject FUSE_INTERRUPT and respond to the FUSE_MKDIR
		std::unique_ptr<mockfs_buf_out> out0(new mockfs_buf_out);
		std::unique_ptr<mockfs_buf_out> out1(new mockfs_buf_out);

		out0->header.unique = in.header.unique;
		out0->header.error = -ENOSYS;
		out0->header.len = sizeof(out0->header);
		out.push_back(std::move(out0));

		SET_OUT_HEADER_LEN(*out1, entry);
		out1->body.create.entry.attr.mode = S_IFDIR | MODE;
		out1->body.create.entry.nodeid = ino1;
		out1->header.unique = mkdir_unique;
		out.push_back(std::move(out1));
	}));
	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in.header.opcode == FUSE_MKDIR);
		}, Eq(true)),
		_)
	).InSequence(seq)
	.WillOnce(Invoke([&](auto in, auto &out) {
		std::unique_ptr<mockfs_buf_out> out0(new mockfs_buf_out);

		sem_post(&sem0);
		sem_wait(&sem1);

		SET_OUT_HEADER_LEN(*out0, entry);
		out0->body.create.entry.attr.mode = S_IFDIR | MODE;
		out0->body.create.entry.nodeid = ino0;
		out0->header.unique = in.header.unique;
		out.push_back(std::move(out0));
	}));

	setup_interruptor(self);
	/* First mkdir operation should finish synchronously */
	ASSERT_EQ(0, mkdir(FULLDIRPATH1, MODE)) << strerror(errno);

	ASSERT_EQ(0, pthread_create(&th0, NULL, mkdir0, NULL))
		<< strerror(errno);

	sem_wait(&sem0);
	/*
	 * th0 should be blocked waiting for the fuse daemon thread.
	 * Signal it.  No FUSE_INTERRUPT should result
	 */
	pthread_kill(th0, SIGUSR1);
	/* Allow the daemon thread to proceed */
	sem_post(&sem1);
	pthread_join(th0, &thr0_value);
	/* Second mkdir should've finished without error */
	EXPECT_EQ(0, (intptr_t)thr0_value);
}

/*
 * A FUSE filesystem is legally allowed to ignore INTERRUPT operations, and
 * complete the original operation whenever it damn well pleases.
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236530 */
TEST_F(Intr, ignore)
{
	uint64_t ino = 42;
	pthread_t self;
	uint64_t mkdir_unique;

	self = pthread_self();

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH0)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_mkdir(&mkdir_unique);
	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in.header.opcode == FUSE_INTERRUPT &&
				in.body.interrupt.unique == mkdir_unique);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([&](auto in __unused, auto &out) {
		// Ignore FUSE_INTERRUPT; respond to the FUSE_MKDIR
		std::unique_ptr<mockfs_buf_out> out0(new mockfs_buf_out);
		out0->header.unique = mkdir_unique;
		SET_OUT_HEADER_LEN(*out0, entry);
		out0->body.create.entry.attr.mode = S_IFDIR | MODE;
		out0->body.create.entry.nodeid = ino;
		out.push_back(std::move(out0));
	}));

	setup_interruptor(self);
	ASSERT_EQ(0, mkdir(FULLDIRPATH0, MODE)) << strerror(errno);
}

/*
 * A restartable operation (basically, anything except write or setextattr)
 * that hasn't yet been sent to userland can be interrupted without sending
 * FUSE_INTERRUPT, and will be automatically restarted.
 */
TEST_F(Intr, in_kernel_restartable)
{
	const char FULLPATH1[] = "mountpoint/other_file.txt";
	const char RELPATH1[] = "other_file.txt";
	uint64_t ino0 = 42, ino1 = 43;
	int fd1;
	pthread_t self, th0, th1;
	sem_t sem0, sem1;
	void *thr0_value, *thr1_value;

	ASSERT_EQ(0, sem_init(&sem0, 0, 0)) << strerror(errno);
	ASSERT_EQ(0, sem_init(&sem1, 0, 0)) << strerror(errno);
	self = pthread_self();

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH0)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_lookup(RELPATH1, ino1);
	expect_open(ino1, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_MKDIR);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([&](auto in __unused, auto& out) {
		/* Let the next write proceed */
		sem_post(&sem1);
		/* Pause the daemon thread so it won't read the next op */
		sem_wait(&sem0);

		SET_OUT_HEADER_LEN(out, entry);
		out.body.create.entry.attr.mode = S_IFDIR | MODE;
		out.body.create.entry.nodeid = ino0;
	})));
	FuseTest::expect_read(ino1, 0, FILESIZE, 0, NULL);

	fd1 = open(FULLPATH1, O_RDONLY);
	ASSERT_LE(0, fd1) << strerror(errno);

	/* Use a separate thread for each operation */
	ASSERT_EQ(0, pthread_create(&th0, NULL, mkdir0, NULL))
		<< strerror(errno);

	sem_wait(&sem1);	/* Sequence the two operations */

	ASSERT_EQ(0, pthread_create(&th1, NULL, read1, (void*)(intptr_t)fd1))
		<< strerror(errno);

	setup_interruptor(self, true);

	pause();		/* Wait for signal */

	/* Unstick the daemon */
	ASSERT_EQ(0, sem_post(&sem0)) << strerror(errno);

	/* Wait awhile to make sure the signal generates no FUSE_INTERRUPT */
	nap();

	pthread_join(th1, &thr1_value);
	pthread_join(th0, &thr0_value);
	EXPECT_EQ(0, (intptr_t)thr1_value);
	EXPECT_EQ(0, (intptr_t)thr0_value);
	sem_destroy(&sem1);
	sem_destroy(&sem0);

	leak(fd1);
}

/*
 * An operation that hasn't yet been sent to userland can be interrupted
 * without sending FUSE_INTERRUPT.  If it's a non-restartable operation (write
 * or setextattr) it will return EINTR.
 */
TEST_F(Intr, in_kernel_nonrestartable)
{
	const char FULLPATH1[] = "mountpoint/other_file.txt";
	const char RELPATH1[] = "other_file.txt";
	const char value[] = "whatever";
	ssize_t value_len = strlen(value) + 1;
	uint64_t ino0 = 42, ino1 = 43;
	int ns = EXTATTR_NAMESPACE_USER;
	int fd1;
	pthread_t self, th0;
	sem_t sem0, sem1;
	void *thr0_value;
	ssize_t r;

	ASSERT_EQ(0, sem_init(&sem0, 0, 0)) << strerror(errno);
	ASSERT_EQ(0, sem_init(&sem1, 0, 0)) << strerror(errno);
	self = pthread_self();

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH0)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_lookup(RELPATH1, ino1);
	expect_open(ino1, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_MKDIR);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([&](auto in __unused, auto& out) {
		/* Let the next write proceed */
		sem_post(&sem1);
		/* Pause the daemon thread so it won't read the next op */
		sem_wait(&sem0);

		SET_OUT_HEADER_LEN(out, entry);
		out.body.create.entry.attr.mode = S_IFDIR | MODE;
		out.body.create.entry.nodeid = ino0;
	})));

	fd1 = open(FULLPATH1, O_WRONLY);
	ASSERT_LE(0, fd1) << strerror(errno);

	/* Use a separate thread for the first write */
	ASSERT_EQ(0, pthread_create(&th0, NULL, mkdir0, NULL))
		<< strerror(errno);

	sem_wait(&sem1);	/* Sequence the two operations */

	setup_interruptor(self, true);

	r = extattr_set_fd(fd1, ns, "foo", (const void*)value, value_len);
	EXPECT_NE(0, r);
	EXPECT_EQ(EINTR, errno);

	/* Unstick the daemon */
	ASSERT_EQ(0, sem_post(&sem0)) << strerror(errno);

	/* Wait awhile to make sure the signal generates no FUSE_INTERRUPT */
	nap();

	pthread_join(th0, &thr0_value);
	EXPECT_EQ(0, (intptr_t)thr0_value);
	sem_destroy(&sem1);
	sem_destroy(&sem0);

	leak(fd1);
}

/* 
 * A syscall that gets interrupted while blocking on FUSE I/O should send a
 * FUSE_INTERRUPT command to the fuse filesystem, which should then send EINTR
 * in response to the _original_ operation.  The kernel should ultimately
 * return EINTR to userspace
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236530 */
TEST_F(Intr, in_progress)
{
	pthread_t self;
	uint64_t mkdir_unique;

	self = pthread_self();

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH0)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_mkdir(&mkdir_unique);
	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in.header.opcode == FUSE_INTERRUPT &&
				in.body.interrupt.unique == mkdir_unique);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([&](auto in __unused, auto &out) {
		std::unique_ptr<mockfs_buf_out> out0(new mockfs_buf_out);
		out0->header.error = -EINTR;
		out0->header.unique = mkdir_unique;
		out0->header.len = sizeof(out0->header);
		out.push_back(std::move(out0));
	}));

	setup_interruptor(self);
	ASSERT_EQ(-1, mkdir(FULLDIRPATH0, MODE));
	EXPECT_EQ(EINTR, errno);
}

/* Reads should also be interruptible */
TEST_F(Intr, in_progress_read)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const size_t bufsize = 80;
	char buf[bufsize];
	uint64_t ino = 42;
	int fd;
	pthread_t self;
	uint64_t read_unique;

	self = pthread_self();

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_read(ino, &read_unique);
	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in.header.opcode == FUSE_INTERRUPT &&
				in.body.interrupt.unique == read_unique);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([&](auto in __unused, auto &out) {
		std::unique_ptr<mockfs_buf_out> out0(new mockfs_buf_out);
		out0->header.error = -EINTR;
		out0->header.unique = read_unique;
		out0->header.len = sizeof(out0->header);
		out.push_back(std::move(out0));
	}));

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	setup_interruptor(self);
	ASSERT_EQ(-1, read(fd, buf, bufsize));
	EXPECT_EQ(EINTR, errno);

	leak(fd);
}

/*
 * When mounted with -o nointr, fusefs will block signals while waiting for the
 * server.
 */
TEST_F(Nointr, block)
{
	uint64_t ino = 42;
	pthread_t self;
	sem_t sem0;

	ASSERT_EQ(0, sem_init(&sem0, 0, 0)) << strerror(errno);
	signaled_semaphore = &sem0;
	self = pthread_self();

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH0)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_MKDIR);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([&](auto in __unused, auto& out) {
		/* Let the killer proceed */
		sem_post(blocked_semaphore);

		/* Wait until after the signal has been sent */
		sem_wait(signaled_semaphore);
		/* Allow time for the mkdir thread to receive the signal */
		nap();

		/* Finally, complete the original op */
		SET_OUT_HEADER_LEN(out, entry);
		out.body.create.entry.attr.mode = S_IFDIR | MODE;
		out.body.create.entry.nodeid = ino;
	})));
	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in.header.opcode == FUSE_INTERRUPT);
		}, Eq(true)),
		_)
	).Times(0);

	setup_interruptor(self);
	ASSERT_EQ(0, mkdir(FULLDIRPATH0, MODE)) << strerror(errno);

	sem_destroy(&sem0);
}

/* FUSE_INTERRUPT operations should take priority over other pending ops */
TEST_F(Intr, priority)
{
	Sequence seq;
	uint64_t ino1 = 43;
	uint64_t mkdir_unique;
	pthread_t th0;
	sem_t sem0, sem1;

	ASSERT_EQ(0, sem_init(&sem0, 0, 0)) << strerror(errno);
	ASSERT_EQ(0, sem_init(&sem1, 0, 0)) << strerror(errno);

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH0)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH1)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_MKDIR);
		}, Eq(true)),
		_)
	).InSequence(seq)
	.WillOnce(Invoke(ReturnImmediate([&](auto in, auto& out) {
		mkdir_unique = in.header.unique;

		/* Let the next mkdir proceed */
		sem_post(&sem1);

		/* Pause the daemon thread so it won't read the next op */
		sem_wait(&sem0);

		/* Finally, interrupt the original op */
		out.header.error = -EINTR;
		out.header.unique = mkdir_unique;
		out.header.len = sizeof(out.header);
	})));
	/* 
	 * FUSE_INTERRUPT should be received before the second FUSE_MKDIR,
	 * even though it was generated later
	 */
	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in.header.opcode == FUSE_INTERRUPT &&
				in.body.interrupt.unique == mkdir_unique);
		}, Eq(true)),
		_)
	).InSequence(seq)
	.WillOnce(Invoke(ReturnErrno(EAGAIN)));
	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in.header.opcode == FUSE_MKDIR);
		}, Eq(true)),
		_)
	).InSequence(seq)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.create.entry.attr.mode = S_IFDIR | MODE;
		out.body.create.entry.nodeid = ino1;
	})));

	/* Use a separate thread for the first mkdir */
	ASSERT_EQ(0, pthread_create(&th0, NULL, mkdir0, NULL))
		<< strerror(errno);

	signaled_semaphore = &sem0;

	sem_wait(&sem1);	/* Sequence the two mkdirs */
	setup_interruptor(th0, true);
	ASSERT_EQ(0, mkdir(FULLDIRPATH1, MODE)) << strerror(errno);

	pthread_join(th0, NULL);
	sem_destroy(&sem1);
	sem_destroy(&sem0);
}

/*
 * If the FUSE filesystem receives the FUSE_INTERRUPT operation before
 * processing the original, then it should wait for "some timeout" for the
 * original operation to arrive.  If not, it should send EAGAIN to the
 * INTERRUPT operation, and the kernel should requeue the INTERRUPT.
 *
 * In this test, we'll pretend that the INTERRUPT arrives too soon, gets
 * EAGAINed, then the kernel requeues it, and the second time around it
 * successfully interrupts the original
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236530 */
TEST_F(Intr, too_soon)
{
	Sequence seq;
	pthread_t self;
	uint64_t mkdir_unique;

	self = pthread_self();

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH0)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_mkdir(&mkdir_unique);

	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in.header.opcode == FUSE_INTERRUPT &&
				in.body.interrupt.unique == mkdir_unique);
		}, Eq(true)),
		_)
	).InSequence(seq)
	.WillOnce(Invoke(ReturnErrno(EAGAIN)));

	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in.header.opcode == FUSE_INTERRUPT &&
				in.body.interrupt.unique == mkdir_unique);
		}, Eq(true)),
		_)
	).InSequence(seq)
	.WillOnce(Invoke([&](auto in __unused, auto &out __unused) {
		std::unique_ptr<mockfs_buf_out> out0(new mockfs_buf_out);
		out0->header.error = -EINTR;
		out0->header.unique = mkdir_unique;
		out0->header.len = sizeof(out0->header);
		out.push_back(std::move(out0));
	}));

	setup_interruptor(self);
	ASSERT_EQ(-1, mkdir(FULLDIRPATH0, MODE));
	EXPECT_EQ(EINTR, errno);
}


// TODO: add a test where write returns EWOULDBLOCK
