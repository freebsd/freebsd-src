#!/usr/libexec/flua
--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2024 Tyler Baxter <agge@FreeBSD.org>
-- Copyright (c) 2023 Warner Losh <imp@bsdimp.com>
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--

-- Setup to be a module, or ran as its own script.
local init_sysent = {}
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
init_sysent.file = "/dev/null"

function init_sysent.generate(tbl, config, fh)
	-- Grab the master system calls table.
	local s = tbl.syscalls

	-- Bind the generator to the parameter file.
	local gen = generator:new({}, fh)

	-- Write the generated preamble.
	gen:preamble("System call switch table.")

	gen:write(tbl.includes)

	-- Newline before and after this line.
	gen:write(
	    "\n#define AS(name) (sizeof(struct name) / sizeof(syscallarg_t))\n")

	-- Write out all the compat directives from compat_options.
	for _, v in pairs(config.compat_options) do
		gen:write(string.format([[

#ifdef %s
#define %s(n, name) .sy_narg = n, .sy_call = (sy_call_t *)__CONCAT(%s, name)
#else
#define %s(n, name) .sy_narg = 0, .sy_call = (sy_call_t *)nosys
#endif
]], v.definition, v.flag:lower(), v.prefix, v.flag:lower()))
	end
	-- Add a newline only if there were compat_options.
	if config.compat_options ~= nil then
		gen:write("\n")
	end

	gen:write(string.format([[
/* The casts are bogus but will do for now. */
struct sysent %s[] = {
]], config.switchname))

	for _, v in pairs(s) do
		local c = v:compatLevel()
		-- Comment is the function name by default, but may change
		-- based on the type of system call.
		local comment = v.name

		gen:write(v.prolog);

		-- Handle non-compat:
		if v:native() then
			gen:write(string.format(
			    "\t{ .sy_narg = %s, .sy_call = (sy_call_t *)",
			    v.args_size))
			-- Handle SYSMUX flag:
			if v.type.SYSMUX then
				gen:write(string.format("nosys, " ..
				    ".sy_auevent = AUE_NULL, " ..
				    ".sy_flags = %s, " ..
				    ".sy_thrcnt = SY_THR_STATIC },",
				    v.cap))
			-- Handle NOSTD flag:
			elseif v.type.NOSTD then
				gen:write(string.format("lkmressys, " ..
				    ".sy_auevent = AUE_NULL, " ..
				    ".sy_flags = %s, " ..
				    ".sy_thrcnt = SY_THR_ABSENT },",
				    v.cap))
			-- Handle rest of non-compat:
			else
				if v.name == "nosys" or
				    v.name == "lkmnosys" or
				    v.name == "sysarch" or
				    v.name:find("^freebsd") or
				    v.name:find("^linux") then
					gen:write(string.format("%s, " ..
					    ".sy_auevent = %s, " ..
					    ".sy_flags = %s, " ..
					    ".sy_thrcnt = %s },",
					    v:symbol(), v.audit, v.cap, v.thr))
				else
					gen:write(string.format("sys_%s, " ..
					    ".sy_auevent = %s, " ..
					    ".sy_flags = %s, " ..
					    ".sy_thrcnt = %s },",
					    v:symbol(), v.audit, v.cap, v.thr))
				end
			end

		-- Handle compat (everything >= FREEBSD3):
		elseif c >= 3 then
			-- Lookup the info for this specific compat option.
			local flag, descr
			for _, opt in pairs(config.compat_options) do
				if opt.compatlevel == c then
					flag = opt.flag
					flag = flag:lower()
					descr = opt.descr
					break
				end
			end

			if v.type.NOSTD then
				gen:write(string.format("\t{ " ..
				    ".sy_narg = %s, " ..
				    ".sy_call = (sy_call_t *)%s, " ..
				    ".sy_auevent = %s, " ..
				    ".sy_flags = 0, " ..
				    ".sy_thrcnt = SY_THR_ABSENT },",
				    "0", "lkmressys", "AUE_NULL"))
			else
				gen:write(string.format("\t{ %s(%s,%s), " ..
				    ".sy_auevent = %s, " ..
				    ".sy_flags = %s, " ..
				    ".sy_thrcnt = %s },",
				    flag, v.args_size, v.name, v.audit, v.cap, v.thr))
			end
			comment = descr .. " " .. v.name

		-- Handle obsolete:
		elseif v.type.OBSOL then
			gen:write("\t{ " ..
			    ".sy_narg = 0, .sy_call = (sy_call_t *)nosys, " ..
			    ".sy_auevent = AUE_NULL, .sy_flags = 0, " ..
			    ".sy_thrcnt = SY_THR_ABSENT },")
			comment = "obsolete " .. v.name

		-- Handle unimplemented:
		elseif v.type.UNIMPL then
			gen:write("\t{ " ..
			    ".sy_narg = 0, .sy_call = (sy_call_t *)nosys, " ..
			    ".sy_auevent = AUE_NULL, .sy_flags = 0, " ..
			    ".sy_thrcnt = SY_THR_ABSENT },")
			-- UNIMPL comment is not different in sysent.

		-- Handle reserved:
		elseif v.type.RESERVED then
			gen:write("\t{ " ..
			    ".sy_narg = 0, .sy_call = (sy_call_t *)nosys, " ..
			    ".sy_auevent = AUE_NULL, .sy_flags = 0, " ..
			    ".sy_thrcnt = SY_THR_ABSENT },")
			comment = "reserved for local use"
		end

		gen:write(string.format("\t/* %d = %s */\n", v.num, comment))
	end
	gen:write(tbl.epilog)

	-- End
	gen:write("};\n")
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

	-- The parsed syscall table.
	local tbl = FreeBSDSyscall:new{sysfile = sysfile, config = config}

	init_sysent.file = config.syssw 	-- change file here
	init_sysent.generate(tbl, config, init_sysent.file)
end

-- Return the module.
return init_sysent
