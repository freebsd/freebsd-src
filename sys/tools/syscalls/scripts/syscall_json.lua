#!/usr/libexec/flua
--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2026 Warner Losh <imp@bsdimp.com>
--

-- Setup to be a module, or ran as its own script.
local syscall_json = {}
local script = not pcall(debug.getlocal, 4, 1)	-- TRUE if script.
if script then
	-- Add library root to the package path.
	local path = arg[0]:gsub("/[^/]+.lua$", "")
	package.path = package.path .. ";" .. path .. "/../?.lua"
end

local FreeBSDSyscall = require("core.freebsd-syscall")
local ucl = require("ucl")

-- Convert the type flags set (table with flag=true entries) to a sorted list.
local function flagsToList(typetbl)
	local flags = {}
	for k, _ in pairs(typetbl) do
		table.insert(flags, k)
	end
	table.sort(flags)
	return flags
end

-- Convert a single syscall object to a plain table suitable for JSON export.
-- Much of the data is available only as a method call.
local function syscallToTable(v)
	local entry = {
		num = v.num,
		name = v.name or "",
		alias = v.alias or "",
		audit = v.audit or "",
		flags = flagsToList(v.type),
		compat_level = v:compatLevel(),
		compat_prefix = v:compatPrefix(),
		symbol = v:symbol(),
		rettype = v.rettype or "int",
		cap = v.cap or "0",
		thr = v.thr or "SY_THR_STATIC",
		changes_abi = v.changes_abi or false,
		noproto = v.noproto or false,
		args_size = v.args_size or "0",
		arg_alias = v.arg_alias or "",
	}

	-- Export arguments with annotations.
	local args = {}
	if v.args ~= nil then
		for _, a in ipairs(v.args) do
			arg = {
				type = a.type,
				name = a.name,
			}
			if a.annotation ~= nil and a.annotation ~= "" then
				arg.annotation = a.annotation
			end
			table.insert(args, arg)
		end
	end
	entry.args = args

	-- Export altname/alttag/rettype if present (loadable syscalls).
	if v.altname ~= nil then
		entry.altname = v.altname
	end
	if v.alttag ~= nil then
		entry.alttag = v.alttag
	end

	return entry
end

function syscall_json.generate(tbl, config)
	-- Build the syscalls array.
	local syscalls = {}
	for _, v in pairs(tbl.syscalls) do
		table.insert(syscalls, syscallToTable(v))
	end

	-- Build the structs data into a nicer structure
	local structs = {}
	if tbl.structs ~= nil then
		for k, _ in pairs(tbl.structs) do
			table.insert(structs, k)
		end
		table.sort(structs)
	end

	local root = {
		syscalls = syscalls,
		structs = structs,
	}

	local json = ucl.to_json(root)

	-- Write to stdout.
	io.write(json)
	io.write("\n")
end

-- Entry of script:
if script then
	local config = require("config")

	if #arg < 1 or #arg > 2 then
		error("usage: " .. arg[0] .. " syscall.master [config]")
	end

	local sysfile, configfile = arg[1], arg[2]

	config.merge(configfile)
	config.mergeCompat()

	-- The parsed system call table.
	local tbl = FreeBSDSyscall:new{sysfile = sysfile, config = config}

	syscall_json.generate(tbl, config)
end

-- Return the module.
return syscall_json
