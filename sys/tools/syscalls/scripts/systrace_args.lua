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
local systrace_args = {}
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
systrace_args.file = "/dev/null"

function systrace_args.generate(tbl, config, fh)
    -- Grab the master syscalls table, and prepare bookkeeping for the max
    -- syscall number.
    local s = tbl.syscalls
    local max = 0

    -- Init the bsdio object, has macros and procedures for LSG specific io.
    local bio = bsdio:new({}, fh) 
    bio.storage_levels = {} -- make sure storage is clear

    -- generated() will be able to handle the newline here.
    bio:generated("System call argument to DTrace register array converstion.\n"
                  .. "This file is part of the DTrace syscall provider.")

    bio:write(string.format([[
static void
systrace_args(int sysnum, void *params, uint64_t *uarg, int *n_args)
{
	int64_t *iarg = (int64_t *)uarg;
	int a = 0;
	switch (sysnum) {
]]))
    
    bio:store(string.format([[
static void
systrace_entry_setargdesc(int sysnum, int ndx, char *desc, size_t descsz)
{
	const char *p = NULL;
	switch (sysnum) {
]]), 1)

    bio:store(string.format([[
static void
systrace_return_setargdesc(int sysnum, int ndx, char *desc, size_t descsz)
{
	const char *p = NULL;
	switch (sysnum) {
]]), 2)

    -- pad64() is an io macro that will pad based on the result of 
    -- abi_changes().
    bio:pad64(config.abiChanges("pair_64bit")) 

    for k, v in pairs(s) do
        local c = v:compat_level()
        if v.num > max then
            max = v.num
        end

        argssize = util.processArgsize(v)

        -- Handle non-compatability.
        if v:native() then
	        bio:write(string.format([[
	/* %s */
	case %d: {
]], v.name, v.num))
	        bio:store(string.format([[
	/* %s */
	case %d:
]], v.name, v.num), 1)
	        bio:store(string.format([[
	/* %s */
	case %d:
]], v.name, v.num), 2)

            local n_args = #v.args
            if v.type.SYSMUX then
                n_args = 0
            end

            if #v.args > 0 and not v.type.SYSMUX then
                padding = ""
                
		        bio:write(string.format(
		            "\t\tstruct %s *p = params;\n", v.arg_alias))
		        bio:store("\t\tswitch (ndx) {\n", 1)

                for idx, arg in ipairs(v.args) do
		    	    argtype = arg.type
		    	    argname = arg.name

		    	    argtype = util.trim(argtype:gsub("__restrict$", ""), nil)
		    	    if argtype == "int" and argname == "_pad" and 
                       config.abiChanges("pair_64bit") then
		    	    	bio:store("#ifdef PAD64_REQUIRED\n", 1)
		    	    end

		    	    -- Pointer arg?
                    local desc = ""
		    	    if argtype:find("*") then
		    	    	desc = "userland " .. argtype
		    	    else
		    	    	desc = argtype;
		    	    end
		    	    bio:store(string.format(
		    	        "\t\tcase %d%s:\n\t\t\tp = \"%s\";\n\t\t\tbreak;\n",
		    	        idx - 1, padding, desc), 1)

		    	    if argtype == "int" and argname == "_pad" and 
                       config.abiChanges("pair_64bit") then
		    	    	padding = " - _P_"
		    	    	bio:store("#define _P_ 0\n#else\n#define _P_ 1\n#endif\n", 1)
		    	    end

		    	    if util.isPtrType(argtype) then
		    	    	bio:write(string.format(
		    	    	    "\t\tuarg[a++] = (%s)p->%s; /* %s */\n",
		    	    	    config.ptr_intptr_t_cast,
		    	    	    argname, argtype))

		    	    elseif argtype == "union l_semun" then
		    	    	bio:write(string.format(
		    	    	    "\t\tuarg[a++] = p->%s.buf; /* %s */\n",
		    	    	    argname, argtype))

		    	    elseif argtype:sub(1,1) == "u" or argtype == "size_t" then
		    	    	bio:write(string.format(
		    	    	    "\t\tuarg[a++] = p->%s; /* %s */\n",
		    	    	    argname, argtype))

		    	    else
		    	    	if argtype == "int" and argname == "_pad" and
                           config.abiChanges("pair_64bit") then
		    	    		bio:write("#ifdef PAD64_REQUIRED\n")
		    	    	end
		    	    	bio:write(string.format(
		    	    	    "\t\tiarg[a++] = p->%s; /* %s */\n",
		    	    	    argname, argtype))
		    	    	if argtype == "int" and argname == "_pad" and 
                           config.abiChanges("pair_64bit") then
		    	    		bio:write("#endif\n")
		    	    	end
                    end
                end

		        bio:store("\t\tdefault:\n\t\t\tbreak;\n\t\t};\n", 1)

		        if padding ~= "" then
		        	bio:store("#undef _P_\n\n", 1)
		        end

                -- xxx error here, mux flag isn't being filtered properly
		        bio:store(string.format([[
		if (ndx == 0 || ndx == 1)
			p = "%s";
		break;
]], v.rettype), 2)
            end

	    bio:write(string.format(
	        "\t\t*n_args = %d;\n\t\tbreak;\n\t}\n", n_args))
	    bio:store("\t\tbreak;\n", 1)

        -- Handle compatibility (everything >= FREEBSD3):
        else
            -- do nothing, only for native
        end
    end

    bio:write(string.format([[
	default:
		*n_args = 0;
		break;
	};
}
]]))

    bio:store(string.format([[
	default:
		break;
	};
	if (p != NULL)
		strlcpy(desc, p, descsz);
}
]]), 1)

    bio:store(string.format([[
	default:
		break;
	};
	if (p != NULL)
		strlcpy(desc, p, descsz);
}
]]), 2)

    -- Write all stored lines.
    if bio.storage_levels ~= nil then
        bio:writeStorage()
    end

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
   
    systrace_args.file = config.systrace -- change file here
    systrace_args.generate(tbl, config, systrace_args.file)
end

-- Return the module
return systrace_args
