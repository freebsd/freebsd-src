#!/usr/libexec/flua
--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2024 Tyler Baxter <agge@FreeBSD.org>
-- Copyright (c) 2023 Warner Losh <imp@bsdimp.com>
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--

-- Setup to be a module, or ran as its own script.
local syscall_mk = {}
local script = not pcall(debug.getlocal, 4, 1)	-- TRUE if script.
if script then
	-- Add library root to the package path.
	local path = arg[0]:gsub("/[^/]+.lua$", "")
	package.path = package.path .. ";" .. path .. "/../?.lua"
end

local FreeBSDSyscall = require("core.freebsd-syscall")
local generator = require("tools.generator")

-- File has not been decided yet; config will decide file.  Default defined as
-- /dev/null.
syscall_mk.file = "/dev/null"

-- Libc has all the STD, NOSTD and SYSMUX system calls in it, as well as
-- replaced system calls dating back to FreeBSD 7. We are lucky that the
-- system call filename is just the base symbol name for it.
function syscall_mk.generate(tbl, config, fh)
	-- Grab the master system calls table.
	local s = tbl.syscalls
	-- Bookkeeping for keeping track of when we're at the last system
	-- call (no backslash).
	local size = #s
	local idx = 0

	-- Bind the generator to the parameter file.
	local gen = generator:new({}, fh)

	-- Write the generated preamble.
	gen:preamble("FreeBSD system call object files.", "#")

	gen:write("MIASM =  \\\n") -- preamble
	for _, v in pairs(s) do
		local c = v:compatLevel()
		local bs = " \\"
		idx = idx + 1
		if (v:native() or c >= 7) and not v.type.NODEF and not v.type.NOLIB then
			if idx >= size then
				-- At last system call, no backslash.
				bs = ""
			end
			gen:write(string.format("\t%s.o%s\n", v:symbol(), bs))
		end
	end
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

	syscall_mk.file = config.sysmk	-- change file here
	syscall_mk.generate(tbl, config, syscall_mk.file)
end

-- Return the module.
return syscall_mk
