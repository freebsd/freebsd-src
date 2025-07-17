#!/usr/libexec/flua
--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2024 Tyler Baxter <agge@FreeBSD.org>
-- Copyright (c) 2023 Warner Losh <imp@bsdimp.com>
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--

-- Setup to be a module, or ran as its own script.
local syscalls = {}
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
syscalls.file = "/dev/null"

function syscalls.generate(tbl, config, fh)
	-- Grab the master system calls table.
	local s = tbl.syscalls

	-- Bind the generator to the parameter file.
	local gen = generator:new({}, fh)

	-- Write the generated preamble.
	gen:preamble("System call names.")

	gen:write(string.format("const char *%s[] = {\n", config.namesname))

	for _, v in pairs(s) do
		--print("num " .. v.num .. " name " .. v.name)
		local c = v:compatLevel()

		gen:write(v.prolog);

		if v:native() then
			gen:write(string.format([[
	"%s",			/* %d = %s */
]],
			    v.name, v.num, v.name))
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

			gen:write(string.format([[
	"%s.%s",		/* %d = %s %s */
]],
			    flag, v.name, v.num, descr, v.name))

		elseif v.type.RESERVED then
			gen:write(string.format([[
	"#%d",			/* %d = reserved for local use */
]],
			    v.num, v.num))

		elseif v.type.UNIMPL then
			gen:write(string.format([[
	"#%d",			/* %d = %s */
]],
			    v.num, v.num, v.alias))

		elseif v.type.OBSOL then
			gen:write(string.format([[
	"obs_%s",			/* %d = obsolete %s */
]],
			    v.name, v.num, v.name))

		end
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

	syscalls.file = config.sysnames	-- change file here
	syscalls.generate(tbl, config, syscalls.file)
end

-- Return the module.
return syscalls
