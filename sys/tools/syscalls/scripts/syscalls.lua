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
local syscalls = {}

local config = require("config")
local FreeBSDSyscall = require("core.freebsd-syscall")
local util = require("tools.util")
local bsdio = require("tools.generator")

-- Globals
-- File has not been decided yet; config will decide file. Default defined as
-- null
syscalls.file = "/dev/null"

function syscalls.generate(tbl, config, fh)
    -- Grab the master syscalls table, and prepare bookkeeping for the max
    -- syscall number.
    local s = tbl.syscalls
    local max = 0

    -- Init the bsdio object, has macros and procedures for LSG specific io.
    local bio = bsdio:new({ }, fh) 

    -- Write the generated tag.
	bio:generated("System call names.")

    bio:write(string.format("const char *%s[] = {\n", config.namesname))

	for k, v in pairs(s) do
		local c = v:compat_level()
		if v.num > max then
			max = v.num
		end

        if v:native() then
            bio:write(string.format("\t\"%s\",\t\t\t/* %d = %s */\n",
	            v.alias, v.num, v.alias))
		elseif c >= 3 then
            -- Lookup the info for this specific compat option.
            local flag, descr = ""
            for k, v in pairs(config.compat_options) do
                if v.compatlevel == c then
                    flag = v.flag
                    flag = flag:lower()
                    descr = v.descr
                end
            end

			bio:write(string.format("\t\"%s.%s\",\t\t/* %d = %s %s */\n",
	            flag, v.alias, v.num, descr, v.alias))
		elseif v.type.RESERVED then
			bio:write(string.format(
                "\t\"#%d\",\t\t\t/* %d = reserved for local use */\n",
	            v.num, v.num))
		elseif v.type.UNIMPL then
            local comment = ""
            comment = v.name -- xxx this is sometimes different
			bio:write(string.format("\t\"#%d\",\t\t\t/* %d = %s */\n",
		    v.num, v.num, comment))
        elseif v.type.OBSOL then
            bio:write(string.format(
                "\t\"obs_%s\",\t\t\t/* %d = obsolete %s */\n",
	            v.name, v.num, v.name))
		else -- do nothing
		end
	end

    -- End
    bio:write("};\n")
end

-- Check if this script is run directly.
if not pcall(debug.getlocal, 4, 1) then
    -- Entry of script:
    -- Use syscalls root as the package path.
    local path = arg[0]:gsub("/[^/]+.lua$", "")
    package.path = package.path .. ";" .. path .. "/../?.lua"  

    if #arg < 1 or #arg > 2 then
    	error("usage: " .. arg[0] .. " syscall.master")
    end
    
    local sysfile, configfile = arg[1], arg[2]
    
    config.merge(configfile)
    config.mergeCompat()
    config.mergeCapability()
    
    -- The parsed syscall table
    local tbl = FreeBSDSyscall:new{sysfile = sysfile, config = config}
    
    syscalls.file = config.sysnames -- change file here
    syscalls.generate(tbl, config, syscalls.file)
end

-- Return the module
return syscalls
