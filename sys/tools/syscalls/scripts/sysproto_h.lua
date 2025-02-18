#!/usr/libexec/flua
--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2024 Tyler Baxter <agge@FreeBSD.org>
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--

-- Setup to be a module, or ran as its own script.
local sysproto_h = {}
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
sysproto_h.file = "/dev/null"

function sysproto_h.generate(tbl, config, fh)
	-- Grab the master system calls table.
	local s = tbl.syscalls

	-- Bind the generator to the parameter file.
	local gen = generator:new({}, fh)
	gen.storage_levels = {}	-- make sure storage is clear

	-- Write the generated preamble.
	gen:preamble("System call prototypes.")

	-- Write out all the preprocessor directives.
	gen:write(string.format([[
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

	-- 64-bit padding preprocessor directive.
	gen:pad64(config.abiChanges("pair_64bit"))

	--
	-- Storing each compat entry requires storing multiple levels of file
	-- generation; compat entries are given ranges of 10 instead to cope
	-- with this.  For example, 13 is indexed as 130; 131 is the second
	-- storage level of 13.
	--

	-- Store all the compat #ifdef from compat_options at their zero index.
	for _, v in pairs(config.compat_options) do
		-- Tag an extra newline to the end, so it doesn't have to be
		-- worried about later.
		gen:store(string.format("\n#ifdef %s\n\n", v.definition),
				  v.compatlevel * 10)
	end

	for _, v in pairs(s) do
		local c = v:compatLevel()

		-- Audit defines are stored at an arbitrarily large number so
		-- that they're always at the last storage level, and compat
		-- entries can be indexed by their compat level (more
		-- intuitive).
		local audit_idx = 10000 -- this should do

		gen:write(v.prolog)
		gen:store(v.prolog, 1)
		for _, w in pairs(config.compat_options) do
			gen:store(v.prolog, w.compatlevel * 10)
		end

		-- Handle non-compat:
		if v:native() then
			-- All these negation conditions are because (in
			-- general) these are cases where code for sysproto.h
			-- is not generated.
			if not v.type.NOARGS and not v.type.NOPROTO and
			    not v.type.NODEF then
				if #v.args > 0 then
					gen:write(string.format(
					    "struct %s {\n", v.arg_alias))
					for _, arg in ipairs(v.args) do
						if arg.type == "int" and
						   arg.name == "_pad" and
						   config.abiChanges(
						       "pair_64bit") then
							gen:write("#ifdef PAD64_REQUIRED\n")
						end

						gen:write(string.format([[
	char %s_l_[PADL_(%s)]; %s %s; char %s_r_[PADR_(%s)];
]],
						    arg.name, arg.type,
						    arg.type, arg.name,
						    arg.name, arg.type))

						if arg.type == "int" and
						    arg.name == "_pad" and
						    config.abiChanges(
							"pair_64bit") then
							gen:write("#endif\n")
						end
					end
					gen:write("};\n")
				else
					gen:write(string.format(
					    "struct %s {\n\tsyscallarg_t dummy;\n};\n",
					    v.arg_alias))
				end
			end
			if not v.type.NOPROTO and not v.type.NODEF then
				local sys_prefix = "sys_"
				if v.name == "nosys" or v.name == "lkmnosys" or
				    v.name == "sysarch" or
				    v.name:find("^freebsd") or
				    v.name:find("^linux") then
					sys_prefix = ""
				end
				gen:store(string.format(
				    "%s\t%s%s(struct thread *, struct %s *);\n",
				    v.rettype, sys_prefix, v.name, v.arg_alias),
				    1)
				gen:store(string.format(
				    "#define\t%sAUE_%s\t%s\n",
				    config.syscallprefix, v:symbol(), v.audit),
				    audit_idx)
			end

		-- Handle compat (everything >= FREEBSD3):
		elseif c >= 3 then
			local idx = c * 10
			if not v.type.NOARGS and not v.type.NOPROTO and
				not v.type.NODEF then
				if #v.args > 0 then
					gen:store(string.format(
					    "struct %s {\n", v.arg_alias), idx)
					for _, arg in ipairs(v.args) do
						gen:store(string.format([[
	char %s_l_[PADL_(%s)]; %s %s; char %s_r_[PADR_(%s)];
]],
						    arg.name, arg.type,
						    arg.type, arg.name,
						    arg.name, arg.type), idx)
					end
					gen:store("};\n", idx)
				else
					-- Not stored, written on the first run.
					gen:write(string.format([[
struct %s {
	syscallarg_t dummy;
};
]],
					    v.arg_alias))
				end
			end
			if not v.type.NOPROTO and not v.type.NODEF then
				gen:store(string.format([[
%s	%s%s(struct thread *, struct %s *);
]],
				    v.rettype, v:compatPrefix(), v.name,
				    v.arg_alias), idx + 1)
				gen:store(string.format([[
#define	%sAUE_%s%s	%s
]],
				    config.syscallprefix, v:compatPrefix(),
				    v.name, v.audit), audit_idx)
			end
		end
		-- Do nothing for obsolete, unimplemented, and reserved.
	end

	-- Append #endif to the end of each compat option.
	for _, v in pairs(config.compat_options) do
		-- Based on how they're indexed, 9 is the last index.
		local end_idx = (v.compatlevel * 10) + 9
		-- Need an extra newline after #endif.
		gen:store(string.format("\n#endif /* %s */\n\n", v.definition),
		    end_idx)
	end

	gen:write(tbl.epilog)
	gen:store(tbl.epilog, 1)
	for _, w in pairs(config.compat_options) do
		gen:store(tbl.epilog, w.compatlevel * 10)
	end

	if gen.storage_levels ~= nil then
		gen:writeStorage()
	end

	-- After storage has been unrolled, tag on the ending bits.
	gen:write(string.format([[

#undef PAD_
#undef PADL_
#undef PADR_

#endif /* !%s */
]], config.sysproto_h))
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

	sysproto_h.file = config.sysproto	-- change file here
	sysproto_h.generate(tbl, config, sysproto_h.file)
end

-- Return the module.
return sysproto_h
