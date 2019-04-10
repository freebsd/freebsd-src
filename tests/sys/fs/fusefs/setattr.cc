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

	EXPECT_LOOKUP(1, RELPATH)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.nodeid = ino;
		out->body.entry.entry_valid = UINT64_MAX;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in->header.opcode == FUSE_SETATTR &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;	// Must match nodeid
		out->body.attr.attr.mode = S_IFREG | newmode;
		out->body.attr.attr_valid = UINT64_MAX;
	})));
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in->header.opcode == FUSE_GETATTR);
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

	EXPECT_LOOKUP(1, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | oldmode;
		out->body.entry.nodeid = ino;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			/* In protocol 7.23, ctime will be changed too */
			uint32_t valid = FATTR_MODE;
			return (in->header.opcode == FUSE_SETATTR &&
				in->header.nodeid == ino &&
				in->body.setattr.valid == valid &&
				in->body.setattr.mode == newmode);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;	// Must match nodeid
		out->body.attr.attr.mode = S_IFREG | newmode;
	})));
	EXPECT_EQ(0, chmod(FULLPATH, newmode)) << strerror(errno);
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

	EXPECT_LOOKUP(1, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.nodeid = ino;
		out->body.entry.attr.gid = oldgroup;
		out->body.entry.attr.uid = olduser;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			/* In protocol 7.23, ctime will be changed too */
			uint32_t valid = FATTR_GID | FATTR_UID;
			return (in->header.opcode == FUSE_SETATTR &&
				in->header.nodeid == ino &&
				in->body.setattr.valid == valid &&
				in->body.setattr.uid == newuser &&
				in->body.setattr.gid == newgroup);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;	// Must match nodeid
		out->body.attr.attr.mode = S_IFREG | 0644;
		out->body.attr.attr.uid = newuser;
		out->body.attr.attr.gid = newgroup;
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

	EXPECT_LOOKUP(1, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in, auto out) {
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | 0777;
		out->body.entry.nodeid = ino;
		out->body.entry.attr.uid = in->header.uid;
		out->body.entry.attr.gid = in->header.gid;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in->header.opcode == FUSE_SETATTR &&
				in->header.nodeid == ino);
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

	EXPECT_LOOKUP(1, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | oldmode;
		out->body.entry.nodeid = ino;
		out->body.entry.attr_valid = UINT64_MAX;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_OPEN &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		out->header.len = sizeof(out->header);
		SET_OUT_HEADER_LEN(out, open);
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			/* In protocol 7.23, ctime will be changed too */
			uint32_t valid = FATTR_MODE;
			return (in->header.opcode == FUSE_SETATTR &&
				in->header.nodeid == ino &&
				in->body.setattr.valid == valid &&
				in->body.setattr.mode == newmode);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;	// Must match nodeid
		out->body.attr.attr.mode = S_IFREG | newmode;
	})));

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, fchmod(fd, newmode)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
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

	EXPECT_LOOKUP(1, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | 0755;
		out->body.entry.nodeid = ino;
		out->body.entry.attr_valid = UINT64_MAX;
		out->body.entry.attr.size = oldsize;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_OPEN &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		out->header.len = sizeof(out->header);
		SET_OUT_HEADER_LEN(out, open);
		out->body.open.fh = fh;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			/* In protocol 7.23, ctime will be changed too */
			uint32_t valid = FATTR_SIZE | FATTR_FH;
			return (in->header.opcode == FUSE_SETATTR &&
				in->header.nodeid == ino &&
				in->body.setattr.valid == valid &&
				in->body.setattr.fh == fh);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;	// Must match nodeid
		out->body.attr.attr.mode = S_IFREG | 0755;
		out->body.attr.attr.size = newsize;
	})));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, ftruncate(fd, newsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* Change the size of the file */
TEST_F(Setattr, truncate) {
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const uint64_t oldsize = 100'000'000;
	const uint64_t newsize = 20'000'000;

	EXPECT_LOOKUP(1, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.nodeid = ino;
		out->body.entry.attr.size = oldsize;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			/* In protocol 7.23, ctime will be changed too */
			uint32_t valid = FATTR_SIZE;
			return (in->header.opcode == FUSE_SETATTR &&
				in->header.nodeid == ino &&
				in->body.setattr.valid == valid &&
				in->body.setattr.size == newsize);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;	// Must match nodeid
		out->body.attr.attr.mode = S_IFREG | 0644;
		out->body.attr.attr.size = newsize;
	})));
	EXPECT_EQ(0, truncate(FULLPATH, newsize)) << strerror(errno);
}

/*
 * Truncating a file should discard cached data past the truncation point.
 * This is a regression test for bug 233783.  The bug only applies when
 * vfs.fusefs.data_cache_mode=1 or 2, but the test should pass regardless.
 */
TEST_F(Setattr, truncate_discards_cached_data) {
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	void *w0buf, *rbuf, *expected;
	off_t w0_offset = 0x1b8df;
	size_t w0_size = 0x61e8;
	off_t r_offset = 0xe1e6;
	off_t r_size = 0xe229;
	size_t trunc0_size = 0x10016;
	size_t trunc1_size = 131072;
	size_t cur_size = 0;
	const uint64_t ino = 42;
	mode_t mode = S_IFREG | 0644;
	int fd;

	w0buf = malloc(w0_size);
	ASSERT_NE(NULL, w0buf) << strerror(errno);
	memset(w0buf, 'X', w0_size);

	rbuf = malloc(r_size);
	ASSERT_NE(NULL, rbuf) << strerror(errno);

	expected = malloc(r_size);
	ASSERT_NE(NULL, expected) << strerror(errno);
	memset(expected, 0, r_size);

	expect_lookup(RELPATH, ino, mode, 0, 1);
	expect_open(ino, O_RDWR, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_GETATTR &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([&](auto i __unused, auto out) {
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;
		out->body.attr.attr.mode = mode;
		out->body.attr.attr.size = cur_size;
	})));
	/* 
	 * The exact pattern of FUSE_WRITE operations depends on the setting of
	 * vfs.fusefs.data_cache_mode.  But it's not important for this test.
	 * Just set the mocks to accept anything
	 */
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_WRITE);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([&](auto in, auto out) {
		SET_OUT_HEADER_LEN(out, write);
		out->body.attr.attr.ino = ino;
		out->body.write.size = in->body.write.size;
		cur_size = std::max(cur_size,
			in->body.write.size + in->body.write.offset);
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_SETATTR &&
				in->header.nodeid == ino &&
				(in->body.setattr.valid & FATTR_SIZE));
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([&](auto in, auto out) {
		auto trunc_size = in->body.setattr.size;
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;
		out->body.attr.attr.mode = mode;
		out->body.attr.attr.size = trunc_size;
		cur_size = trunc_size;
	})));

	/* exact pattern of FUSE_READ depends on vfs.fusefs.data_cache_mode */
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_READ);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([&](auto in, auto out) {
		auto osize = std::min(cur_size - in->body.read.offset,
			(size_t)in->body.read.size);
		out->header.len = sizeof(struct fuse_out_header) + osize;
		bzero(out->body.bytes, osize);
	})));

	fd = open(FULLPATH, O_RDWR, 0644);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ((ssize_t)w0_size, pwrite(fd, w0buf, w0_size, w0_offset));
	/* 1st truncate should discard cached data */
	EXPECT_EQ(0, ftruncate(fd, trunc0_size)) << strerror(errno);
	/* 2nd truncate extends file into previously cached data */
	EXPECT_EQ(0, ftruncate(fd, trunc1_size)) << strerror(errno);
	/* Read should return all zeros */
	ASSERT_EQ((ssize_t)r_size, pread(fd, rbuf, r_size, r_offset));

	ASSERT_EQ(0, memcmp(expected, rbuf, r_size));

	free(expected);
	free(rbuf);
	free(w0buf);
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

	EXPECT_LOOKUP(1, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.nodeid = ino;
		out->body.entry.attr_valid = UINT64_MAX;
		out->body.entry.attr.atime = oldtimes[0].tv_sec;
		out->body.entry.attr.atimensec = oldtimes[0].tv_nsec;
		out->body.entry.attr.mtime = oldtimes[1].tv_sec;
		out->body.entry.attr.mtimensec = oldtimes[1].tv_nsec;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			/* In protocol 7.23, ctime will be changed too */
			uint32_t valid = FATTR_ATIME | FATTR_MTIME;
			return (in->header.opcode == FUSE_SETATTR &&
				in->header.nodeid == ino &&
				in->body.setattr.valid == valid &&
				in->body.setattr.atime == newtimes[0].tv_sec &&
				in->body.setattr.atimensec ==
					newtimes[0].tv_nsec &&
				in->body.setattr.mtime == newtimes[1].tv_sec &&
				in->body.setattr.mtimensec ==
					newtimes[1].tv_nsec);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;	// Must match nodeid
		out->body.attr.attr.mode = S_IFREG | 0644;
		out->body.attr.attr.atime = newtimes[0].tv_sec;
		out->body.attr.attr.atimensec = newtimes[0].tv_nsec;
		out->body.attr.attr.mtime = newtimes[1].tv_sec;
		out->body.attr.attr.mtimensec = newtimes[1].tv_nsec;
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

	EXPECT_LOOKUP(1, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.nodeid = ino;
		out->body.entry.attr_valid = UINT64_MAX;
		out->body.entry.attr.atime = oldtimes[0].tv_sec;
		out->body.entry.attr.atimensec = oldtimes[0].tv_nsec;
		out->body.entry.attr.mtime = oldtimes[1].tv_sec;
		out->body.entry.attr.mtimensec = oldtimes[1].tv_nsec;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			/* In protocol 7.23, ctime will be changed too */
			uint32_t valid = FATTR_MTIME;
			return (in->header.opcode == FUSE_SETATTR &&
				in->header.nodeid == ino &&
				in->body.setattr.valid == valid &&
				in->body.setattr.mtime == newtimes[1].tv_sec &&
				in->body.setattr.mtimensec ==
					newtimes[1].tv_nsec);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;	// Must match nodeid
		out->body.attr.attr.mode = S_IFREG | 0644;
		out->body.attr.attr.atime = oldtimes[0].tv_sec;
		out->body.attr.attr.atimensec = oldtimes[0].tv_nsec;
		out->body.attr.attr.mtime = newtimes[1].tv_sec;
		out->body.attr.attr.mtimensec = newtimes[1].tv_nsec;
	})));
	EXPECT_EQ(0, utimensat(AT_FDCWD, FULLPATH, &newtimes[0], 0))
		<< strerror(errno);
}

/* On a read-only mount, no attributes may be changed */
TEST_F(RofsSetattr, erofs)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t oldmode = 0755;
	const mode_t newmode = 0644;

	EXPECT_LOOKUP(1, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | oldmode;
		out->body.entry.nodeid = ino;
	})));

	ASSERT_EQ(-1, chmod(FULLPATH, newmode));
	ASSERT_EQ(EROFS, errno);
}
