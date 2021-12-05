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
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <dirent.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <semaphore.h>
#include <unistd.h>
}

#include <gtest/gtest.h>

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

/*
 * The default max_write is set to this formula in libfuse, though
 * individual filesystems can lower it.  The "- 4096" was added in
 * commit 154ffe2, with the commit message "fix".
 */
const uint32_t libfuse_max_write = 32 * getpagesize() + 0x1000 - 4096;

/* Check that fusefs(4) is accessible and the current user can mount(2) */
void check_environment()
{
	const char *devnode = "/dev/fuse";
	const char *bsdextended_node = "security.mac.bsdextended.enabled";
	int bsdextended_val = 0;
	size_t bsdextended_size = sizeof(bsdextended_val);
	int bsdextended_found;
	const char *usermount_node = "vfs.usermount";
	int usermount_val = 0;
	size_t usermount_size = sizeof(usermount_val);
	if (eaccess(devnode, R_OK | W_OK)) {
		if (errno == ENOENT) {
			GTEST_SKIP() << devnode << " does not exist";
		} else if (errno == EACCES) {
			GTEST_SKIP() << devnode <<
			    " is not accessible by the current user";
		} else {
			GTEST_SKIP() << strerror(errno);
		}
	}
	// mac_bsdextended(4), when enabled, generates many more GETATTR
	// operations. The fusefs tests' expectations don't account for those,
	// and adding extra code to handle them obfuscates the real purpose of
	// the tests.  Better just to skip the fusefs tests if mac_bsdextended
	// is enabled.
	bsdextended_found = sysctlbyname(bsdextended_node, &bsdextended_val,
					 &bsdextended_size, NULL, 0);
	if (bsdextended_found == 0 && bsdextended_val != 0)
		GTEST_SKIP() <<
		    "The fusefs tests are incompatible with mac_bsdextended.";
	ASSERT_EQ(sysctlbyname(usermount_node, &usermount_val, &usermount_size,
			       NULL, 0),
		  0);
	if (geteuid() != 0 && !usermount_val)
		GTEST_SKIP() << "current user is not allowed to mount";
}

const char *cache_mode_to_s(enum cache_mode cm) {
	switch (cm) {
	case Uncached:
		return "Uncached";
	case Writethrough:
		return "Writethrough";
	case Writeback:
		return "Writeback";
	case WritebackAsync:
		return "WritebackAsync";
	default:
		return "Unknown";
	}
}

bool is_unsafe_aio_enabled(void) {
	const char *node = "vfs.aio.enable_unsafe";
	int val = 0;
	size_t size = sizeof(val);

	if (sysctlbyname(node, &val, &size, NULL, 0)) {
		perror("sysctlbyname");
		return (false);
	}
	return (val != 0);
}

class FuseEnv: public Environment {
	virtual void SetUp() {
	}
};

void FuseTest::SetUp() {
	const char *maxbcachebuf_node = "vfs.maxbcachebuf";
	const char *maxphys_node = "kern.maxphys";
	int val = 0;
	size_t size = sizeof(val);

	/*
	 * XXX check_environment should be called from FuseEnv::SetUp, but
	 * can't due to https://github.com/google/googletest/issues/2189
	 */
	check_environment();
	if (IsSkipped())
		return;

	ASSERT_EQ(0, sysctlbyname(maxbcachebuf_node, &val, &size, NULL, 0))
		<< strerror(errno);
	m_maxbcachebuf = val;
	ASSERT_EQ(0, sysctlbyname(maxphys_node, &val, &size, NULL, 0))
		<< strerror(errno);
	m_maxphys = val;
	/*
	 * Set the default max_write to a distinct value from MAXPHYS to catch
	 * bugs that confuse the two.
	 */
	if (m_maxwrite == 0)
		m_maxwrite = MIN(libfuse_max_write, (uint32_t)m_maxphys / 2);

	try {
		m_mock = new MockFS(m_maxreadahead, m_allow_other,
			m_default_permissions, m_push_symlinks_in, m_ro,
			m_pm, m_init_flags, m_kernel_minor_version,
			m_maxwrite, m_async, m_noclusterr, m_time_gran,
			m_nointr, m_noatime);
		/* 
		 * FUSE_ACCESS is called almost universally.  Expecting it in
		 * each test case would be super-annoying.  Instead, set a
		 * default expectation for FUSE_ACCESS and return ENOSYS.
		 *
		 * Individual test cases can override this expectation since
		 * googlemock evaluates expectations in LIFO order.
		 */
		EXPECT_CALL(*m_mock, process(
			ResultOf([=](auto in) {
				return (in.header.opcode == FUSE_ACCESS);
			}, Eq(true)),
			_)
		).Times(AnyNumber())
		.WillRepeatedly(Invoke(ReturnErrno(ENOSYS)));
		/*
		 * FUSE_BMAP is called for most test cases that read data.  Set
		 * a default expectation and return ENOSYS.
		 *
		 * Individual test cases can override this expectation since
		 * googlemock evaluates expectations in LIFO order.
		 */
		EXPECT_CALL(*m_mock, process(
			ResultOf([=](auto in) {
				return (in.header.opcode == FUSE_BMAP);
			}, Eq(true)),
			_)
		).Times(AnyNumber())
		.WillRepeatedly(Invoke(ReturnErrno(ENOSYS)));
	} catch (std::system_error err) {
		FAIL() << err.what();
	}
}

void
FuseTest::expect_access(uint64_t ino, mode_t access_mode, int error)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_ACCESS &&
				in.header.nodeid == ino &&
				in.body.access.mask == access_mode);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(error)));
}

void
FuseTest::expect_destroy(int error)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_DESTROY);
		}, Eq(true)),
		_)
	).WillOnce(Invoke( ReturnImmediate([&](auto in, auto& out) {
		m_mock->m_quit = true;
		out.header.len = sizeof(out.header);
		out.header.unique = in.header.unique;
		out.header.error = -error;
	})));
}

void
FuseTest::expect_flush(uint64_t ino, int times, ProcessMockerT r)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_FLUSH &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(times)
	.WillRepeatedly(Invoke(r));
}

void
FuseTest::expect_forget(uint64_t ino, uint64_t nlookup, sem_t *sem)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_FORGET &&
				in.header.nodeid == ino &&
				in.body.forget.nlookup == nlookup);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([=](auto in __unused, auto &out __unused) {
		if (sem != NULL)
			sem_post(sem);
		/* FUSE_FORGET has no response! */
	}));
}

void FuseTest::expect_getattr(uint64_t ino, uint64_t size)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | 0644;
		out.body.attr.attr.size = size;
		out.body.attr.attr_valid = UINT64_MAX;
	})));
}

void FuseTest::expect_getxattr(uint64_t ino, const char *attr, ProcessMockerT r)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *a = (const char*)in.body.bytes +
				sizeof(fuse_getxattr_in);
			return (in.header.opcode == FUSE_GETXATTR &&
				in.header.nodeid == ino &&
				0 == strcmp(attr, a));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(r));
}

void FuseTest::expect_lookup(const char *relpath, uint64_t ino, mode_t mode,
	uint64_t size, int times, uint64_t attr_valid, uid_t uid, gid_t gid)
{
	EXPECT_LOOKUP(FUSE_ROOT_ID, relpath)
	.Times(times)
	.WillRepeatedly(Invoke(
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.nlink = 1;
		out.body.entry.attr_valid = attr_valid;
		out.body.entry.attr.size = size;
		out.body.entry.attr.uid = uid;
		out.body.entry.attr.gid = gid;
	})));
}

void FuseTest::expect_lookup_7_8(const char *relpath, uint64_t ino, mode_t mode,
	uint64_t size, int times, uint64_t attr_valid, uid_t uid, gid_t gid)
{
	EXPECT_LOOKUP(FUSE_ROOT_ID, relpath)
	.Times(times)
	.WillRepeatedly(Invoke(
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry_7_8);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.nlink = 1;
		out.body.entry.attr_valid = attr_valid;
		out.body.entry.attr.size = size;
		out.body.entry.attr.uid = uid;
		out.body.entry.attr.gid = gid;
	})));
}

void FuseTest::expect_open(uint64_t ino, uint32_t flags, int times)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_OPEN &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(times)
	.WillRepeatedly(Invoke(
		ReturnImmediate([=](auto in __unused, auto& out) {
		out.header.len = sizeof(out.header);
		SET_OUT_HEADER_LEN(out, open);
		out.body.open.fh = FH;
		out.body.open.open_flags = flags;
	})));
}

void FuseTest::expect_opendir(uint64_t ino)
{
	/* opendir(3) calls fstatfs */
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_STATFS);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(
	ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, statfs);
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_OPENDIR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		out.header.len = sizeof(out.header);
		SET_OUT_HEADER_LEN(out, open);
		out.body.open.fh = FH;
	})));
}

void FuseTest::expect_read(uint64_t ino, uint64_t offset, uint64_t isize,
	uint64_t osize, const void *contents, int flags, uint64_t fh)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READ &&
				in.header.nodeid == ino &&
				in.body.read.fh == fh &&
				in.body.read.offset == offset &&
				in.body.read.size == isize &&
				(flags == -1 ?
					(in.body.read.flags == O_RDONLY ||
					 in.body.read.flags == O_RDWR)
				: in.body.read.flags == (uint32_t)flags));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		out.header.len = sizeof(struct fuse_out_header) + osize;
		memmove(out.body.bytes, contents, osize);
	}))).RetiresOnSaturation();
}

void FuseTest::expect_readdir(uint64_t ino, uint64_t off,
	std::vector<struct dirent> &ents)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READDIR &&
				in.header.nodeid == ino &&
				in.body.readdir.fh == FH &&
				in.body.readdir.offset == off);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([=](auto in, auto& out) {
		struct fuse_dirent *fde = (struct fuse_dirent*)&(out.body);
		int i = 0;

		out.header.error = 0;
		out.header.len = 0;

		for (const auto& it: ents) {
			size_t entlen, entsize;

			fde->ino = it.d_fileno;
			fde->off = it.d_off;
			fde->type = it.d_type;
			fde->namelen = it.d_namlen;
			strncpy(fde->name, it.d_name, it.d_namlen);
			entlen = FUSE_NAME_OFFSET + fde->namelen;
			entsize = FUSE_DIRENT_SIZE(fde);
			/* 
			 * The FUSE protocol does not require zeroing out the
			 * unused portion of the name.  But it's a good
			 * practice to prevent information disclosure to the
			 * FUSE client, even though the client is usually the
			 * kernel
			 */
			memset(fde->name + fde->namelen, 0, entsize - entlen);
			if (out.header.len + entsize > in.body.read.size) {
				printf("Overflow in readdir expectation: i=%d\n"
					, i);
				break;
			}
			out.header.len += entsize;
			fde = (struct fuse_dirent*)
				((intmax_t*)fde + entsize / sizeof(intmax_t));
			i++;
		}
		out.header.len += sizeof(out.header);
	})));

}
void FuseTest::expect_release(uint64_t ino, uint64_t fh)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_RELEASE &&
				in.header.nodeid == ino &&
				in.body.release.fh == fh);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(0)));
}

void FuseTest::expect_releasedir(uint64_t ino, ProcessMockerT r)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_RELEASEDIR &&
				in.header.nodeid == ino &&
				in.body.release.fh == FH);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(r));
}

void FuseTest::expect_unlink(uint64_t parent, const char *path, int error)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_UNLINK &&
				0 == strcmp(path, in.body.unlink) &&
				in.header.nodeid == parent);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(error)));
}

void FuseTest::expect_write(uint64_t ino, uint64_t offset, uint64_t isize,
	uint64_t osize, uint32_t flags_set, uint32_t flags_unset,
	const void *contents)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *buf = (const char*)in.body.bytes +
				sizeof(struct fuse_write_in);
			bool pid_ok;
			uint32_t wf = in.body.write.write_flags;

			if (wf & FUSE_WRITE_CACHE)
				pid_ok = true;
			else
				pid_ok = (pid_t)in.header.pid == getpid();

			return (in.header.opcode == FUSE_WRITE &&
				in.header.nodeid == ino &&
				in.body.write.fh == FH &&
				in.body.write.offset == offset  &&
				in.body.write.size == isize &&
				pid_ok &&
				(wf & flags_set) == flags_set &&
				(wf & flags_unset) == 0 &&
				(in.body.write.flags == O_WRONLY ||
				 in.body.write.flags == O_RDWR) &&
				0 == bcmp(buf, contents, isize));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, write);
		out.body.write.size = osize;
	})));
}

void FuseTest::expect_write_7_8(uint64_t ino, uint64_t offset, uint64_t isize,
	uint64_t osize, const void *contents)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *buf = (const char*)in.body.bytes +
				FUSE_COMPAT_WRITE_IN_SIZE;
			bool pid_ok = (pid_t)in.header.pid == getpid();
			return (in.header.opcode == FUSE_WRITE &&
				in.header.nodeid == ino &&
				in.body.write.fh == FH &&
				in.body.write.offset == offset  &&
				in.body.write.size == isize &&
				pid_ok &&
				0 == bcmp(buf, contents, isize));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, write);
		out.body.write.size = osize;
	})));
}

void
get_unprivileged_id(uid_t *uid, gid_t *gid)
{
	struct passwd *pw;
	struct group *gr;

	/* 
	 * First try "tests", Kyua's default unprivileged user.  XXX after
	 * GoogleTest gains a proper Kyua wrapper, get this with the Kyua API
	 */
	pw = getpwnam("tests");
	if (pw == NULL) {
		/* Fall back to "nobody" */
		pw = getpwnam("nobody");
	}
	if (pw == NULL)
		GTEST_SKIP() << "Test requires an unprivileged user";
	/* Use group "nobody", which is Kyua's default unprivileged group */
	gr = getgrnam("nobody");
	if (gr == NULL)
		GTEST_SKIP() << "Test requires an unprivileged group";
	*uid = pw->pw_uid;
	*gid = gr->gr_gid;
}

void
FuseTest::fork(bool drop_privs, int *child_status,
	std::function<void()> parent_func,
	std::function<int()> child_func)
{
	sem_t *sem;
	int mprot = PROT_READ | PROT_WRITE;
	int mflags = MAP_ANON | MAP_SHARED;
	pid_t child;
	uid_t uid;
	gid_t gid;
	
	if (drop_privs) {
		get_unprivileged_id(&uid, &gid);
		if (IsSkipped())
			return;
	}

	sem = (sem_t*)mmap(NULL, sizeof(*sem), mprot, mflags, -1, 0);
	ASSERT_NE(MAP_FAILED, sem) << strerror(errno);
	ASSERT_EQ(0, sem_init(sem, 1, 0)) << strerror(errno);

	if ((child = ::fork()) == 0) {
		/* In child */
		int err = 0;

		if (sem_wait(sem)) {
			perror("sem_wait");
			err = 1;
			goto out;
		}

		if (drop_privs && 0 != setegid(gid)) {
			perror("setegid");
			err = 1;
			goto out;
		}
		if (drop_privs && 0 != setreuid(-1, uid)) {
			perror("setreuid");
			err = 1;
			goto out;
		}
		err = child_func();

out:
		sem_destroy(sem);
		_exit(err);
	} else if (child > 0) {
		/* 
		 * In parent.  Cleanup must happen here, because it's still
		 * privileged.
		 */
		m_mock->m_child_pid = child;
		ASSERT_NO_FATAL_FAILURE(parent_func());

		/* Signal the child process to go */
		ASSERT_EQ(0, sem_post(sem)) << strerror(errno);

		ASSERT_LE(0, wait(child_status)) << strerror(errno);
	} else {
		FAIL() << strerror(errno);
	}
	munmap(sem, sizeof(*sem));
	return;
}

static void usage(char* progname) {
	fprintf(stderr, "Usage: %s [-v]\n\t-v increase verbosity\n", progname);
	exit(2);
}

int main(int argc, char **argv) {
	int ch;
	FuseEnv *fuse_env = new FuseEnv;

	InitGoogleTest(&argc, argv);
	AddGlobalTestEnvironment(fuse_env);

	while ((ch = getopt(argc, argv, "v")) != -1) {
		switch (ch) {
			case 'v':
				verbosity++;
				break;
			default:
				usage(argv[0]);
				break;
		}
	}

	return (RUN_ALL_TESTS());
}
