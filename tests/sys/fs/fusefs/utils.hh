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

struct _sem;
typedef struct _sem sem_t;
struct _dirdesc;
typedef struct _dirdesc DIR;

/* Nanoseconds to sleep, for tests that must */
#define NAP_NS	(100'000'000)

void get_unprivileged_id(uid_t *uid, gid_t *gid);
inline void nap()
{
	usleep(NAP_NS / 1000);
}

enum cache_mode {
	Uncached,
	Writethrough,
	Writeback,
	WritebackAsync
};

const char *cache_mode_to_s(enum cache_mode cm);
bool is_unsafe_aio_enabled(void);

extern const uint32_t libfuse_max_write;
class FuseTest : public ::testing::Test {
	protected:
	uint32_t m_maxreadahead;
	uint32_t m_maxwrite;
	uint32_t m_init_flags;
	bool m_allow_other;
	bool m_default_permissions;
	uint32_t m_kernel_minor_version;
	enum poll_method m_pm;
	bool m_noatime;
	bool m_push_symlinks_in;
	bool m_ro;
	bool m_async;
	bool m_noclusterr;
	bool m_nointr;
	unsigned m_time_gran;
	MockFS *m_mock = NULL;
	const static uint64_t FH = 0xdeadbeef1a7ebabe;
	const char *reclaim_mib = "debug.try_reclaim_vnode";

	public:
	int m_maxbcachebuf;
	int m_maxphys;

	FuseTest():
		m_maxreadahead(0),
		m_maxwrite(0),
		m_init_flags(0),
		m_allow_other(false),
		m_default_permissions(false),
		m_kernel_minor_version(FUSE_KERNEL_MINOR_VERSION),
		m_pm(BLOCKING),
		m_noatime(false),
		m_push_symlinks_in(false),
		m_ro(false),
		m_async(false),
		m_noclusterr(false),
		m_nointr(false),
		m_time_gran(1),
		m_maxbcachebuf(0),
		m_maxphys(0)
	{}

	virtual void SetUp();

	virtual void TearDown() {
		if (m_mock)
			delete m_mock;
	}

	/*
	 * Create an expectation that FUSE_ACCESS will be called once for the
	 * given inode with the given access_mode, returning the given errno
	 */
	void expect_access(uint64_t ino, mode_t access_mode, int error);

	/* Expect FUSE_DESTROY and shutdown the daemon */
	void expect_destroy(int error);

	/*
	 * Create an expectation that FUSE_FLUSH will be called times times for
	 * the given inode
	 */
	void expect_flush(uint64_t ino, int times, ProcessMockerT r);

	/*
	 * Create an expectation that FUSE_FORGET will be called for the given
	 * inode.  There will be no response.  If sem is provided, it will be
	 * posted after the operation is received by the daemon.
	 */
	void expect_forget(uint64_t ino, uint64_t nlookup, sem_t *sem = NULL);

	/*
	 * Create an expectation that FUSE_GETATTR will be called for the given
	 * inode any number of times.  It will respond with a few basic
	 * attributes, like the given size and the mode S_IFREG | 0644
	 */
	void expect_getattr(uint64_t ino, uint64_t size);

	/*
	 * Create an expectation that FUSE_GETXATTR will be called once for the
	 * given inode.
	 */
	void expect_getxattr(uint64_t ino, const char *attr, ProcessMockerT r);

	/*
	 * Create an expectation that FUSE_LOOKUP will be called for the given
	 * path exactly times times and cache validity period.  It will respond
	 * with inode ino, mode mode, filesize size.
	 */
	void expect_lookup(const char *relpath, uint64_t ino, mode_t mode,
		uint64_t size, int times, uint64_t attr_valid = UINT64_MAX,
		uid_t uid = 0, gid_t gid = 0);

	/* The protocol 7.8 version of expect_lookup */
	void expect_lookup_7_8(const char *relpath, uint64_t ino, mode_t mode,
		uint64_t size, int times, uint64_t attr_valid = UINT64_MAX,
		uid_t uid = 0, gid_t gid = 0);

	/*
	 * Create an expectation that FUSE_OPEN will be called for the given
	 * inode exactly times times.  It will return with open_flags flags and
	 * file handle FH.
	 */
	void expect_open(uint64_t ino, uint32_t flags, int times);

	/*
	 * Create an expectation that FUSE_OPENDIR will be called exactly once
	 * for inode ino.
	 */
	void expect_opendir(uint64_t ino);

	/*
	 * Create an expectation that FUSE_READ will be called exactly once for
	 * the given inode, at offset offset and with size isize.  It will
	 * return the first osize bytes from contents
	 *
	 * Protocol 7.8 tests can use this same expectation method because
	 * nothing currently validates the size of the fuse_read_in struct.
	 */
	void expect_read(uint64_t ino, uint64_t offset, uint64_t isize,
		uint64_t osize, const void *contents, int flags = -1,
		uint64_t fh = FH);

	/*
	 * Create an expectation that FUSE_READIR will be called any number of
	 * times on the given ino with the given offset, returning (by copy)
	 * the provided entries
	 */
	void expect_readdir(uint64_t ino, uint64_t off,
		std::vector<struct dirent> &ents);

	/* 
	 * Create an expectation that FUSE_RELEASE will be called exactly once
	 * for the given inode and filehandle, returning success
	 */
	void expect_release(uint64_t ino, uint64_t fh);

	/*
	 * Create an expectation that FUSE_RELEASEDIR will be called exactly
	 * once for the given inode
	 */
	void expect_releasedir(uint64_t ino, ProcessMockerT r);

	/*
	 * Create an expectation that FUSE_UNLINK will be called exactly once
	 * for the given path, returning an errno
	 */
	void expect_unlink(uint64_t parent, const char *path, int error);

	/*
	 * Create an expectation that FUSE_WRITE will be called exactly once
	 * for the given inode, at offset offset, with  size isize and buffer
	 * contents.  Any flags present in flags_set must be set, and any
	 * present in flags_unset must not be set.  Other flags are don't care.
	 * It will return osize.
	 */
	void expect_write(uint64_t ino, uint64_t offset, uint64_t isize,
		uint64_t osize, uint32_t flags_set, uint32_t flags_unset,
		const void *contents);

	/* Protocol 7.8 version of expect_write */
	void expect_write_7_8(uint64_t ino, uint64_t offset, uint64_t isize,
		uint64_t osize, const void *contents);

	/*
	 * Helper that runs code in a child process.
	 *
	 * First, parent_func runs in the parent process.
	 * Then, child_func runs in the child process, dropping privileges if
	 * desired.
	 * Finally, fusetest_fork returns.
	 *
	 * # Returns
	 *
	 * fusetest_fork may SKIP the test, which the caller should detect with
	 * the IsSkipped() method.  If not, then the child's exit status will
	 * be returned in status.
	 */
	void fork(bool drop_privs, int *status,
		std::function<void()> parent_func,
		std::function<int()> child_func);

	/*
	 * Deliberately leak a file descriptor.
	 *
	 * Closing a file descriptor on fusefs would cause the server to
	 * receive FUSE_CLOSE and possibly FUSE_INACTIVE.  Handling those
	 * operations would needlessly complicate most tests.  So most tests
	 * deliberately leak the file descriptors instead.  This method serves
	 * to document the leakage, and provide a single point of suppression
	 * for static analyzers.
	 */
	/* coverity[+close: arg-0] */
	static void leak(int fd __unused) {}

	/*
	 * Deliberately leak a DIR* pointer
	 *
	 * See comments for FuseTest::leak
	 */
	static void leakdir(DIR* dirp __unused) {}

	/* Manually reclaim a vnode.  Requires root privileges. */
	void reclaim_vnode(const char *fullpath);
};
