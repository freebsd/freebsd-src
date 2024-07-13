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
	"sys/resource.h",
	"sys/time.h",
	"sys/wait.h",
	"dirent.h",
	"errno.h",
	"fcntl.h",
	"limits.h",
	"signal.h",
	"stdio.h",
	"stdlib.h",
	"string.h",
	"strings.h",
	"sysexits.h",
	"unistd.h",
	"atf-c.h",
}

local tests_added = {}

-- Some of these will need to be excluded because clang sees the wrong size when
-- an array is embedded inside a struct, we'll get something that looks more
-- like __builtin_object_size(ptr, 0) than it does the correct
-- __builtin_object_size(ptr, 1) (i.e., includes the padding after).  This is
-- almost certainly a bug in llvm.
local function excludes_stack_overflow(disposition, is_heap)
	return (not is_heap) and disposition > 0
end

local printf_stackvars = "\tchar srcvar[__len + 10];\n"
local printf_init = [[
	memset(srcvar, 'A', sizeof(srcvar) - 1);
	srcvar[sizeof(srcvar) - 1] = '\0';
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
			bufsize = "4",
			arguments = {
				"__buf",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = "\tchar sysdomain[256];\n",
			early_init = [[
	(void)getdomainname(sysdomain, __len);
	if (strlen(sysdomain) <= __len)
		atf_tc_skip("domain name too short for testing");
]]
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
			bufsize = "4",
			arguments = {
				"__buf",
				"__len",
			},
			exclude = excludes_stack_overflow,
			stackvars = [[
	char syshost[256];
	int error;
]],
			early_init = [[
	error = gethostname(syshost, __len);
	if (error != 0 || strlen(syshost) <= __len)
		atf_tc_skip("hostname too short for testing");
]]
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
}

local function write_test_boilerplate(fh, name, body)
	fh:write("ATF_TC_WITHOUT_HEAD(" .. name .. ");\n")
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
	local function len_offset(inverted, disposition)
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

	local function test_uses_len(def)
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

	local uses_len = test_uses_len(def)
	local bufsize_offset = len_offset(not uses_len, disposition)
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

	local function need_addr(buftype)
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
			if not heap and need_addr(buftype) then
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
	return write_test_boilerplate(fh, testname, body)
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
