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
local syscalls_map = {}
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
syscalls_map.file = "/dev/null"

function syscalls_map.generate(tbl, config, fh)
	-- Grab the master system calls table.
	local s = tbl.syscalls

	-- Bind the generator to the parameter file.
	local gen = generator:new({}, fh)

	-- Write the generated preamble.
	gen:preamble("FreeBSD system call symbols.")

	gen:write(string.format("FBSDprivate_1.0 {\n"))

	for _, v in pairs(s) do
		gen:write(v.prolog)
		if v:native() and not v.type.NODEF and not v.type.NOLIB then
			if v.name ~= "_exit" and v.name ~= "vfork" then
				gen:write(string.format("\t_%s;\n", v.name))
			end
			gen:write(string.format("\t__sys_%s;\n", v.name))
		end
	end
	gen:write(tbl.epilog)

	-- End
	gen:write("};\n")
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

	syscalls_map.file = config.libsysmap	-- change file here
	syscalls_map.generate(tbl, config, syscalls_map.file)
end

-- Return the module.
return syscalls_map
