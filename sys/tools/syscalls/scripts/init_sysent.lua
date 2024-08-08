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
local init_sysent = {}

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
-- null
init_sysent.file = "/dev/null"
-- To align comments.
local column = 80

-- xxx need compat call count

-- Should be the same as makesyscalls.lua generates, except that we don't bother
-- to align the system call stuff... it's badly broken anyway and looks like crap
-- so we're declaring that a bug and removing all that crazy book-keeping to.
-- If we need to do it, and I hope we don't, I'll just create a string and do
-- #str to figure out how many tabs to add
function init_sysent.generate(tbl, config, fh)
    -- Grab the master syscalls table, and prepare bookkeeping for the max
    -- syscall number.
    local s = tbl.syscalls
    local max = 0

    -- Init the bsdio object, has macros and procedures for LSG specific io.
    local bio = bsdio:new({}, fh) 

    -- Write the generated tag.
    bio:generated("System call switch table.")

	bio:write(tbl.includes)

    -- Newline after includes and after this line.
	bio:write("\n#define AS(name) (sizeof(struct name) / sizeof(syscallarg_t))\n")

    -- Write out all the compat directives from compat_options
    -- NOTE: Linux won't have any, so it's skipped as expected.
    for _, v in pairs(config.compat_options) do
        bio:write(string.format([[

#ifdef %s
#define %s(n, name) .sy_narg = n, .sy_call = (sy_call_t *)__CONCAT(%s, name)
#else
#define %s(n, name) .sy_narg = 0, .sy_call = (sy_call_t *)nosys
#endif
]], v.definition, v.flag:lower(), v.prefix, v.flag:lower()))
	end

    -- Add a newline only if there were compat_options
    if config.compat_options ~= nil then
        bio:write("\n")
    end

    bio:write(string.format([[
/* The casts are bogus but will do for now. */
struct sysent %s[] = {
]], config.switchname))

    -- Handle linux nosys
    if config.syscallprefix:find("LINUX") ~= nil then
        bio:write("#define nosys   linux_nosys\n")
    end
  
    -- Looping for each system call.
    for k, v in pairs(s) do
        local c = v:compat_level()
        if v.num > max then
            max = v.num
        end
        local argssize = util.processArgsize(v)

        -- Comment is the function alias by default, but may change based on the 
        -- type of system call.
        local comment = v.alias

        -- Creating a string first, to length the string and align comments
        -- based on its length.
        local str = ""

        -- Handle native (non-compat):
        if v:native() then
            str = string.format(
                "\t{ .sy_narg = %s, .sy_call = (sy_call_t *)", 
                argssize)

            -- Handle SYSMUX flag:
            if v.type.SYSMUX then
                str = str .. string.format(
	        	    "nosys, .sy_auevent = AUE_NULL, " ..
	        	    ".sy_flags = %s, .sy_thrcnt = SY_THR_STATIC },",
	        	    v.cap)

            -- Handle NOSTD flag:
            elseif v.type.NOSTD then
	        	str = str .. string.format(
	        	    "lkmressys, .sy_auevent = AUE_NULL, " ..
	        	    ".sy_flags = %s, .sy_thrcnt = SY_THR_ABSENT },",
	        	    v.cap)

            -- Handle rest of non-compat:
            elseif v.type.STD or
                   v.type.NODEF or
                   v.type.NOARGS or
                   v.type.NOPROTO then
	            if v.name == "nosys" or 
                   v.name == "lkmnosys" or
                   v.name == "sysarch" or
                   v.name:find("^freebsd") or
	        	   v.name:find("^linux") then
                    str = str .. string.format(
                        "%s, .sy_auevent = %s, .sy_flags = %s, " .. 
                        ".sy_thrcnt = %s },",
	        		    v:symbol(), v.audit, v.cap, v.thr)
	        	else
                    str = str .. string.format(
                        "sys_%s, .sy_auevent = %s, .sy_flags = %s, " .. 
                        ".sy_thrcnt = %s },",
	        		    v:symbol(), v.audit, v.cap, v.thr)
	        	end

                else
                    -- Assume something went wrong.
                    --util.abort(1, 
                    --    "Unable to generate system switch for system call: " .. 
                    --    v.name)
                    -- xxx breaks linux, what case is linux skipping?
                end

        -- Handle compatibility (everything >= FREEBSD3):
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

            if v.type.NOSTD then
                str = string.format(
	    	        "\t{ .sy_narg = %s, .sy_call = (sy_call_t *)%s, " ..
	    	        ".sy_auevent = %s, .sy_flags = 0, " ..
	    	        ".sy_thrcnt = SY_THR_ABSENT },",
	    	        "0", "lkmressys", "AUE_NULL")
	        else
	    	    str = string.format(
	    	        "\t{ %s(%s,%s), .sy_auevent = %s, .sy_flags = %s, " ..
                    ".sy_thrcnt = %s },",
	    	        flag, argssize, v.name, v.audit, v.cap, v.thr)
            end
            comment = descr .. " " .. v.alias

        -- Handle obsolete:
        elseif v.type.OBSOL then
	        str = string.format(
                "\t{ .sy_narg = 0, .sy_call = (sy_call_t *)nosys, " ..
	            ".sy_auevent = AUE_NULL, .sy_flags = 0, " ..
                ".sy_thrcnt = SY_THR_ABSENT },")
            comment = "obsolete " .. v.alias
        
        -- Handle unimplemented:
        elseif v.type.UNIMPL then
		    str = string.format(
		        "\t{ .sy_narg = 0, .sy_call = (sy_call_t *)nosys, " ..
		        ".sy_auevent = AUE_NULL, .sy_flags = 0, " ..
		        ".sy_thrcnt = SY_THR_ABSENT },")
            -- UNIMPL comment is not different in sysent

        -- Handle reserved:
        elseif v.type.RESERVED then
            str = string.format(
		        "\t{ .sy_narg = 0, .sy_call = (sy_call_t *)nosys, " ..
		        ".sy_auevent = AUE_NULL, .sy_flags = 0, " ..
		        ".sy_thrcnt = SY_THR_ABSENT },")
            comment = "reserved for local use"
            
        else
            -- do nothing
        end

        bio:write(str)

        -- NOTE: Aligning comments doesn't really do much right now, other than 
        -- align to 80 columns. That can be changed by keeping track of the 
        -- columns for the desired line(s).
        local tabs = (column - #str) / 4
        for _ = 1, tabs do
            bio:write("\t")
        end

        -- NOTE: Comments are just tabbed from the line otherwise.
	    bio:write(string.format("\t/* %d = %s */\n", 
            v.num, comment))
    end

    -- End
    bio:write("};\n")
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

    init_sysent.file = config.syssw -- change file here
    init_sysent.generate(tbl, config, init_sysent.file)
end

-- Return the module
return init_sysent
