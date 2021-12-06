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
#include <sys/stat.h>

#include <fcntl.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Setattr : public FuseTest {};

class RofsSetattr: public Setattr {
public:
virtual void SetUp() {
	m_ro = true;
	Setattr::SetUp();
}
};

class Setattr_7_8: public Setattr {
public:
virtual void SetUp() {
	m_kernel_minor_version = 8;
	Setattr::SetUp();
}
};


/*
 * If setattr returns a non-zero cache timeout, then subsequent VOP_GETATTRs
 * should use the cached attributes, rather than query the daemon
 */
TEST_F(Setattr, attr_cache)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	struct stat sb;
	const mode_t newmode = 0644;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = ino;
		out.body.entry.entry_valid = UINT64_MAX;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | newmode;
		out.body.attr.attr_valid = UINT64_MAX;
	})));
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_GETATTR);
		}, Eq(true)),
		_)
	).Times(0);

	/* Set an attribute with SETATTR */
	ASSERT_EQ(0, chmod(FULLPATH, newmode)) << strerror(errno);

	/* The stat(2) should use cached attributes */
	ASSERT_EQ(0, stat(FULLPATH, &sb));
	EXPECT_EQ(S_IFREG | newmode, sb.st_mode);
}

/* Change the mode of a file */
TEST_F(Setattr, chmod)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t oldmode = 0755;
	const mode_t newmode = 0644;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | oldmode;
		out.body.entry.nodeid = ino;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			uint32_t valid = FATTR_MODE;
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid &&
				in.body.setattr.mode == newmode);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | newmode;
	})));
	EXPECT_EQ(0, chmod(FULLPATH, newmode)) << strerror(errno);
}

/* 
 * Chmod a multiply-linked file with cached attributes.  Check that both files'
 * attributes have changed.
 */
TEST_F(Setattr, chmod_multiply_linked)
{
	const char FULLPATH0[] = "mountpoint/some_file.txt";
	const char RELPATH0[] = "some_file.txt";
	const char FULLPATH1[] = "mountpoint/other_file.txt";
	const char RELPATH1[] = "other_file.txt";
	struct stat sb;
	const uint64_t ino = 42;
	const mode_t oldmode = 0777;
	const mode_t newmode = 0666;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH0)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | oldmode;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.nlink = 2;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH1)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | oldmode;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.nlink = 2;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			uint32_t valid = FATTR_MODE;
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid &&
				in.body.setattr.mode == newmode);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;
		out.body.attr.attr.mode = S_IFREG | newmode;
		out.body.attr.attr.nlink = 2;
		out.body.attr.attr_valid = UINT64_MAX;
	})));

	/* For a lookup of the 2nd file to get it into the cache*/
	ASSERT_EQ(0, stat(FULLPATH1, &sb)) << strerror(errno);
	EXPECT_EQ(S_IFREG | oldmode, sb.st_mode);

	ASSERT_EQ(0, chmod(FULLPATH0, newmode)) << strerror(errno);
	ASSERT_EQ(0, stat(FULLPATH0, &sb)) << strerror(errno);
	EXPECT_EQ(S_IFREG | newmode, sb.st_mode);
	ASSERT_EQ(0, stat(FULLPATH1, &sb)) << strerror(errno);
	EXPECT_EQ(S_IFREG | newmode, sb.st_mode);
}


/* Change the owner and group of a file */
TEST_F(Setattr, chown)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const gid_t oldgroup = 66;
	const gid_t newgroup = 99;
	const uid_t olduser = 33;
	const uid_t newuser = 44;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.gid = oldgroup;
		out.body.entry.attr.uid = olduser;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			uint32_t valid = FATTR_GID | FATTR_UID;
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid &&
				in.body.setattr.uid == newuser &&
				in.body.setattr.gid == newgroup);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | 0644;
		out.body.attr.attr.uid = newuser;
		out.body.attr.attr.gid = newgroup;
	})));
	EXPECT_EQ(0, chown(FULLPATH, newuser, newgroup)) << strerror(errno);
}



/* 
 * FUSE daemons are allowed to check permissions however they like.  If the
 * daemon returns EPERM, even if the file permissions "should" grant access,
 * then fuse(4) should return EPERM too.
 */
TEST_F(Setattr, eperm)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0777;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.uid = in.header.uid;
		out.body.entry.attr.gid = in.header.gid;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EPERM)));
	EXPECT_NE(0, truncate(FULLPATH, 10));
	EXPECT_EQ(EPERM, errno);
}

/* Change the mode of an open file, by its file descriptor */
TEST_F(Setattr, fchmod)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;
	const mode_t oldmode = 0755;
	const mode_t newmode = 0644;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | oldmode;
		out.body.entry.nodeid = ino;
		out.body.entry.attr_valid = UINT64_MAX;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_OPEN &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		out.header.len = sizeof(out.header);
		SET_OUT_HEADER_LEN(out, open);
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			uint32_t valid = FATTR_MODE;
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid &&
				in.body.setattr.mode == newmode);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | newmode;
	})));

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, fchmod(fd, newmode)) << strerror(errno);
	leak(fd);
}

/* Change the size of an open file, by its file descriptor */
TEST_F(Setattr, ftruncate)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;
	uint64_t fh = 0xdeadbeef1a7ebabe;
	const off_t oldsize = 99;
	const off_t newsize = 12345;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0755;
		out.body.entry.nodeid = ino;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.attr.size = oldsize;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_OPEN &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		out.header.len = sizeof(out.header);
		SET_OUT_HEADER_LEN(out, open);
		out.body.open.fh = fh;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			uint32_t valid = FATTR_SIZE | FATTR_FH;
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid &&
				in.body.setattr.fh == fh);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | 0755;
		out.body.attr.attr.size = newsize;
	})));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, ftruncate(fd, newsize)) << strerror(errno);
	leak(fd);
}

/* Change the size of the file */
TEST_F(Setattr, truncate) {
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const uint64_t oldsize = 100'000'000;
	const uint64_t newsize = 20'000'000;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.size = oldsize;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			uint32_t valid = FATTR_SIZE;
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid &&
				in.body.setattr.size == newsize);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | 0644;
		out.body.attr.attr.size = newsize;
	})));
	EXPECT_EQ(0, truncate(FULLPATH, newsize)) << strerror(errno);
}

/*
 * Truncating a file should discard cached data past the truncation point.
 * This is a regression test for bug 233783.
 *
 * There are two distinct failure modes.  The first one is a failure to zero
 * the portion of the file's final buffer past EOF.  It can be reproduced by
 * fsx -WR -P /tmp -S10 fsx.bin
 *
 * The second is a failure to drop buffers beyond that.  It can be reproduced by
 * fsx -WR -P /tmp -S18 -n fsx.bin
 * Also reproducible in sh with:
 * $> /path/to/libfuse/build/example/passthrough -d /tmp/mnt
 * $> cd /tmp/mnt/tmp
 * $> dd if=/dev/random of=randfile bs=1k count=192
 * $> truncate -s 1k randfile && truncate -s 192k randfile
 * $> xxd randfile | less # xxd will wrongly show random data at offset 0x8000
 */
TEST_F(Setattr, truncate_discards_cached_data) {
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	void *w0buf, *r0buf, *r1buf, *expected;
	off_t w0_offset = 0;
	size_t w0_size = 0x30000;
	off_t r0_offset = 0;
	off_t r0_size = w0_size;
	size_t trunc0_size = 0x400;
	size_t trunc1_size = w0_size;
	off_t r1_offset = trunc0_size;
	off_t r1_size = w0_size - trunc0_size;
	size_t cur_size = 0;
	const uint64_t ino = 42;
	mode_t mode = S_IFREG | 0644;
	int fd, r;
	bool should_have_data = false;

	w0buf = malloc(w0_size);
	ASSERT_NE(nullptr, w0buf) << strerror(errno);
	memset(w0buf, 'X', w0_size);

	r0buf = malloc(r0_size);
	ASSERT_NE(nullptr, r0buf) << strerror(errno);
	r1buf = malloc(r1_size);
	ASSERT_NE(nullptr, r1buf) << strerror(errno);

	expected = malloc(r1_size);
	ASSERT_NE(nullptr, expected) << strerror(errno);
	memset(expected, 0, r1_size);

	expect_lookup(RELPATH, ino, mode, 0, 1);
	expect_open(ino, O_RDWR, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([&](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;
		out.body.attr.attr.mode = mode;
		out.body.attr.attr.size = cur_size;
	})));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_WRITE);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([&](auto in, auto& out) {
		SET_OUT_HEADER_LEN(out, write);
		out.body.attr.attr.ino = ino;
		out.body.write.size = in.body.write.size;
		cur_size = std::max(static_cast<uint64_t>(cur_size),
			in.body.write.size + in.body.write.offset);
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				(in.body.setattr.valid & FATTR_SIZE));
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([&](auto in, auto& out) {
		auto trunc_size = in.body.setattr.size;
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;
		out.body.attr.attr.mode = mode;
		out.body.attr.attr.size = trunc_size;
		cur_size = trunc_size;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READ);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([&](auto in, auto& out) {
		auto osize = std::min(
			static_cast<uint64_t>(cur_size) - in.body.read.offset,
			static_cast<uint64_t>(in.body.read.size));
		out.header.len = sizeof(struct fuse_out_header) + osize;
		if (should_have_data)
			memset(out.body.bytes, 'X', osize);
		else
			bzero(out.body.bytes, osize);
	})));

	fd = open(FULLPATH, O_RDWR, 0644);
	ASSERT_LE(0, fd) << strerror(errno);

	/* Fill the file with Xs */
	ASSERT_EQ(static_cast<ssize_t>(w0_size),
		pwrite(fd, w0buf, w0_size, w0_offset));
	should_have_data = true;
	/* Fill the cache */
	ASSERT_EQ(static_cast<ssize_t>(r0_size),
		pread(fd, r0buf, r0_size, r0_offset));
	/* 1st truncate should discard cached data */
	EXPECT_EQ(0, ftruncate(fd, trunc0_size)) << strerror(errno);
	should_have_data = false;
	/* 2nd truncate extends file into previously cached data */
	EXPECT_EQ(0, ftruncate(fd, trunc1_size)) << strerror(errno);
	/* Read should return all zeros */
	ASSERT_EQ(static_cast<ssize_t>(r1_size),
		pread(fd, r1buf, r1_size, r1_offset));

	r = memcmp(expected, r1buf, r1_size);
	ASSERT_EQ(0, r);

	free(expected);
	free(r1buf);
	free(r0buf);
	free(w0buf);

	leak(fd);
}

/* Change a file's timestamps */
TEST_F(Setattr, utimensat) {
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const timespec oldtimes[2] = {
		{.tv_sec = 1, .tv_nsec = 2},
		{.tv_sec = 3, .tv_nsec = 4},
	};
	const timespec newtimes[2] = {
		{.tv_sec = 5, .tv_nsec = 6},
		{.tv_sec = 7, .tv_nsec = 8},
	};

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = ino;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.attr.atime = oldtimes[0].tv_sec;
		out.body.entry.attr.atimensec = oldtimes[0].tv_nsec;
		out.body.entry.attr.mtime = oldtimes[1].tv_sec;
		out.body.entry.attr.mtimensec = oldtimes[1].tv_nsec;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			uint32_t valid = FATTR_ATIME | FATTR_MTIME;
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid &&
				(time_t)in.body.setattr.atime ==
					newtimes[0].tv_sec &&
				(long)in.body.setattr.atimensec ==
					newtimes[0].tv_nsec &&
				(time_t)in.body.setattr.mtime ==
					newtimes[1].tv_sec &&
				(long)in.body.setattr.mtimensec ==
					newtimes[1].tv_nsec);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | 0644;
		out.body.attr.attr.atime = newtimes[0].tv_sec;
		out.body.attr.attr.atimensec = newtimes[0].tv_nsec;
		out.body.attr.attr.mtime = newtimes[1].tv_sec;
		out.body.attr.attr.mtimensec = newtimes[1].tv_nsec;
	})));
	EXPECT_EQ(0, utimensat(AT_FDCWD, FULLPATH, &newtimes[0], 0))
		<< strerror(errno);
}

/* Change a file mtime but not its atime */
TEST_F(Setattr, utimensat_mtime_only) {
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const timespec oldtimes[2] = {
		{.tv_sec = 1, .tv_nsec = 2},
		{.tv_sec = 3, .tv_nsec = 4},
	};
	const timespec newtimes[2] = {
		{.tv_sec = 5, .tv_nsec = UTIME_OMIT},
		{.tv_sec = 7, .tv_nsec = 8},
	};

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = ino;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.attr.atime = oldtimes[0].tv_sec;
		out.body.entry.attr.atimensec = oldtimes[0].tv_nsec;
		out.body.entry.attr.mtime = oldtimes[1].tv_sec;
		out.body.entry.attr.mtimensec = oldtimes[1].tv_nsec;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			uint32_t valid = FATTR_MTIME;
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid &&
				(time_t)in.body.setattr.mtime ==
					newtimes[1].tv_sec &&
				(long)in.body.setattr.mtimensec ==
					newtimes[1].tv_nsec);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | 0644;
		out.body.attr.attr.atime = oldtimes[0].tv_sec;
		out.body.attr.attr.atimensec = oldtimes[0].tv_nsec;
		out.body.attr.attr.mtime = newtimes[1].tv_sec;
		out.body.attr.attr.mtimensec = newtimes[1].tv_nsec;
	})));
	EXPECT_EQ(0, utimensat(AT_FDCWD, FULLPATH, &newtimes[0], 0))
		<< strerror(errno);
}

/*
 * Set a file's mtime and atime to now
 *
 * The design of FreeBSD's VFS does not allow fusefs to set just one of atime
 * or mtime to UTIME_NOW; it's both or neither.
 */
TEST_F(Setattr, utimensat_utime_now) {
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const timespec oldtimes[2] = {
		{.tv_sec = 1, .tv_nsec = 2},
		{.tv_sec = 3, .tv_nsec = 4},
	};
	const timespec newtimes[2] = {
		{.tv_sec = 0, .tv_nsec = UTIME_NOW},
		{.tv_sec = 0, .tv_nsec = UTIME_NOW},
	};
	/* "now" is whatever the server says it is */
	const timespec now[2] = {
		{.tv_sec = 5, .tv_nsec = 7},
		{.tv_sec = 6, .tv_nsec = 8},
	};
	struct stat sb;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = ino;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
		out.body.entry.attr.atime = oldtimes[0].tv_sec;
		out.body.entry.attr.atimensec = oldtimes[0].tv_nsec;
		out.body.entry.attr.mtime = oldtimes[1].tv_sec;
		out.body.entry.attr.mtimensec = oldtimes[1].tv_nsec;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			uint32_t valid = FATTR_ATIME | FATTR_ATIME_NOW |
				FATTR_MTIME | FATTR_MTIME_NOW;
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | 0644;
		out.body.attr.attr.atime = now[0].tv_sec;
		out.body.attr.attr.atimensec = now[0].tv_nsec;
		out.body.attr.attr.mtime = now[1].tv_sec;
		out.body.attr.attr.mtimensec = now[1].tv_nsec;
		out.body.attr.attr_valid = UINT64_MAX;
	})));
	ASSERT_EQ(0, utimensat(AT_FDCWD, FULLPATH, &newtimes[0], 0))
		<< strerror(errno);
	ASSERT_EQ(0, stat(FULLPATH, &sb)) << strerror(errno);
	EXPECT_EQ(now[0].tv_sec, sb.st_atim.tv_sec);
	EXPECT_EQ(now[0].tv_nsec, sb.st_atim.tv_nsec);
	EXPECT_EQ(now[1].tv_sec, sb.st_mtim.tv_sec);
	EXPECT_EQ(now[1].tv_nsec, sb.st_mtim.tv_nsec);
}

/*
 * FUSE_SETATTR returns a different file type, even though the entry cache
 * hasn't expired.  This is a server bug!  It probably means that the server
 * removed the file and recreated it with the same inode but a different vtyp.
 * The best thing fusefs can do is return ENOENT to the caller.  After all, the
 * entry must not have existed recently.
 */
TEST_F(Setattr, vtyp_conflict)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	uid_t newuser = 12345;
	sem_t sem;

	ASSERT_EQ(0, sem_init(&sem, 0, 0)) << strerror(errno);

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0777;
		out.body.entry.nodeid = ino;
		out.body.entry.entry_valid = UINT64_MAX;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;
		out.body.attr.attr.mode = S_IFDIR | 0777;	// Changed!
		out.body.attr.attr.uid = newuser;
	})));
	// We should reclaim stale vnodes
	expect_forget(ino, 1, &sem);

	EXPECT_NE(0, chown(FULLPATH, newuser, -1));
	EXPECT_EQ(ENOENT, errno);

	sem_wait(&sem);
	sem_destroy(&sem);
}

/* On a read-only mount, no attributes may be changed */
TEST_F(RofsSetattr, erofs)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t oldmode = 0755;
	const mode_t newmode = 0644;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | oldmode;
		out.body.entry.nodeid = ino;
	})));

	ASSERT_EQ(-1, chmod(FULLPATH, newmode));
	ASSERT_EQ(EROFS, errno);
}

/* Change the mode of a file */
TEST_F(Setattr_7_8, chmod)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t oldmode = 0755;
	const mode_t newmode = 0644;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry_7_8);
		out.body.entry.attr.mode = S_IFREG | oldmode;
		out.body.entry.nodeid = ino;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			uint32_t valid = FATTR_MODE;
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid &&
				in.body.setattr.mode == newmode);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr_7_8);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | newmode;
	})));
	EXPECT_EQ(0, chmod(FULLPATH, newmode)) << strerror(errno);
}
