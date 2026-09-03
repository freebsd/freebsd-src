/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* Landlock sandbox: read anywhere, write only in $TMPDIR. */
#include "file.h"

#if HAVE_LINUX_LANDLOCK_H
#include "magic.h"
#include <linux/landlock.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/* glibc only got Landlock wrappers in 2.40, so call via syscall(2). */
#ifndef landlock_create_ruleset
static inline int
landlock_create_ruleset(const struct landlock_ruleset_attr *attr,
    size_t size, uint32_t flags)
{
	return CAST(int, syscall(__NR_landlock_create_ruleset, attr, size,
	    flags));
}
#endif

#ifndef landlock_add_rule
static inline int
landlock_add_rule(int ruleset_fd, enum landlock_rule_type rule_type,
    const void *rule_attr, uint32_t flags)
{
	return CAST(int, syscall(__NR_landlock_add_rule, ruleset_fd,
	    rule_type, rule_attr, flags));
}
#endif

#ifndef landlock_restrict_self
static inline int
landlock_restrict_self(int ruleset_fd, uint32_t flags)
{
	return CAST(int, syscall(__NR_landlock_restrict_self, ruleset_fd,
	    flags));
}
#endif

/* A missing path (e.g. unset $TMPDIR) is not fatal, just skipped. */
static int
landlock_allow_path(int ruleset_fd, const char *path, uint64_t allowed)
{
	struct landlock_path_beneath_attr pb;
	int rv;

	pb.allowed_access = allowed;
	pb.parent_fd = open(path, O_PATH | O_CLOEXEC);
	if (pb.parent_fd == -1)
		return errno == ENOENT ? 0 : -1;
	rv = landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &pb, 0);
	(void)close(pb.parent_fd);
	return rv;
}

int
enable_landlock(int flags, int action)
{
	struct landlock_ruleset_attr attr;
	struct stat sb;
	int ruleset_fd, abi, needs_write;
	const char *tmpdir;

	(void)flags;

	/* Magic build modes write outside /tmp; just skip Landlock
	   for those. seccomp still applies. */
	if (action == FILE_COMPILE || action == FILE_CHECK ||
	    action == FILE_LIST)
		return 0;

	/* Pipe input gets copied to a tempfile in /tmp. */
	needs_write = fstat(STDIN_FILENO, &sb) == 0 && S_ISFIFO(sb.st_mode);

	abi = CAST(int, syscall(__NR_landlock_create_ruleset, NULL, 0,
	    LANDLOCK_CREATE_RULESET_VERSION));
	if (abi < 1)
		return 0;

	(void)memset(&attr, 0, sizeof(attr));
	attr.handled_access_fs =
	    LANDLOCK_ACCESS_FS_EXECUTE |
	    LANDLOCK_ACCESS_FS_WRITE_FILE |
	    LANDLOCK_ACCESS_FS_READ_FILE |
	    LANDLOCK_ACCESS_FS_READ_DIR |
	    LANDLOCK_ACCESS_FS_REMOVE_DIR |
	    LANDLOCK_ACCESS_FS_REMOVE_FILE |
	    LANDLOCK_ACCESS_FS_MAKE_CHAR |
	    LANDLOCK_ACCESS_FS_MAKE_DIR |
	    LANDLOCK_ACCESS_FS_MAKE_REG |
	    LANDLOCK_ACCESS_FS_MAKE_SOCK |
	    LANDLOCK_ACCESS_FS_MAKE_FIFO |
	    LANDLOCK_ACCESS_FS_MAKE_BLOCK |
	    LANDLOCK_ACCESS_FS_MAKE_SYM;
#ifdef LANDLOCK_ACCESS_FS_REFER
	if (abi >= 2)
		attr.handled_access_fs |= LANDLOCK_ACCESS_FS_REFER;
#endif
#ifdef LANDLOCK_ACCESS_FS_TRUNCATE
	if (abi >= 3)
		attr.handled_access_fs |= LANDLOCK_ACCESS_FS_TRUNCATE;
#endif
#ifdef LANDLOCK_ACCESS_NET_BIND_TCP
	if (abi >= 4) {
		attr.handled_access_net =
		    LANDLOCK_ACCESS_NET_BIND_TCP |
		    LANDLOCK_ACCESS_NET_CONNECT_TCP;
	}
#endif

	ruleset_fd = landlock_create_ruleset(&attr, sizeof(attr), 0);
	if (ruleset_fd == -1)
		return -1;

	if (landlock_allow_path(ruleset_fd, "/",
	    LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR) == -1)
		goto fail;

	if (needs_write) {
		uint64_t tmp_access =
		    LANDLOCK_ACCESS_FS_READ_FILE |
		    LANDLOCK_ACCESS_FS_READ_DIR |
		    LANDLOCK_ACCESS_FS_WRITE_FILE |
		    LANDLOCK_ACCESS_FS_MAKE_REG |
		    LANDLOCK_ACCESS_FS_REMOVE_FILE;
#ifdef LANDLOCK_ACCESS_FS_TRUNCATE
		if (abi >= 3)
			tmp_access |= LANDLOCK_ACCESS_FS_TRUNCATE;
#endif
		tmpdir = getenv("TMPDIR");
		if (tmpdir == NULL || *tmpdir == '\0')
			tmpdir = "/tmp";
		if (landlock_allow_path(ruleset_fd, tmpdir, tmp_access) == -1)
			goto fail;
	}

	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1)
		goto fail;
	if (landlock_restrict_self(ruleset_fd, 0) == -1)
		goto fail;

	(void)close(ruleset_fd);
	return 0;
fail:
	(void)close(ruleset_fd);
	return -1;
}

#endif /* HAVE_LINUX_LANDLOCK_H */
