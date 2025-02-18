#!/usr/libexec/flua
--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2024 Tyler Baxter <agge@FreeBSD.org>
-- Copyright (c) 2023 Warner Losh <imp@bsdimp.com>
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--

-- Setup to be a module, or ran as its own script.
local systrace_args = {}
local script = not pcall(debug.getlocal, 4, 1)	-- TRUE if script.
if script then
	-- Add library root to the package path.
	local path = arg[0]:gsub("/[^/]+.lua$", "")
	package.path = package.path .. ";" .. path .. "/../?.lua"
end

local FreeBSDSyscall = require("core.freebsd-syscall")
local util = require("tools.util")
local generator = require("tools.generator")

-- File has not been decided yet; config will decide file.  Default defined as
-- /dev/null.
systrace_args.file = "/dev/null"

function systrace_args.generate(tbl, config, fh)
	-- Grab the master system calls table.
	local s = tbl.syscalls

	-- Bind the generator to the parameter file.
	local gen = generator:new({}, fh)
	gen.storage_levels = {}	-- make sure storage is clear

	-- 64-bit padding preprocessor directive.
	gen:pad64(config.abiChanges("pair_64bit"))

	-- Write the generated preamble.
	gen:preamble(
	    "System call argument to DTrace register array conversion.\n" ..
	    "\n" ..
	    "This file is part of the DTrace syscall provider.")

	gen:write(string.format([[
static void
systrace_args(int sysnum, void *params, uint64_t *uarg, int *n_args)
{
	int64_t *iarg = (int64_t *)uarg;
	int a = 0;
	switch (sysnum) {
]]))

	gen:store(string.format([[
static void
systrace_entry_setargdesc(int sysnum, int ndx, char *desc, size_t descsz)
{
	const char *p = NULL;
	switch (sysnum) {
]]), 1)

	gen:store(string.format([[
static void
systrace_return_setargdesc(int sysnum, int ndx, char *desc, size_t descsz)
{
	const char *p = NULL;
	switch (sysnum) {
]]), 2)

	for _, v in pairs(s) do

		gen:write(v.prolog);
		gen:store(v.prolog, 1);
		gen:store(v.prolog, 2);

		-- Handle non compat:
		if v:native() then
			gen:write(string.format([[
	/* %s */
	case %d: {
]], v.name, v.num))

			gen:store(string.format([[
	/* %s */
	case %d:
]], v.name, v.num), 1)

			gen:store(string.format([[
	/* %s */
	case %d:
]], v.name, v.num), 2)

			local n_args = #v.args
			if v.type.SYSMUX then
				n_args = 0
			end

			if #v.args > 0 and not v.type.SYSMUX then
				local padding = ""

				gen:write(string.format([[
		struct %s *p = params;
]],
				    v.arg_alias))

				gen:store([[
		switch (ndx) {
]],
				    1)

				for idx, arg in ipairs(v.args) do
					local argtype = util.trim(
						arg.type:gsub(
						    "__restrict$", ""), nil)
					if argtype == "int" and
					    arg.name == "_pad" and
					    config.abiChanges("pair_64bit") then
						gen:store(
						    "#ifdef PAD64_REQUIRED\n",
						    1)
					end

					-- Pointer arg?
					local desc
					if argtype:find("*") then
						desc = "userland " .. argtype
					else
						desc = argtype;
					end

					gen:store(string.format([[
		case %d%s:
			p = "%s";
			break;
]],
					    idx - 1, padding, desc), 1)

					if argtype == "int" and
					    arg.name == "_pad" and
					   config.abiChanges("pair_64bit") then
						padding = " - _P_"
						gen:store([[
#define _P_ 0
#else
#define _P_ 1
#endif
]],
					    1)
					end

					if util.isPtrType(argtype,
					    config.abi_intptr_t) then
						gen:write(string.format([[
		uarg[a++] = (%s)p->%s; /* %s */
]],
						    config.ptr_intptr_t_cast,
						    arg.name, argtype))
					elseif argtype == "union l_semun" then
						gen:write(string.format([[
		uarg[a++] = p->%s.buf; /* %s */
]],
						    arg.name, argtype))
					elseif argtype:sub(1,1) == "u" or
					    argtype == "size_t" then
						gen:write(string.format([[
		uarg[a++] = p->%s; /* %s */
]],
						    arg.name, argtype))
					else
						if argtype == "int" and
						    arg.name == "_pad" and
						    config.abiChanges(
						    "pair_64bit") then
							gen:write([[
#ifdef PAD64_REQUIRED
]])
						end

						gen:write(string.format([[
		iarg[a++] = p->%s; /* %s */
]],
						    arg.name, argtype))

						if argtype == "int" and
						    arg.name == "_pad" and
						    config.abiChanges(
						    "pair_64bit") then
							gen:write("#endif\n")
						end
					end
				end

				gen:store([[
		default:
			break;
		};
]],
				    1)

				if padding ~= "" then
					gen:store("#undef _P_\n\n", 1)
				end

				gen:store(string.format([[
		if (ndx == 0 || ndx == 1)
			p = "%s";
		break;
]], v.ret), 2)
		end

		gen:write(string.format("\t\t*n_args = %d;\n\t\tbreak;\n\t}\n",
		    n_args))
		gen:store("\t\tbreak;\n", 1)

		-- Handle compat (everything >= FREEBSD3):
		-- Do nothing, only for native.
		end
	end

	gen:write(tbl.epilog)
	gen:write([[
	default:
		*n_args = 0;
		break;
	};
}
]])

	gen:store(tbl.epilog, 1)
	gen:store([[
	default:
		break;
	};
	if (p != NULL)
		strlcpy(desc, p, descsz);
}
]], 1)

	gen:store(tbl.epilog, 2)
	gen:store([[
	default:
		break;
	};
	if (p != NULL)
		strlcpy(desc, p, descsz);
}
]], 2)

	-- Write all stored lines.
	if gen.storage_levels ~= nil then
		gen:writeStorage()
	end

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

	systrace_args.file = config.systrace	-- change file here
	systrace_args.generate(tbl, config, systrace_args.file)
end

-- Return the module.
return systrace_args
