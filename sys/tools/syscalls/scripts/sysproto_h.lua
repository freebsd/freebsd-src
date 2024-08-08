#!/usr/libexec/flua
--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2024 Tyler Baxter <agge@FreeBSD.org>
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--

-- We generally assume that this script will be run by flua, however we've
-- carefully crafted modules for it that mimic interfaces provided by modules
-- available in ports.  Currently, this script is compatible with lua from
-- ports along with the compatible luafilesystem and lua-posix modules.

-- Setup to be a module, or ran as its own script.
local sysproto_h = {}
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
sysproto_h.file = "/dev/null"

function sysproto_h.generate(tbl, config, fh)
    -- Grab the master syscalls table, and prepare bookkeeping for the max
    -- syscall number.
    local s = tbl.syscalls
    local max = 0

    -- Init the bsdio object, has macros and procedures for LSG specific io.
    local bio = bsdio:new({}, fh) 
    bio.storage_levels = {} -- make sure storage is clear

    -- Write the generated tag.
    bio:generated("System call prototypes.")

    -- Write out the preamble.
    bio:write(string.format([[
#ifndef %s
#define	%s

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/cpuset.h>
#include <sys/domainset.h>
#include <sys/_ffcounter.h>
#include <sys/_semaphore.h>
#include <sys/ucontext.h>
#include <sys/wait.h>

#include <bsm/audit_kevents.h>

struct proc;

struct thread;

#define	PAD_(t)	(sizeof(syscallarg_t) <= sizeof(t) ? \
		0 : sizeof(syscallarg_t) - sizeof(t))

#if BYTE_ORDER == LITTLE_ENDIAN
#define	PADL_(t)	0
#define	PADR_(t)	PAD_(t)
#else
#define	PADL_(t)	PAD_(t)
#define	PADR_(t)	0
#endif

]], config.sysproto_h, config.sysproto_h))
    
    -- pad64() is an io macro that will pad based on the result of 
    -- abi_changes().
    bio:pad64(config.abiChanges("pair_64bit"))

    -- Make a local copy of global compat options, write lines are going to be 
    -- stored in it. There's a lot of specific compat handling for sysproto.h
    local compat_options = config.compat_options

    -- Write out all the compat directives from compat_options
    -- NOTE: Linux won't have any, so it's skipped as expected.
    for _, v in pairs(config.compat_options) do
        -- 
        -- NOTE: Storing each compat entry requires storing multiple levels of 
        -- file generation; compat entries are given ranges of 10 instead to 
        -- cope with this.
        -- EXAMPLE: 13 is indexed as 130, 131 is the second generation level of 
        -- 13
        --
        -- Tag an extra newline to the end, so it doesn't have to be worried 
        -- about later.
        bio:store(string.format("\n#ifdef %s\n\n", v.definition), v.compatlevel * 10)
	end

    for k, v in pairs(s) do
        local c = v:compat_level()
        if v.num > max then
            max = v.num
        end

        -- Audit defines are stored at an arbitrarily large number so that 
        -- they're always at the last storage level; to allow compat entries to 
        -- be indexed more intuitively (by their compat level).
        local audit_idx = 0xffffffff -- this should do

        -- Handle non-compatability.
        if v:native() then
            -- All these negation conditions are because (in general) these are
            -- cases where sysproto.h is not generated.
            if not v.type.NOARGS and
               not v.type.NOPROTO and
               not v.type.NODEF then
                if #v.args > 0 then
                    --print("entering args cond")
                    --dump(v.args)
                    bio:write(string.format("struct %s {\n",
		    	        v.arg_alias))

		    	    for _, v in ipairs(v.args) do
		    		    if v.type == "int" and v.name == "_pad" and 
                           config.abiChanges("pair_64bit") then 
                            bio:write("#ifdef PAD64_REQUIRED\n")
                        end

		    		    bio:write(string.format(
                            "\tchar %s_l_[PADL_(%s)]; %s %s; char %s_r_[PADR_(%s)];\n",
                            v.name, v.type,
		    		        v.type, v.name,
		    		        v.name, v.type))

                        if v.type == "int" and v.name == "_pad" and
                           config.abiChanges("pair_64bit") then
                            bio:write("#endif\n")
                        end
		    		end

                    bio:write("};\n")

                else
                    bio:write(string.format(
		    	        "struct %s {\n\tsyscallarg_t dummy;\n};\n", 
                        v.arg_alias))
                end
            end

            if not v.type.NOPROTO and
               not v.type.NODEF then
                local sys_prefix = "sys_"

                if v.name == "nosys" or v.name == "lkmnosys" or
                   v.name == "sysarch" or v.name:find("^freebsd") or
                   v.name:find("^linux") then
                    sys_prefix = ""
                end
                
                -- xxx rettype is not correct
                bio:store(string.format(
                    "%s\t%s%s(struct thread *, struct %s *);\n",
		            v.rettype, sys_prefix, v.name, v.arg_alias), 1)

                -- Audit defines are stored at an arbitrarily large number so 
                -- that they're always at the end; to allow compat entries to 
                -- just be indexed by their compat level.
		        bio:store(string.format("#define\t%sAUE_%s\t%s\n",
		            config.syscallprefix, v.alias, v.audit), audit_idx) 
            end

            -- Handle reached end of native.
            if max >= v.num then
                -- nothing for now
            else
                -- all cases covered, do nothing
            end
        -- noncompat done
                
        --
        -- Handle compatibility (everything >= FREEBSD3)
        -- Because of the way sysproto.h is printed, lines are stored by their 
        -- compat level, then written in the expected order later.
        --
        -- NOTE: Storing each compat entry requires storing multiple levels of 
        -- file generation; compat entries are given ranges of 10 instead to 
        -- cope with this.
        -- EXAMPLE: 13 is indexed as 130, 131 is the second generation level of 
        -- 13
        -- 
        elseif c >= 3 then
            local idx = c * 10
            if not v.type.NOPROTO and
               not v.type.NODEF and
               not v.type.NOARGS then
                if #v.args > 0 then
                    bio:store(string.format("struct %s {\n", v.arg_alias), idx)
                    for _, arg in ipairs(v.args) do
		                bio:store(string.format(
		                    "\tchar %s_l_[PADL_(%s)]; %s %s; " ..
                            "char %s_r_[PADR_(%s)];\n",
		                    arg.name, arg.type,
		                    arg.type, arg.name,
		                    arg.name, arg.type), idx)
		             end
		             bio:store("};\n", idx)
                else 
                     -- NOTE: Not stored, writen sequentially in the first run.
                     bio:write(string.format(
		                 "struct %s {\n\tsyscallarg_t dummy;\n};\n", v.arg_alias))
                end
            end

            if not v.type.NOPROTO and
               not v.type.NODEF then 
		        bio:store(string.format(
		            "%s\t%s%s(struct thread *, struct %s *);\n",
		            v.rettype, v.prefix, v:symbol(), v.arg_alias), idx + 1)
		        bio:store(string.format(
		            "#define\t%sAUE_%s%s\t%s\n", config.syscallprefix,
		            v.prefix, v:symbol(), v.audit), audit_idx)
            end
        
        -- Handle obsolete, unimplemented, and reserved -- do nothing
        else
            -- do nothing
        end
    end

    -- Append #endif to each compat option.
    for _, v in pairs(config.compat_options) do
        -- If compat entries are indexed by 10s, then 9 will always be the end 
        -- of that compat entry.
        local end_idx = (v.compatlevel * 10) + 9
        -- Need an extra newline after #endif
        bio:store(string.format("\n#endif /* %s */\n\n", v.definition), end_idx)
	end

    if bio.storage_levels ~= nil then
        bio:writeStorage()
    end

    -- After everything has been unrolled, tag the ending bits.
    bio:write(string.format([[

#undef PAD_
#undef PADL_
#undef PADR_

#endif /* !%s */
]], config.sysproto_h))

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

    sysproto_h.file = config.sysproto -- change file here
    sysproto_h.generate(tbl, config, sysproto_h.file)
end

-- Return the module
return sysproto_h
