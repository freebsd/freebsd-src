#!/usr/libexec/flua
--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2024, Klara, Inc.
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions
-- are met:
-- 1. Redistributions of source code must retain the above copyright
--    notice, this list of conditions and the following disclaimer.
-- 2. Redistributions in binary form must reproduce the above copyright
--    notice, this list of conditions and the following disclaimer in the
--    documentation and/or other materials provided with the distribution.
--
-- THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
-- ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
-- ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
-- FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
-- DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
-- OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
-- HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
-- LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
-- OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
-- SUCH DAMAGE.
--

-- THEORY OF OPERATION
--
-- generate-fortify-tests.lua is intended to test fortified functions as found
-- mostly in the various headers in /usr/include/ssp.  Each fortified function
-- gets three basic tests:
--
--   1. Write just before the end of the buffer,
--   2. Write right at the end of the buffer,
--   3. Write just after the end of the buffer.
--
-- Each test is actually generated twice: once with a buffer on the stack, and
-- again with a buffer on the heap, to confirm that __builtin_object_size(3) can
-- deduce the buffer size in both scenarios.  The tests work by setting up the
-- stack with our buffer (and some padding on either side to avoid tripping any
-- other stack or memory protection), doing any initialization as described by
-- the test definition, then calling the fortified function with the buffer as
-- outlined by the test definition.
--
-- For the 'before' and 'at' the end tests, we're ensuring that valid writes
-- that are on the verge of being invalid aren't accidentally being detected as
-- invalid.
--
-- The 'after' test is the one that actually tests the functional benefit of
-- _FORTIFY_SOURCE by violating a boundary that should trigger an abort.  As
-- such, this test differs more from the other two in that it has to fork() off
-- the fortified function call so that we can monitor for a SIGABRT and
-- pass/fail the test at function end appropriately.

-- Some tests, like the FD_*() macros, may define these differently.  For
-- instance, for fd sets we're varying the index we pass and not using arbitrary
-- buffers.  Other tests that don't use the length in any way may physically
-- vary the buffer size for each test case when we'd typically vary the length
-- we're requesting a write for.

local includes = {
	"sys/param.h",
	"sys/jail.h",
	"sys/random.h",
	"sys/resource.h",
	"sys/select.h",
	"sys/socket.h",
	"sys/time.h",
	"sys/uio.h",
	"sys/wait.h",
	"dirent.h",
	"errno.h",
	"fcntl.h",
	"limits.h",
	"poll.h",
	"signal.h",
	"stdio.h",
	"stdlib.h",
	"string.h",
	"strings.h",
	"sysexits.h",
	"unistd.h",
	"wchar.h",
	"atf-c.h",
}

local tests_added = {}

-- Configuration for tests that want the host/domainname
local hostname = "host.example.com"
local domainname = "example.com"

-- Some of these will need to be excluded because clang sees the wrong size when
-- an array is embedded inside a struct, we'll get something that looks more
-- like __builtin_object_size(ptr, 0) than it does the correct
-- __builtin_object_size(ptr, 1) (i.e., includes the padding after).  This is
-- almost certainly a bug in llvm.
local function excludes_stack_overflow(disposition, is_heap)
	return (not is_heap) and disposition > 0
end

local poll_init = [[
	for (size_t i = 0; i < howmany(__bufsz, sizeof(struct pollfd)); i++) {
		__stack.__buf[i].fd = -1;
	}
]]

local printf_stackvars = "\tchar srcvar[__len + 10];\n"
local printf_init = [[
	memset(srcvar, 'A', sizeof(srcvar) - 1);
	srcvar[sizeof(srcvar) - 1] = '\0';
]]

local readv_stackvars = "\tstruct iovec iov[1];\n"
local readv_init = [[
	iov[0].iov_base = __stack.__buf;
	iov[0].iov_len = __len;

	replace_stdin();
]]

local socket_stackvars = "\tint sock[2] = { -1, -1 };\n"
local recvfrom_sockaddr_stackvars = socket_stackvars .. [[
	char data[16];
	socklen_t socklen;
]]
local recvmsg_stackvars = socket_stackvars .. "\tstruct msghdr msg;\n"
local socket_init = [[
	new_socket(sock);
]]
local socket_socklen_init = socket_init .. [[
	socklen = __len;
]]

local stdio_init = [[
	replace_stdin();
]]

local string_stackvars = "\tchar src[__len];\n"
local string_init = [[
	memset(__stack.__buf, 0, __len);
	memset(src, 'A', __len - 1);
	src[__len - 1] = '\0';
]]

local wstring_stackvars = "\twchar_t src[__len];\n"
local wstring_init = [[
	wmemset(__stack.__buf, 0, __len);
	wmemset(src, 'A', __len - 1);
	src[__len - 1] = '\0';
]]

-- Each test entry describes how to test a given function.  We need to know how
-- to construct the buffer, we need to know the argument set we're dealing with,
-- and we need to know what we're passing to each argument.  We could be passing
-- fixed values, or we could be passing the __buf under test.
--
-- definition:
--   func: name of the function under test to call
--   bufsize: size of buffer to generate, defaults to 42
--   buftype: type of buffer to generate, defaults to unsigned char[]
--   arguments: __buf, __len, or the name of a variable placed on the stack
--   exclude: a function(disposition, is_heap) that returns true if this combo
--     should be excluded.
--   stackvars: extra variables to be placed on the stack, should be a string
--     optionally formatted with tabs and newlines
--   init: extra code to inject just before the function call for initialization
--     of the buffer or any of the above-added stackvars; also a string
--   uses_len: bool-ish, necessary if arguments doesn't include either __idx or
--     or __len so that the test generator doesn't try to vary the size of the
--     buffer instead of just manipulating __idx/__len to try and induce an
--     overflow.
--
-- Most tests will just use the default bufsize/buftype, but under some
-- circumstances it's useful to use a different type (e.g., for alignment
-- requirements).
local all_tests = {
	random = {
		-- <sys/random.h>
		{
			func = "getrandom",
			arguments = {
				"__buf",
				"__len",
				"0",
			},
			exclude = excludes_stack_overflow,
		},
	},
	select = {
		-- <sys/select.h>
		{
			func = "FD_SET",
			bufsize = "FD_SETSIZE",
			buftype = "fd_set",
			arguments = {
				"__idx",
				"__buf",
			},
		},
		{
			func = "FD_CLR",
			bufsize = "FD_SETSIZE",
			buftype = "fd_set",
			arguments = {
				"__idx",
				"__buf",
			},
		},
		{
			func = "FD_ISSET",
			bufsize = "FD_SETSIZE",
			buftype = "fd_set",
			arguments = {
				"__idx",
				"__buf",
			},
		},
	},
	socket = {
		-- <sys/socket.h>
		{
			func = "getpeername",
			buftype = "struct sockaddr",
			bufsize = "sizeof(struct sockaddr)",
			arguments = {
				"sock[0]",
				"__buf",
				"&socklen",
			},
			exclude = excludes_stack_overflow,
			stackvars = socket_stackvars .. "\tsocklen_t socklen;",
			init = socket_socklen_init,
			uses_len = true,
		},
		{
			func = "getsockname",
			buftype = "struct sockaddr",
			bufsize = "sizeof(struct sockaddr)",
			arguments = {
				"sock[0]",
				"__buf",
				"&socklen",
			},
			exclude = excludes_stack_overflow,
			stackvars = socket_stackvars .. "\tsocklen_t socklen;",
			init = socket_socklen_init,
			uses_len = true,
		},
		{
			func = "recv",
			arguments = {
				"sock[0]",
				"__buf",
				"__len",
				"0",
			},
			exclude = excludes_stack_overflow,
			stackvars = socket_stackvars,
			init = socket_init,
		},
		{
			func = "recvfrom",
			arguments = {
				"sock[0]",
				"__buf",
				"__len",
				"0",
				"NULL",
				"NULL",
			},
			exclude = excludes_stack_overflow,
			stackvars = socket_stackvars,
			init = socket_init,
		},
		{
			func = "recvfrom",
			variant = "sockaddr",
			buftype = "struct sockaddr",
			bufsize = "sizeof(struct sockaddr)",
			arguments = {
				"sock[0]",
				"data",
				"sizeof(data)",
				"0",
				"__buf",
				"&socklen",
			},
			exclude = excludes_stack_overflow,
			stackvars = recvfrom_sockaddr_stackvars,
			init = socket_socklen_init,
			uses_len = true,
		},
		{
			func = "recvmsg",
			variant = "msg_name",
			buftype = "struct sockaddr",
			bufsize = "sizeof(struct sockaddr)",
			arguments = {
				"sock[0]",
				"&msg",
				"0",
			},
			exclude = excludes_stack_overflow,
			stackvars = recvmsg_stackvars,
			init = [[
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = BUF;
	msg.msg_namelen = __len;
]],
			uses_len = true,
		},
		{
			func = "recvmsg",
			variant = "msg_iov",
			arguments = {
				"sock[0]",
				"&msg",
				"0",
			},
			exclude = excludes_stack_overflow,
			stackvars = recvmsg_stackvars .. "\tstruct iovec iov[2];\n",
			init = [[
	memset(&msg, 0, sizeof(msg));
	memset(&iov[0], 0, sizeof(iov));

	/*
	 * We position the buffer second just so that we can confirm that the
	 * fortification bits are traversing the iovec correctly.
	 */
	iov[1].iov_base = BUF;
	iov[1].iov_len = __len;

	msg.msg_iov = &iov[0];
	msg.msg_iovlen = nitems(iov);
]],
			uses_len = true,
		},
		{
			func = "recvmsg",
			variant = "msg_control",
			bufsize = "CMSG_SPACE(sizeof(int))",
			arguments = {
				"sock[0]",
				"&msg",
				"0",
			},
			exclude = excludes_stack_overflow,
			stackvars = recvmsg_stackvars,
			init = [[
	memset(&msg, 0, sizeof(msg));

	msg.msg_control = BUF;
	msg.msg_controllen = __len;
]],
			uses_len = true,
		},
		{
			func = "recvmmsg",
			variant = "msgvec",
			buftype = "struct mmsghdr[]",
			bufsize = "2",
			arguments = {
				"sock[0]",
				"__buf",
				"__len",
				"0",
				"NULL",
			},
			stackvars = socket_stackvars,
		},
		{
			-- We'll assume that recvmsg is covering msghdr
			-- validation thoroughly enough, we'll just try tossing
			-- an error in the second element of a msgvec to try and
			-- make sure that each one is being validated.
			func = "recvmmsg",
			variant = "msghdr",
			arguments = {
				"sock[0]",
				"&msgvec[0]",
				"nitems(msgvec)",
				"0",
				"NULL",
			},
			exclude = excludes_stack_overflow,
			stackvars = socket_stackvars .. "\tstruct mmsghdr msgvec[2];\n",
			init = [[
	memset(&msgvec[0], 0, sizeof(msgvec));

	/*
	 * Same as above, make sure fortification isn't ignoring n > 1 elements
	 * of the msgvec.
	 */
	msgvec[1].msg_hdr.msg_control = BUF;
	msgvec[1].msg_hdr.msg_controllen = __len;
]],
			uses_len = true,
		},
	},
	uio = {
		-- <sys/uio.h>
		{
			func = "readv",
			buftype = "struct iovec[]",
			bufsize = 2,
			arguments = {
				"STDIN_FILENO",
				"__buf",
				"__len",
			},
		},
		{
			func = "readv",
			variant = "iov",
			arguments = {
				"STDIN_FILENO",
				"iov",
				"nitems(iov)",
			},
			exclude = excludes_stack_overflow,
			stackvars = readv_stackvars,
			init = readv_init,
			uses_len = true,
		},
		{
			func = "preadv",
			buftype = "struct iovec[]",
			bufsize = 2,
			arguments = {
				"STDIN_FILENO",
				"__buf",
				"__len",
				"0",
			},
		},
		{
			func = "preadv",
			variant = "iov",
			arguments = {
				"STDIN_FILENO",
				"iov",
				"nitems(iov)",
				"0",
			},
			exclude = excludes_stack_overflow,
			stackvars = readv_stackvars,
			init = readv_init,
			uses_len = true,
		},
	},
	poll = {
		-- <poll.h>
		{
			func = "poll",
			bufsize = "4",
			buftype = "struct pollfd[]",
			arguments = {
				"__buf",
				"__len",
				"0",
			},
			init = poll_init,
		},
		{
			func = "ppoll",
			bufsize = "4",
			buftype = "struct pollfd[]",
			arguments = {
				"__buf",
				"__len",
				"&tv",
				"NULL",
			},
			stackvars = "\tstruct timespec tv = { 0 };\n",
			init = poll_init,
		},
	},
	signal = {
		-- <signal.h>
		{
			func = "sig2str",
			bufsize = "SIG2STR_MAX",
			arguments = {
				"1",
				"__buf",
			},
			exclude = excludes_stack_overflow,
		},
	},
	stdio = {
		-- <stdio.h>
		{
			func = "ctermid",
			bufsize = "L_ctermid",
			arguments = {
				"__buf",
			},
			exclude = excludes_stack_overflow,
		},
		{
			func = "ctermid_r",
			bufsize = "L_ctermid",
			arguments = {
				"__buf",
			},
			exclude = excludes_stack_overflow,
		},
		{
			func = "fread",
			arguments = {
				"__buf",
				"__len",
				"1",
				"stdin",
			},
			exclude = excludes_stack_overflow,
			init = stdio_init,
		},
		{
			func = "fread_unlocked",
			arguments = {
				"__buf",
				"__len",
				"1",
				"stdin",
			},
			exclude = excludes_stack_overflow,
			init = stdio_init,
		},
		{
			func = "gets_s",
			arguments = {
				"__buf",
				"__len",
			},
			exclude = excludes_stack_overflow,
			init = stdio_init,
		},
		{
			func = "sprintf",
			arguments = {
				"__buf",
				"\"%.*s\"",
				"(int)__len - 1",	-- - 1 for NUL terminator
				"srcvar",
			},
			exclude = excludes_stack_overflow,
			stackvars = printf_stackvars,
			init = printf_init,
		},
		{
			func = "snprintf",
			arguments = {
				"__buf",
				"__len",
				"\"%.*s\"",
				"(int)__len - 1",	-- - 1 for NUL terminator
				"srcvar",
			},
			exclude = excludes_stack_overflow,
			stackvars = printf_stackvars,
			init = printf_init,
		},
		{
			func = "tmpnam",
			bufsize = "L_tmpnam",
			arguments = {
				"__buf",
			},
			exclude = excludes_stack_overflow,
		},
		{
			func = "fgets",
			arguments = {
				"__buf",
				"__len",
				"fp",
			},
			exclude = excludes_stack_overflow,
			stackvars = "\tFILE *fp;\n",
			init = [[
	fp = new_fp(__len);
]],
		},
	},
	stdlib = {
		-- <stdlib.h>
		{
			func = "arc4random_buf",
			arguments = {
				"__buf",
				"__len",
			},
			exclude = excludes_stack_overflow,
		},
		{
			func = "getenv_r",
			arguments = {
				"\"PATH\"",
				"__buf",
				"__len",
			},
			exclude = excludes_stack_overflow,
		},
		{
			func = "realpath",
			bufsize = "PATH_MAX",
			arguments = {
				"\".\"",
				"__buf",
			},
			exclude = excludes_stack_overflow,
		},
	},
	string = {
		-- <string.h>
		{
			func = "memcpy",
			arguments = {
				"__buf",
				"src",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = "\tchar src[__len + 10];\n",
		},
		{
			func = "mempcpy",
			arguments = {
				"__buf",
				"src",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = "\tchar src[__len + 10];\n",
		},
		{
			func = "memmove",
			arguments = {
				"__buf",
				"src",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = "\tchar src[__len + 10];\n",
		},
		{
			func = "memset",
			arguments = {
				"__buf",
				"0",
				"__len",
			},
			exclude = excludes_stack_overflow,
		},
		{
			func = "memset_explicit",
			arguments = {
				"__buf",
				"0",
				"__len",
			},
			exclude = excludes_stack_overflow,
		},
		{
			func = "stpcpy",
			arguments = {
				"__buf",
				"src",
			},
			exclude = excludes_stack_overflow,
			stackvars = string_stackvars,
			init = string_init,
			uses_len = true,
		},
		{
			func = "stpncpy",
			arguments = {
				"__buf",
				"src",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = string_stackvars,
			init = string_init,
		},
		{
			func = "strcat",
			arguments = {
				"__buf",
				"src",
			},
			exclude = excludes_stack_overflow,
			stackvars = string_stackvars,
			init = string_init,
			uses_len = true,
		},
		{
			func = "strlcat",
			arguments = {
				"__buf",
				"src",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = string_stackvars,
			init = string_init,
		},
		{
			func = "strncat",
			arguments = {
				"__buf",
				"src",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = string_stackvars,
			init = string_init,
		},
		{
			func = "strcpy",
			arguments = {
				"__buf",
				"src",
			},
			exclude = excludes_stack_overflow,
			stackvars = string_stackvars,
			init = string_init,
			uses_len = true,
		},
		{
			func = "strlcpy",
			arguments = {
				"__buf",
				"src",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = string_stackvars,
			init = string_init,
		},
		{
			func = "strncpy",
			arguments = {
				"__buf",
				"src",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = string_stackvars,
			init = string_init,
		},
	},
	strings = {
		-- <strings.h>
		{
			func = "bcopy",
			arguments = {
				"src",
				"__buf",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = "\tchar src[__len + 10];\n",
		},
		{
			func = "bzero",
			arguments = {
				"__buf",
				"__len",
			},
			exclude = excludes_stack_overflow,
		},
		{
			func = "explicit_bzero",
			arguments = {
				"__buf",
				"__len",
			},
			exclude = excludes_stack_overflow,
		},
	},
	unistd = {
		-- <unistd.h>
		{
			func = "getcwd",
			bufsize = "8",
			arguments = {
				"__buf",
				"__len",
			},
			exclude = excludes_stack_overflow,
		},
		{
			func = "getgrouplist",
			bufsize = "4",
			buftype = "gid_t[]",
			arguments = {
				"\"root\"",
				"0",
				"__buf",
				"&intlen",
			},
			exclude = excludes_stack_overflow,
			stackvars = "\tint intlen = (int)__len;\n",
			uses_len = true,
		},
		{
			func = "getgroups",
			bufsize = "4",
			buftype = "gid_t[]",
			arguments = {
				"__len",
				"__buf",
			},
			exclude = excludes_stack_overflow,
		},
		{
			func = "getloginclass",
			arguments = {
				"__buf",
				"__len",
			},
			exclude = excludes_stack_overflow,
		},
		{
			func = "pread",
			bufsize = "41",
			arguments = {
				"fd",
				"__buf",
				"__len",
				"0",
			},
			exclude = excludes_stack_overflow,
			stackvars = "\tint fd;\n",
			init = [[
	fd = new_tmpfile();	/* Cannot fail */
]],
		},
		{
			func = "read",
			bufsize = "41",
			arguments = {
				"fd",
				"__buf",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = "\tint fd;\n",
			init = [[
	fd = new_tmpfile();	/* Cannot fail */
]],
		},
		{
			func = "readlink",
			arguments = {
				"path",
				"__buf",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = "\tconst char *path;\n",
			init = [[
	path = new_symlink(__len);		/* Cannot fail */
]],
		},
		{
			func = "readlinkat",
			arguments = {
				"AT_FDCWD",
				"path",
				"__buf",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = "\tconst char *path;\n",
			init = [[
	path = new_symlink(__len);		/* Cannot fail */
]],
		},
		{
			func = "getdomainname",
			bufsize = #domainname + 1,
			arguments = {
				"__buf",
				"__len",
			},
			need_root = true,
			exclude = excludes_stack_overflow,
			early_init = "	dhost_jail();",
		},
		{
			func = "getentropy",
			arguments = {
				"__buf",
				"__len",
			},
			exclude = excludes_stack_overflow,
		},
		{
			func = "gethostname",
			bufsize = #hostname + 1,
			arguments = {
				"__buf",
				"__len",
			},
			need_root = true,
			exclude = excludes_stack_overflow,
			early_init = "	dhost_jail();",
		},
		{
			func = "getlogin_r",
			bufsize = "MAXLOGNAME + 1",
			arguments = {
				"__buf",
				"__len",
			},
			exclude = excludes_stack_overflow,
		},
		{
			func = "ttyname_r",
			arguments = {
				"fd",
				"__buf",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = "\tint fd;\n",
			early_init = [[
	fd = STDIN_FILENO;
	if (!isatty(fd))
		atf_tc_skip("stdin is not an fd");
]]
		},
	},
	wchar = {
		-- <wchar.h>
		{
			func = "wmemcpy",
			buftype = "wchar_t[]",
			arguments = {
				"__buf",
				"src",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = "\twchar_t src[__len + 10];\n",
		},
		{
			func = "wmempcpy",
			buftype = "wchar_t[]",
			arguments = {
				"__buf",
				"src",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = "\twchar_t src[__len + 10];\n",
		},
		{
			func = "wmemmove",
			buftype = "wchar_t[]",
			arguments = {
				"__buf",
				"src",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = "\twchar_t src[__len + 10];\n",
		},
		{
			func = "wmemset",
			buftype = "wchar_t[]",
			arguments = {
				"__buf",
				"L'0'",
				"__len",
			},
			exclude = excludes_stack_overflow,
		},
		{
			func = "wcpcpy",
			buftype = "wchar_t[]",
			arguments = {
				"__buf",
				"src",
			},
			exclude = excludes_stack_overflow,
			stackvars = wstring_stackvars,
			init = wstring_init,
			uses_len = true,
		},
		{
			func = "wcpncpy",
			buftype = "wchar_t[]",
			arguments = {
				"__buf",
				"src",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = wstring_stackvars,
			init = wstring_init,
		},
		{
			func = "wcscat",
			buftype = "wchar_t[]",
			arguments = {
				"__buf",
				"src",
			},
			exclude = excludes_stack_overflow,
			stackvars = wstring_stackvars,
			init = wstring_init,
			uses_len = true,
		},
		{
			func = "wcslcat",
			buftype = "wchar_t[]",
			arguments = {
				"__buf",
				"src",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = wstring_stackvars,
			init = wstring_init,
		},
		{
			func = "wcsncat",
			buftype = "wchar_t[]",
			arguments = {
				"__buf",
				"src",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = wstring_stackvars,
			init = wstring_init,
		},
		{
			func = "wcscpy",
			buftype = "wchar_t[]",
			arguments = {
				"__buf",
				"src",
			},
			exclude = excludes_stack_overflow,
			stackvars = wstring_stackvars,
			init = wstring_init,
			uses_len = true,
		},
		{
			func = "wcslcpy",
			buftype = "wchar_t[]",
			arguments = {
				"__buf",
				"src",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = wstring_stackvars,
			init = wstring_init,
		},
		{
			func = "wcsncpy",
			buftype = "wchar_t[]",
			arguments = {
				"__buf",
				"src",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = wstring_stackvars,
			init = wstring_init,
		},
	},
}

local function write_test_boilerplate(fh, name, body, def)
	fh:write("ATF_TC(" .. name .. ");\n")
	fh:write("ATF_TC_HEAD(" .. name .. ", tc)\n")
	fh:write("{\n")
	if def.need_root then
		fh:write("	atf_tc_set_md_var(tc, \"require.user\", \"root\");\n")
	end
	fh:write("}\n")

	fh:write("ATF_TC_BODY(" .. name .. ", tc)\n")
	fh:write("{\n" .. body .. "\n}\n\n")
	return name
end

local function generate_test_name(func, variant, disposition, heap)
	local basename = func
	if variant then
		basename = basename .. "_" .. variant
	end
	if heap then
		basename = basename .. "_heap"
	end
	if disposition < 0 then
		return basename .. "_before_end"
	elseif disposition == 0 then
		return basename .. "_end"
	else
		return basename .. "_after_end"
	end
end

local function array_type(buftype)
	if not buftype:match("%[%]") then
		return nil
	end

	return buftype:gsub("%[%]", "")
end

local function configurable(def, idx)
	local cfgitem = def[idx]

	if not cfgitem then
		return nil
	end

	if type(cfgitem) == "function" then
		return cfgitem()
	end

	return cfgitem
end

local function generate_stackframe(buftype, bufsize, disposition, heap, def)
	local function len_offset(inverted)
		-- Tests that don't use __len in their arguments may use an
		-- inverted sense because we can't just specify a length that
		-- would induce an access just after the end.  Instead, we have
		-- to manipulate the buffer size to be too short so that the
		-- function under test would write one too many.
		if disposition < 0 then
			return ((inverted and " + ") or " - ") .. "1"
		elseif disposition == 0 then
			return ""
		else
			return ((inverted and " - ") or " + ") .. "1"
		end
	end

	local function test_uses_len()
		if def.uses_len then
			return true
		end

		for _, arg in ipairs(def.arguments) do
			if arg:match("__len") or arg:match("__idx") then
				return true
			end
		end

		return false
	end


	-- This is perhaps a little convoluted, but we toss the buffer into a
	-- struct on the stack to guarantee that we have at least one valid
	-- byte on either side of the buffer -- a measure to make sure that
	-- we're tripping _FORTIFY_SOURCE specifically in the buffer + 1 case,
	-- rather than some other stack or memory protection.
	local vars = "\tstruct {\n"
	vars = vars .. "\t\tuint8_t padding_l;\n"

	local uses_len = test_uses_len()
	local bufsize_offset = len_offset(not uses_len)
	local buftype_elem = array_type(buftype)
	local size_expr = bufsize

	if not uses_len then
		-- If the length isn't in use, we have to vary the buffer size
		-- since the fortified function likely has some internal size
		-- constraint that it's supposed to be checking.
		size_expr = size_expr .. bufsize_offset
	end

	if not heap and buftype_elem then
		-- Array type: size goes after identifier
		vars = vars .. "\t\t" .. buftype_elem ..
		    " __buf[" .. size_expr .. "];\n"
	else
		local basic_type = buftype_elem or buftype

		-- Heap tests obviously just put a pointer on the stack that
		-- points to our new allocation, but we leave it in the padded
		-- struct just to simplify our generator.
		if heap then
			basic_type = basic_type .. " *"
		end
		vars = vars .. "\t\t" .. basic_type .. " __buf;\n"
	end

	-- padding_r is our just-past-the-end padding that we use to make sure
	-- that there's a valid portion after the buffer that isn't being
	-- included in our function calls.  If we didn't have it, then we'd have
	-- a hard time feeling confident that an abort on the just-after tests
	-- isn't maybe from some other memory or stack protection.
	vars = vars .. "\t\tuint8_t padding_r;\n"
	vars = vars .. "\t} __stack;\n"

	-- Not all tests will use __bufsz, but some do for, e.g., clearing
	-- memory..
	vars = vars .. "\tconst size_t __bufsz __unused = "
	if heap then
		local scalar = 1
		if buftype_elem then
			scalar = size_expr
		end

		vars = vars .. "sizeof(*__stack.__buf) * (" .. scalar .. ");\n"
	else
		vars = vars .. "sizeof(__stack.__buf);\n"
	end

	vars = vars .. "\tconst size_t __len = " .. bufsize ..
	    bufsize_offset .. ";\n"
	vars = vars .. "\tconst size_t __idx __unused = __len - 1;\n"

	-- For overflow testing, we need to fork() because we're expecting the
	-- test to ultimately abort()/_exit().  Then we can collect the exit
	-- status and report appropriately.
	if disposition > 0 then
		vars = vars .. "\tpid_t __child;\n"
		vars = vars .. "\tint __status;\n"
	end

	-- Any other stackvars defined by the test get placed after everything
	-- else.
	vars = vars .. (configurable(def, "stackvars") or "")

	return vars
end

local function write_test(fh, func, disposition, heap, def)
	local testname = generate_test_name(func, def.variant, disposition, heap)
	local buftype = def.buftype or "unsigned char[]"
	local bufsize = def.bufsize or 42
	local body = ""

	if def.exclude and def.exclude(disposition, heap) then
		return
	end

	local function need_addr()
		return not (buftype:match("%[%]") or buftype:match("%*"))
	end

	if heap then
		body = body .. "#define BUF __stack.__buf\n"
	else
		body = body .. "#define BUF &__stack.__buf\n"
	end

	-- Setup the buffer
	body = body .. generate_stackframe(buftype, bufsize, disposition, heap, def) ..
	    "\n"

	-- Any early initialization goes before we would fork for the just-after
	-- tests, because they may want to skip the test based on some criteria
	-- and we can't propagate that up very easily once we're forked.
	local early_init = configurable(def, "early_init")
	body = body .. (early_init or "")
	if early_init then
		body = body .. "\n"
	end

	-- Fork off, iff we're testing some access past the end of the buffer.
	if disposition > 0 then
		body = body .. [[
	__child = fork();
	ATF_REQUIRE(__child >= 0);
	if (__child > 0)
		goto monitor;

	/* Child */
	disable_coredumps();
]]
	end

	local bufvar = "__stack.__buf"
	if heap then
		-- Buffer needs to be initialized because it's a heap allocation.
		body = body .. "\t" .. bufvar .. " = malloc(__bufsz);\n"
	end

	-- Non-early init happens just after the fork in the child, not the
	-- monitor.  This is used to setup any other buffers we may need, for
	-- instance.
	local extra_init = configurable(def, "init")
	body = body .. (extra_init or "")

	if heap or extra_init then
		body = body .. "\n"
	end

	-- Setup the function call with arguments as described in the test
	-- definition.
	body = body .. "\t" .. func .. "("

	for idx, arg in ipairs(def.arguments) do
		if idx > 1 then
			body = body .. ", "
		end

		if arg == "__buf" then
			if not heap and need_addr() then
				body = body .. "&"
			end

			body = body .. bufvar
		else
			local argname = arg

			if def.value_of then
				argname = argname or def.value_of(arg)
			end

			body = body .. argname
		end
	end

	body = body .. ");\n"

	-- Monitor stuff follows, for OOB access.
	if disposition <= 0 then
		goto skip
	end

	body = body .. [[
	_exit(EX_SOFTWARE);	/* Should have aborted. */

monitor:
	while (waitpid(__child, &__status, 0) != __child) {
		ATF_REQUIRE_EQ(EINTR, errno);
	}

	if (!WIFSIGNALED(__status)) {
		switch (WEXITSTATUS(__status)) {
		case EX_SOFTWARE:
			atf_tc_fail("FORTIFY_SOURCE failed to abort");
			break;
		case EX_OSERR:
			atf_tc_fail("setrlimit(2) failed");
			break;
		default:
			atf_tc_fail("child exited with status %d",
			    WEXITSTATUS(__status));
		}
	} else {
		ATF_REQUIRE_EQ(SIGABRT, WTERMSIG(__status));
	}
]]

::skip::
	body = body .. "#undef BUF\n"
	return write_test_boilerplate(fh, testname, body, def)
end

-- main()
local tests
local tcat = assert(arg[1], "usage: generate-fortify-tests.lua <category>")
for k, defs in pairs(all_tests) do
	if k == tcat then
		tests = defs
		break
	end
end

assert(tests, "category " .. tcat .. " not found")

local fh = io.stdout
fh:write("/* @" .. "generated" .. " by `generate-fortify-tests.lua \"" ..
    tcat .. "\"` */\n\n")
fh:write("#define	_FORTIFY_SOURCE	2\n")
fh:write("#define	TMPFILE_SIZE	(1024 * 32)\n")

fh:write("\n")
for _, inc in ipairs(includes) do
	fh:write("#include <" .. inc .. ">\n")
end

fh:write([[

static FILE * __unused
new_fp(size_t __len)
{
	static char fpbuf[LINE_MAX];
	FILE *fp;

	ATF_REQUIRE(__len <= sizeof(fpbuf));

	memset(fpbuf, 'A', sizeof(fpbuf) - 1);
	fpbuf[sizeof(fpbuf) - 1] = '\0';

	fp = fmemopen(fpbuf, sizeof(fpbuf), "rb");
	ATF_REQUIRE(fp != NULL);

	return (fp);
}

/*
 * Create a new symlink to use for readlink(2) style tests, we'll just use a
 * random target name to have something interesting to look at.
 */
static const char * __unused
new_symlink(size_t __len)
{
	static const char linkname[] = "link";
	char target[MAXNAMLEN];
	int error;

	ATF_REQUIRE(__len <= sizeof(target));

	arc4random_buf(target, sizeof(target));

	error = unlink(linkname);
	ATF_REQUIRE(error == 0 || errno == ENOENT);

	error = symlink(target, linkname);
	ATF_REQUIRE(error == 0);

	return (linkname);
}

/*
 * For our purposes, first descriptor will be the reader; we'll send both
 * raw data and a control message over it so that the result can be used for
 * any of our recv*() tests.
 */
static void __unused
new_socket(int sock[2])
{
	unsigned char ctrl[CMSG_SPACE(sizeof(int))] = { 0 };
	static char sockbuf[256];
	ssize_t rv;
	size_t total = 0;
	struct msghdr hdr = { 0 };
	struct cmsghdr *cmsg;
	int error, fd;

	error = socketpair(AF_UNIX, SOCK_STREAM, 0, sock);
	ATF_REQUIRE(error == 0);

	while (total != sizeof(sockbuf)) {
		rv = send(sock[1], &sockbuf[total], sizeof(sockbuf) - total, 0);

		ATF_REQUIRE_MSG(rv > 0,
		    "expected bytes sent, got %zd with %zu left (size %zu, total %zu)",
		    rv, sizeof(sockbuf) - total, sizeof(sockbuf), total);
		ATF_REQUIRE_MSG(total + (size_t)rv <= sizeof(sockbuf),
		    "%zd exceeds total %zu", rv, sizeof(sockbuf));
		total += rv;
	}

	hdr.msg_control = ctrl;
	hdr.msg_controllen = sizeof(ctrl);

	cmsg = CMSG_FIRSTHDR(&hdr);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
	fd = STDIN_FILENO;
	memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));

	error = sendmsg(sock[1], &hdr, 0);
	ATF_REQUIRE(error != -1);
}

/*
 * Constructs a tmpfile that we can use for testing read(2) and friends.
 */
static int __unused
new_tmpfile(void)
{
	char buf[1024];
	ssize_t rv;
	size_t written;
	int fd;

	fd = open("tmpfile", O_RDWR | O_CREAT | O_TRUNC, 0644);
	ATF_REQUIRE(fd >= 0);

	written = 0;
	while (written < TMPFILE_SIZE) {
		rv = write(fd, buf, sizeof(buf));
		ATF_REQUIRE(rv > 0);

		written += rv;
	}

	ATF_REQUIRE_EQ(0, lseek(fd, 0, SEEK_SET));
	return (fd);
}

static void
disable_coredumps(void)
{
	struct rlimit rl = { 0 };

	if (setrlimit(RLIMIT_CORE, &rl) == -1)
		_exit(EX_OSERR);
}

/*
 * Replaces stdin with a file that we can actually read from, for tests where
 * we want a FILE * or fd that we can get data from.
 */
static void __unused
replace_stdin(void)
{
	int fd;

	fd = new_tmpfile();

	(void)dup2(fd, STDIN_FILENO);
	if (fd != STDIN_FILENO)
		close(fd);
}

]])

if tcat == "unistd" then
	fh:write("#define	JAIL_HOSTNAME	\"" .. hostname .. "\"\n")
	fh:write("#define	JAIL_DOMAINNAME	\"" .. domainname .. "\"\n")
	fh:write([[
static void
dhost_jail(void)
{
	struct iovec iov[4];
	int jid;

	iov[0].iov_base = __DECONST(char *, "host.hostname");
	iov[0].iov_len = sizeof("host.hostname");
	iov[1].iov_base = __DECONST(char *, JAIL_HOSTNAME);
	iov[1].iov_len = sizeof(JAIL_HOSTNAME);
	iov[2].iov_base = __DECONST(char *, "host.domainname");
	iov[2].iov_len = sizeof("host.domainname");
	iov[3].iov_base = __DECONST(char *, JAIL_DOMAINNAME);
	iov[3].iov_len = sizeof(JAIL_DOMAINNAME);

	jid = jail_set(iov, nitems(iov), JAIL_CREATE | JAIL_ATTACH);
	ATF_REQUIRE_MSG(jid > 0, "Jail creation failed: %s", strerror(errno));
}

]])
end

for _, def in pairs(tests) do
	local func = def.func
	local function write_tests(heap)
		-- Dispositions here are relative to the buffer size prescribed
		-- by the test definition.
		local dispositions = def.dispositions or { -1, 0, 1 }

		for _, disposition in ipairs(dispositions) do
			tests_added[#tests_added + 1] = write_test(fh, func, disposition, heap, def)
		end
	end

	write_tests(false)
	write_tests(true)
end

fh:write("ATF_TP_ADD_TCS(tp)\n")
fh:write("{\n")
for idx = 1, #tests_added do
	fh:write("\tATF_TP_ADD_TC(tp, " .. tests_added[idx] .. ");\n")
end
fh:write("\treturn (atf_no_error());\n")
fh:write("}\n")
