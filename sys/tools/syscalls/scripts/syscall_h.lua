#!/usr/libexec/flua
--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2024 Tyler Baxter <agge@FreeBSD.org>
-- Copyright (c) 2023 Warner Losh <imp@bsdimp.com>
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--

-- We generally assume that this script will be run by flua, however we've
-- carefully crafted modules for it that mimic interfaces provided by modules
-- available in ports.  Currently, this script is compatible with lua from
-- ports along with the compatible luafilesystem and lua-posix modules.

-- Setup to be a module, or ran as its own script.
local syscall_h = {}
local script = not pcall(debug.getlocal, 4, 1) -- TRUE if script.

if script then
    -- Add library root to the package path.
    local path = arg[0]:gsub("/[^/]+.lua$", "")
    package.path = package.path .. ";" .. path .. "/../?.lua"  
end

local config = require("config")
local FreeBSDSyscall = require("core.freebsd-syscall")
local util = require("tools.util")
local bsdio = require("tools.generator")

-- Globals
-- File has not been decided yet; config will decide file. Default defined as
-- null.
syscall_h.file = "/dev/null"

-- Libc has all the STD, NOSTD and SYSMUX system calls in it, as well as
-- replaced system calls dating back to FreeBSD 7. We are lucky that the
-- system call filename is just the base symbol name for it.
function syscall_h.generate(tbl, config, fh)
    -- Grab the master syscalls table, and prepare bookkeeping for the max
    -- syscall number.
    local s = tbl.syscalls
    local max = 0

    -- Init the bsdio object, has macros and procedures for LSG specific io.
    local bio = bsdio:new({}, fh) 

    -- Write the generated tag.
	bio:generated("System call numbers.")

	for k, v in pairs(s) do
		local c = v:compat_level()
		if v.num > max then
			max = v.num
		end
		if  v.type.STD or
			v.type.NOSTD or
			v.type.SYSMUX or
			c >= 7 then
			bio:write(string.format("#define\t%s%s\t%d\n", 
                config.syscallprefix, v:symbol(), v.num))
		elseif c >= 0 then
			local s
			if c == 0 then
				s = "obsolete"
			elseif c == 3 then
				s = "old"
			else
				s = "freebsd" .. c
			end
			bio:write(string.format("\t\t\t\t/* %d is %s %s */\n", 
                v.num, s, v.name))
		elseif v.type.RESERVED then
			bio:write(string.format("\t\t\t\t/* %d is reserved */\n", v.num))
		elseif v.type.UNIMPL then
			bio:write(string.format("\t\t\t\t/* %d is unimplemented %s */\n", 
                v.num, v.name))
		else -- do nothing
		end
	end
	bio:write(string.format("#define\t%sMAXSYSCALL\t%d\n", 
        config.syscallprefix, max + 1))
end

-- Entry of script:
if script then
    if #arg < 1 or #arg > 2 then
    	error("usage: " .. arg[0] .. " syscall.master")
    end
    
    local sysfile, configfile = arg[1], arg[2]
    
    config.merge(configfile)
    config.mergeCompat()
    config.mergeCapability()
    
    -- The parsed syscall table
    local tbl = FreeBSDSyscall:new{sysfile = sysfile, config = config}
   
    syscall_h.file = config.syshdr -- change file here
    syscall_h.generate(tbl, config, syscall_h.file)
end

-- Return the module
return syscall_h
