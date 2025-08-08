#!/usr/libexec/flua
--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2024 SRI International
-- Copyright (c) 2024 Tyler Baxter <agge@FreeBSD.org>
-- Copyright (c) 2023 Warner Losh <imp@bsdimp.com>
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--

-- Setup to be a module, or ran as its own script.
local libsys_h = {}
local script = not pcall(debug.getlocal, 4, 1)	-- TRUE if script.
if script then
	-- Add library root to the package path.
	local path = arg[0]:gsub("/[^/]+.lua$", "")
	package.path = package.path .. ";" .. path .. "/../?.lua"
end

local FreeBSDSyscall = require("core.freebsd-syscall")
local generator = require("tools.generator")
local util = require("tools.util")

-- File has not been decided yet; config will decide file.  Default defined as
-- /dev/null.
libsys_h.file = "/dev/null"

function libsys_h.generate(tbl, config, fh)
	-- Grab the master system calls table.
	local s = tbl.syscalls

	local print_decl = function (sc)
		return sc:native() and not sc.type.NODEF and
		    not sc.type.NOLIB and not sc.type.SYSMUX
	end

	-- Bind the generator to the parameter file.
	local gen = generator:new({}, fh)

	-- Write the generated preamble.
	gen:preamble("Public system call stubs provided by libsys.\n" ..
	    "\n" ..
	    "Do not use directly, include <libsys.h> instead.")

	gen:write(string.format([[
#ifndef __LIBSYS_H_
#define __LIBSYS_H_

#include <sys/_cpuset.h>
#include <sys/_domainset.h>
#include <sys/_ffcounter.h>
#include <sys/_semaphore.h>
#include <sys/_sigaltstack.h>
#include <machine/ucontext.h>   /* for mcontext_t */
#include <sys/_ucontext.h>
#include <sys/wait.h>

]]))

	for name, _ in util.pairsByKeys(tbl.structs) do
		gen:write(string.format("struct %s;\n", name))
	end
	gen:write("union semun;\n")

	gen:write("\n__BEGIN_DECLS\n")

	for _, v in pairs(s) do
		if print_decl(v) then
			gen:write(string.format(
			    "typedef %s (__sys_%s_t)(%s);\n",
			    v.ret, v.name, v.argstr_type))
		end
	end

	gen:write("\n")

	for _, v in pairs(s) do
		if print_decl(v) then
			local ret_attr = "";
			if v.type.NORETURN then
				ret_attr = "_Noreturn "
			end
			gen:write(string.format("%s%s __sys_%s(%s);\n",
			    ret_attr, v.ret, v.name, v.argstr_type_var))
		end
	end

	gen:write("__END_DECLS\n")
	-- End
	gen:write("\n#endif /* __LIBSYS_H_ */\n")
end

-- Entry of script:
if script then
	local config = require("config")

	if #arg < 1 or #arg > 2 then
		error("usage: " .. arg[0] .. " syscall.master")
	end

	local sysfile, configfile = arg[1], arg[2]

	config.merge(configfile)
	config.mergeCompat()

	-- The parsed syscall table.
	local tbl = FreeBSDSyscall:new{sysfile = sysfile, config = config}

	libsys_h.file = config.libsys_h	-- change file here
	libsys_h.generate(tbl, config, libsys_h.file)
end

-- Return the module.
return libsys_h
