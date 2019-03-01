/*-
 * Copyright (c) 2019 The FreeBSD Foundation
 * All rights reserved.
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

#include <sys/module.h>

#define GTEST_REQUIRE_KERNEL_MODULE(_mod_name) do {	\
	if (modfind(_mod_name) == -1) {	\
		printf("module %s could not be resolved: %s\n", \
			_mod_name, strerror(errno)); \
		/*
		 * TODO: enable GTEST_SKIP once GoogleTest 1.8.2 merges
		 * GTEST_SKIP()
		 */ \
		FAIL() << "Module " << _mod_name << " could not be resolved\n";\
	} \
} while(0)

class FuseTest : public ::testing::Test {
	protected:
	MockFS *m_mock = NULL;

	public:
	void SetUp() {
		GTEST_REQUIRE_KERNEL_MODULE("fuse");
		try {
			m_mock = new MockFS{};
		} catch (std::system_error err) {
			FAIL() << err.what();
		}
	}

	void TearDown() {
		if (m_mock)
			delete m_mock;
	}
};
