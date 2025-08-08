#!/usr/libexec/flua
--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2024 Tyler Baxter <agge@FreeBSD.org>
-- Copyright (c) 2023 Warner Losh <imp@bsdimp.com>
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--

-- Setup to be a module, or ran as its own script.
local syscall_h = {}
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
syscall_h.file = "/dev/null"

-- Libc has all the STD, NOSTD and SYSMUX system calls in it, as well as
-- replaced system calls dating back to FreeBSD 7. We are lucky that the
-- system call filename is just the base symbol name for it.
function syscall_h.generate(tbl, config, fh)
	-- Grab the master system calls table, and prepare bookkeeping for
	-- the max system call number.
	local s = tbl.syscalls
	local max = 0

	-- Bind the generator to the parameter file.
	local gen = generator:new({}, fh)

	-- Write the generated preamble.
	gen:preamble("System call numbers.")

	if config.syshdr_extra then
		gen:write(string.format("%s\n\n", config.syshdr_extra))
	end

	for _, v in pairs(s) do
		local c = v:compatLevel()
		if v.num > max then
			max = v.num
		end
		if v.type.UNIMPL then
			goto skip
		elseif v.type.RESERVED then
			goto skip
		elseif v.type.NODEF then
			goto skip
		elseif v.type.STD or v.type.NOSTD or v.type.SYSMUX or
		    c >= 7 then
			gen:write(string.format("#define\t%s%s%s\t%d\n",
			    config.syscallprefix, v:compatPrefix(), v.name,
			    v.num))
		elseif c >= 0 then
			local comment
			if c == 0 then
				comment = "obsolete"
			elseif c == 3 then
				comment = "old"
			else
				comment = "freebsd" .. c
			end
			gen:write(string.format("\t\t\t\t/* %d is %s %s */\n",
			    v.num, comment, v.name))
		end
		::skip::
	end
	gen:write(string.format("#define\t%sMAXSYSCALL\t%d\n",
	    config.syscallprefix, max + 1))
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

	-- The parsed system call table.
	local tbl = FreeBSDSyscall:new{sysfile = sysfile, config = config}

	syscall_h.file = config.syshdr	-- change file here
	syscall_h.generate(tbl, config, syscall_h.file)
end

-- Return the module.
return syscall_h
