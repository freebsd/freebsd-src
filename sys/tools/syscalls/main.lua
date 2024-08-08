#!/usr/libexec/flua
--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2024 Tyler Baxter <agge@FreeBSD.org>
-- Copyright (c) 2023 Warner Losh <imp@bsdimp.com>
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--
-- Thanks to Kyle Evans for his makesyscall.lua in FreeBSD which served as
-- inspiration for this, and as a source of code at times.
--

-- We generally assume that this script will be run by flua, however we've
-- carefully crafted modules for it that mimic interfaces provided by modules
-- available in ports.  Currently, this script is compatible with lua from ports
-- along with the compatible luafilesystem and lua-posix modules.

-- When we have a path, add it to the package.path (. is already in the list)
if arg[0]:match("/") then
	local path = arg[0]:gsub("/[^/]+.lua$", "")
	package.path = package.path .. ";" .. path .. "/?.lua"
end

-- Common config file management
local config = require("config")
-- FreeBSDSyscall generates a table of system calls from syscalls.master
local FreeBSDSyscall = require("core.freebsd-syscall")

-- Modules for each file:
local syscalls = require("scripts.syscalls")
local syscall_h = require("scripts.syscall_h")
local syscall_mk = require("scripts.syscall_mk")
local init_sysent = require("scripts.init_sysent")
local systrace_args = require("scripts.systrace_args")
local sysproto_h = require("scripts.sysproto_h")

-- Entry
if #arg < 1 or #arg > 2 then
	error("usage: " .. arg[0] .. " syscall.master")
end

local sysfile, configfile = arg[1], arg[2]

config.merge(configfile)
config.mergeCompat()
config.mergeCapability()

local tbl = FreeBSDSyscall:new{sysfile = sysfile, config = config}

-- Output files
syscalls.file = config.sysnames
syscall_h.file = config.syshdr
syscall_mk.file = config.sysmk
init_sysent.file = config.syssw
systrace_args.file = config.systrace
sysproto_h.file = config.sysproto

syscalls.generate(tbl, config, syscalls.file)
syscall_h.generate(tbl, config, syscall_h.file)
syscall_mk.generate(tbl, config, syscall_mk.file)
init_sysent.generate(tbl, config, init_sysent.file)
systrace_args.generate(tbl, config, systrace_args.file)
sysproto_h.generate(tbl, config, sysproto_h.file)
