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

#include <sys/param.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <gtest/gtest.h>
#include <unistd.h>

#include "mockfs.hh"
#include "utils.hh"

class FuseEnv: public ::testing::Environment {
	virtual void SetUp() {
		const char *mod_name = "fuse";
		const char *devnode = "/dev/fuse";
		const char *usermount_node = "vfs.usermount";
		int usermount_val = 0;
		size_t usermount_size = sizeof(usermount_val);
		if (modfind(mod_name) == -1) {
			FAIL() << "Module " << mod_name <<
				" could not be resolved";
		}
		if (eaccess(devnode, R_OK | W_OK)) {
			if (errno == ENOENT) {
				FAIL() << devnode << " does not exist";
			} else if (errno == EACCES) {
				FAIL() << devnode <<
				    " is not accessible by the current user";
			} else {
				FAIL() << strerror(errno);
			}
		}
		sysctlbyname(usermount_node, &usermount_val, &usermount_size,
			     NULL, 0);
		if (geteuid() != 0 && !usermount_val)
			FAIL() << "current user is not allowed to mount";
	}
};

void FuseTest::SetUp() {
	const char *node = "vfs.maxbcachebuf";
	int val = 0;
	size_t size = sizeof(val);

	ASSERT_EQ(0, sysctlbyname(node, &val, &size, NULL, 0))
		<< strerror(errno);
	m_maxbcachebuf = val;

	try {
		m_mock = new MockFS(m_maxreadahead);
	} catch (std::system_error err) {
		FAIL() << err.what();
	}
}

static void usage(char* progname) {
	fprintf(stderr, "Usage: %s [-v]\n\t-v increase verbosity\n", progname);
	exit(2);
}

int main(int argc, char **argv) {
	int ch;
	FuseEnv *fuse_env = new FuseEnv;

	::testing::InitGoogleTest(&argc, argv);
	::testing::AddGlobalTestEnvironment(fuse_env);

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
