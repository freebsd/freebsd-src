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

-- When we have a path, add it to the package.path (. is already in the list).
if arg[0]:match("/") then
	local path = arg[0]:gsub("/[^/]+.lua$", "")
	package.path = package.path .. ";" .. path .. "/?.lua"
end

-- Common config file management.
local config = require("config")
-- FreeBSDSyscall generates a table of system calls from syscalls.master.
local FreeBSDSyscall = require("core.freebsd-syscall")

-- Modules for each file:
local init_sysent = require("scripts.init_sysent")
local libsys_h = require("scripts.libsys_h")
local syscall_h = require("scripts.syscall_h")
local syscall_mk = require("scripts.syscall_mk")
local syscalls = require("scripts.syscalls")
local syscalls_map = require("scripts.syscalls_map")
local sysproto_h = require("scripts.sysproto_h")
local systrace_args = require("scripts.systrace_args")

-- Entry:
if #arg < 1 or #arg > 2 then
	error("usage: " .. arg[0] .. " syscall.master")
end

local sysfile, configfile = arg[1], arg[2]

config.merge(configfile)
config.mergeCompat()

local tbl = FreeBSDSyscall:new{sysfile = sysfile, config = config}

-- Output files:
init_sysent.file = config.syssw
libsys_h.file = config.libsys_h
syscall_h.file = config.syshdr
syscall_mk.file = config.sysmk
syscalls.file = config.sysnames
syscalls_map.file = config.libsysmap
sysproto_h.file = config.sysproto
systrace_args.file = config.systrace

init_sysent.generate(tbl, config, init_sysent.file)
libsys_h.generate(tbl, config, libsys_h.file)
syscall_h.generate(tbl, config, syscall_h.file)
syscall_mk.generate(tbl, config, syscall_mk.file)
syscalls.generate(tbl, config, syscalls.file)
syscalls_map.generate(tbl, config, syscalls_map.file)
sysproto_h.generate(tbl, config, sysproto_h.file)
systrace_args.generate(tbl, config, systrace_args.file)
