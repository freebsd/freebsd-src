--
-- SPDX-License-Identifier: BSD-2-Clause-FreeBSD
--
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions
-- are met:
-- 1. Redistributions of source code must retain the above copyright
--    notice, this list of conditions and the following disclaimer.
-- 2. Redistributions in binary form must reproduce the above copyright
--    notice, this list of conditions and the following disclaimer in the
--    documentation and/or other materials provided with the distribution.
--
-- THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
-- ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
-- ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
-- FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
-- DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
-- OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
-- HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
-- LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
-- OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
-- SUCH DAMAGE.
--
-- $FreeBSD$
--


-- We generally assume that this script will be run by flua, however we've
-- carefully crafted modules for it that mimic interfaces provided by modules
-- available in ports.  Currently, this script is compatible with lua from ports
-- along with the compatible luafilesystem and lua-posix modules.
local lfs = require("lfs")
local unistd = require("posix.unistd")

local savesyscall = -1
local maxsyscall = -1
local generated_tag = "@" .. "generated"

-- Default configuration; any of these may get replaced by a configuration file
-- optionally specified.
local config = {
	os_id_keyword = "FreeBSD",
	abi_func_prefix = "",
	sysnames = "syscalls.c",
	sysproto = "../sys/sysproto.h",
	sysproto_h = "_SYS_SYSPROTO_H_",
	syshdr = "../sys/syscall.h",
	sysmk = "../sys/syscall.mk",
	syssw = "init_sysent.c",
	syscallprefix = "SYS_",
	switchname = "sysent",
	namesname = "syscallnames",
	systrace = "systrace_args.c",
	capabilities_conf = "capabilities.conf",
	capenabled = {},
	mincompat = 0,
	abi_type_suffix = "",
	abi_flags = "",
	abi_flags_mask = 0,
	abi_headers = "",
	abi_intptr_t = "intptr_t",
	abi_size_t = "size_t",
	abi_u_long = "u_long",
	abi_long = "long",
	abi_semid_t = "semid_t",
	abi_ptr_array_t = "",
	ptr_intptr_t_cast = "intptr_t",
}

local config_modified = {}
local cleantmp = true
local tmpspace = "/tmp/sysent." .. unistd.getpid() .. "/"

local output_files = {
	"sysnames",
	"syshdr",
	"sysmk",
	"syssw",
	"systrace",
	"sysproto",
}

-- These ones we'll create temporary files for; generation purposes.
local temp_files = {
	"sysaue",
	"sysdcl",
	"syscompat",
	"syscompatdcl",
	"sysent",
	"sysinc",
	"sysarg",
	"sysprotoend",
	"systracetmp",
	"systraceret",
}

-- Opened files
local files = {}

local function cleanup()
	for _, v in pairs(files) do
		assert(v:close())
	end
	if cleantmp then
		if lfs.dir(tmpspace) then
			for fname in lfs.dir(tmpspace) do
				if fname ~= "." and fname ~= ".." then
					assert(os.remove(tmpspace .. "/" ..
					    fname))
				end
			end
		end

		if lfs.attributes(tmpspace) and not lfs.rmdir(tmpspace) then
			assert(io.stderr:write("Failed to clean up tmpdir: " ..
			    tmpspace .. "\n"))
		end
	else
		assert(io.stderr:write("Temp files left in " .. tmpspace ..
		    "\n"))
	end
end

local function abort(status, msg)
	assert(io.stderr:write(msg .. "\n"))
	cleanup()
	os.exit(status)
end

-- Each entry should have a value so we can represent abi flags as a bitmask
-- for convenience.  One may also optionally provide an expr; this gets applied
-- to each argument type to indicate whether this argument is subject to ABI
-- change given the configured flags.
local known_abi_flags = {
	long_size = {
		value	= 0x00000001,
		exprs	= {
			"_Contains[a-z_]*_long_",
			"^long [a-z0-9_]+$",
			"long [*]",
			"size_t [*]",
			-- semid_t is not included because it is only used
			-- as an argument or written out individually and
			-- said writes are handled by the ksem framework.
			-- Technically a sign-extension issue exists for
			-- arguments, but because semid_t is actually a file
			-- descriptor negative 32-bit values are invalid
			-- regardless of sign-extension.
		},
	},
	time_t_size = {
		value	= 0x00000002,
		exprs	= {
			"_Contains[a-z_]*_timet_",
		},
	},
	pointer_args = {
		value	= 0x00000004,
	},
	pointer_size = {
		value	= 0x00000008,
		exprs	= {
			"_Contains[a-z_]*_ptr_",
			"[*][*]",
		},
	},
}

local known_flags = {
	STD		= 0x00000001,
	OBSOL		= 0x00000002,
	RESERVED	= 0x00000004,
	UNIMPL		= 0x00000008,
	NODEF		= 0x00000010,
	NOARGS		= 0x00000020,
	NOPROTO		= 0x00000040,
	NOSTD		= 0x00000080,
	NOTSTATIC	= 0x00000100,
	CAPENABLED	= 0x00000200,

	-- Compat flags start from here.  We have plenty of space.
}

-- All compat_options entries should have five entries:
--	definition: The preprocessor macro that will be set for this
--	compatlevel: The level this compatibility should be included at.  This
--	    generally represents the version of FreeBSD that it is compatible
--	    with, but ultimately it's just the level of mincompat in which it's
--	    included.
--	flag: The name of the flag in syscalls.master.
--	prefix: The prefix to use for _args and syscall prototype.  This will be
--	    used as-is, without "_" or any other character appended.
--	descr: The description of this compat option in init_sysent.c comments.
-- The special "stdcompat" entry will cause the other five to be autogenerated.
local compat_options = {
	{
		definition = "COMPAT_43",
		compatlevel = 3,
		flag = "COMPAT",
		prefix = "o",
		descr = "old",
	},
	{ stdcompat = "FREEBSD4" },
	{ stdcompat = "FREEBSD6" },
	{ stdcompat = "FREEBSD7" },
	{ stdcompat = "FREEBSD10" },
	{ stdcompat = "FREEBSD11" },
	{ stdcompat = "FREEBSD12" },
}

local function trim(s, char)
	if s == nil then
		return nil
	end
	if char == nil then
		char = "%s"
	end
	return s:gsub("^" .. char .. "+", ""):gsub(char .. "+$", "")
end

-- config looks like a shell script; in fact, the previous makesyscalls.sh
-- script actually sourced it in.  It had a pretty common format, so we should
-- be fine to make various assumptions
local function process_config(file)
	local cfg = {}
	local comment_line_expr = "^%s*#.*"
	-- We capture any whitespace padding here so we can easily advance to
	-- the end of the line as needed to check for any trailing bogus bits.
	-- Alternatively, we could drop the whitespace and instead try to
	-- use a pattern to strip out the meaty part of the line, but then we
	-- would need to sanitize the line for potentially special characters.
	local line_expr = "^([%w%p]+%s*)=(%s*[`\"]?[^\"`]+[`\"]?)"

	if not file then
		return nil, "No file given"
	end

	local fh = assert(io.open(file))

	for nextline in fh:lines() do
		-- Strip any whole-line comments
		nextline = nextline:gsub(comment_line_expr, "")
		-- Parse it into key, value pairs
		local key, value = nextline:match(line_expr)
		if key ~= nil and value ~= nil then
			local kvp = key .. "=" .. value
			key = trim(key)
			value = trim(value)
			local delim = value:sub(1,1)
			if delim == '"' then
				local trailing_context

				-- Strip off the key/value part
				trailing_context = nextline:sub(kvp:len() + 1)
				-- Strip off any trailing comment
				trailing_context = trailing_context:gsub("#.*$",
				    "")
				-- Strip off leading/trailing whitespace
				trailing_context = trim(trailing_context)
				if trailing_context ~= "" then
					print(trailing_context)
					abort(1, "Malformed line: " .. nextline)
				end

				value = trim(value, delim)
			else
				-- Strip off potential comments
				value = value:gsub("#.*$", "")
				-- Strip off any padding whitespace
				value = trim(value)
				if value:match("%s") then
					abort(1, "Malformed config line: " ..
					    nextline)
				end
			end
			cfg[key] = value
		elseif not nextline:match("^%s*$") then
			-- Make sure format violations don't get overlooked
			-- here, but ignore blank lines.  Comments are already
			-- stripped above.
			abort(1, "Malformed config line: " .. nextline)
		end
	end

	assert(io.close(fh))
	return cfg
end

local function grab_capenabled(file, open_fail_ok)
	local capentries = {}
	local commentExpr = "#.*"

	if file == nil then
		print "No file"
		return {}
	end

	local fh = io.open(file)
	if fh == nil then
		if not open_fail_ok then
			abort(1, "Failed to open " .. file)
		end
		return {}
	end

	for nextline in fh:lines() do
		-- Strip any comments
		nextline = nextline:gsub(commentExpr, "")
		if nextline ~= "" then
			capentries[nextline] = true
		end
	end

	assert(io.close(fh))
	return capentries
end

local function process_compat()
	local nval = 0
	for _, v in pairs(known_flags) do
		if v > nval then
			nval = v
		end
	end

	nval = nval << 1
	for _, v in pairs(compat_options) do
		if v["stdcompat"] ~= nil then
			local stdcompat = v["stdcompat"]
			v["definition"] = "COMPAT_" .. stdcompat:upper()
			v["compatlevel"] = tonumber(stdcompat:match("([0-9]+)$"))
			v["flag"] = stdcompat:gsub("FREEBSD", "COMPAT")
			v["prefix"] = stdcompat:lower() .. "_"
			v["descr"] = stdcompat:lower()
		end

		local tmpname = "sys" .. v["flag"]:lower()
		local dcltmpname = tmpname .. "dcl"
		files[tmpname] = io.tmpfile()
		files[dcltmpname] = io.tmpfile()
		v["tmp"] = tmpname
		v["dcltmp"] = dcltmpname

		known_flags[v["flag"]] = nval
		v["mask"] = nval
		nval = nval << 1

		v["count"] = 0
	end
end

local function process_abi_flags()
	local flags, mask = config["abi_flags"], 0
	for txtflag in flags:gmatch("([^|]+)") do
		if known_abi_flags[txtflag] == nil then
			abort(1, "Unknown abi_flag: " .. txtflag)
		end

		mask = mask | known_abi_flags[txtflag]["value"]
	end

	config["abi_flags_mask"] = mask
end

local function abi_changes(name)
	if known_abi_flags[name] == nil then
		abort(1, "abi_changes: unknown flag: " .. name)
	end

	return config["abi_flags_mask"] & known_abi_flags[name]["value"] ~= 0
end

local function strip_abi_prefix(funcname)
	local abiprefix = config["abi_func_prefix"]
	local stripped_name
	if funcname == nil then
		return nil
	end
	if abiprefix ~= "" and funcname:find("^" .. abiprefix) then
		stripped_name = funcname:gsub("^" .. abiprefix, "")
	else
		stripped_name = funcname
	end

	return stripped_name
end

local function read_file(tmpfile)
	if files[tmpfile] == nil then
		print("Not found: " .. tmpfile)
		return
	end

	local fh = files[tmpfile]
	assert(fh:seek("set"))
	return assert(fh:read("a"))
end

local function write_line(tmpfile, line)
	if files[tmpfile] == nil then
		print("Not found: " .. tmpfile)
		return
	end
	assert(files[tmpfile]:write(line))
end

local function write_line_pfile(tmppat, line)
	for k in pairs(files) do
		if k:match(tmppat) ~= nil then
			assert(files[k]:write(line))
		end
	end
end

-- Check both literal intptr_t and the abi version because this needs
-- to work both before and after the substitution
local function isptrtype(type)
	return type:find("*") or type:find("caddr_t") or
	    type:find("intptr_t") or type:find(config['abi_intptr_t'])
end

local function isptrarraytype(type)
	return type:find("[*][*]") or type:find("[*][ ]*const[ ]*[*]")
end

local process_syscall_def

-- These patterns are processed in order on any line that isn't empty.
local pattern_table = {
	{
		pattern = "%s*$" .. config['os_id_keyword'],
		process = function(_, _)
			-- Ignore... ID tag
		end,
	},
	{
		dump_prevline = true,
		pattern = "^#%s*include",
		process = function(line)
			line = line .. "\n"
			write_line('sysinc', line)
		end,
	},
	{
		dump_prevline = true,
		pattern = "^#",
		process = function(line)
			if line:find("^#%s*if") then
				savesyscall = maxsyscall
			elseif line:find("^#%s*else") then
				maxsyscall = savesyscall
			end
			line = line .. "\n"
			write_line('sysent', line)
			write_line('sysdcl', line)
			write_line('sysarg', line)
			write_line_pfile('syscompat[0-9]*$', line)
			write_line('sysnames', line)
			write_line_pfile('systrace.*', line)
		end,
	},
	{
		dump_prevline = true,
		pattern = "%%ABI_HEADERS%%",
		process = function()
			if config['abi_headers'] ~= "" then
				line = config['abi_headers'] .. "\n"
				write_line('sysinc', line)
			end
		end,
	},
	{
		-- Buffer anything else
		pattern = ".+",
		process = function(line, prevline)
			local incomplete = line:find("\\$") ~= nil
			-- Lines that end in \ get the \ stripped
			-- Lines that start with a syscall number, prepend \n
			line = trim(line):gsub("\\$", "")
			if line:find("^[0-9]") and prevline then
				process_syscall_def(prevline)
				prevline = nil
			end

			prevline = (prevline or '') .. line
			incomplete = incomplete or prevline:find(",$") ~= nil
			incomplete = incomplete or prevline:find("{") ~= nil and
			    prevline:find("}") == nil
			if prevline:find("^[0-9]") and not incomplete then
				process_syscall_def(prevline)
				prevline = nil
			end

			return prevline
		end,
	},
}

local function process_sysfile(file)
	local capentries = {}
	local commentExpr = "^%s*;.*"

	if file == nil then
		print "No file"
		return {}
	end

	local fh = io.open(file)
	if fh == nil then
		print("Failed to open " .. file)
		return {}
	end

	local function do_match(nextline, prevline)
		local pattern, handler, dump
		for _, v in pairs(pattern_table) do
			pattern = v['pattern']
			handler = v['process']
			dump = v['dump_prevline']
			if nextline:match(pattern) then
				if dump and prevline then
					process_syscall_def(prevline)
					prevline = nil
				end

				return handler(nextline, prevline)
			end
		end

		abort(1, "Failed to handle: " .. nextline)
	end

	local prevline
	for nextline in fh:lines() do
		-- Strip any comments
		nextline = nextline:gsub(commentExpr, "")
		if nextline ~= "" then
			prevline = do_match(nextline, prevline)
		end
	end

	-- Dump any remainder
	if prevline ~= nil and prevline:find("^[0-9]") then
		process_syscall_def(prevline)
	end

	assert(io.close(fh))
	return capentries
end

local function get_mask(flags)
	local mask = 0
	for _, v in ipairs(flags) do
		if known_flags[v] == nil then
			abort(1, "Checking for unknown flag " .. v)
		end

		mask = mask | known_flags[v]
	end

	return mask
end

local function get_mask_pat(pflags)
	local mask = 0
	for k, v in pairs(known_flags) do
		if k:find(pflags) then
			mask = mask | v
		end
	end

	return mask
end

local function align_sysent_comment(col)
	write_line("sysent", "\t")
	col = col + 8 - col % 8
	while col < 56 do
		write_line("sysent", "\t")
		col = col + 8
	end
end

local function strip_arg_annotations(arg)
	arg = arg:gsub("_In[^ ]*[_)] ?", "")
	arg = arg:gsub("_Out[^ ]*[_)] ?", "")
	return trim(arg)
end

local function check_abi_changes(arg)
	for k, v in pairs(known_abi_flags) do
		local exprs = v["exprs"]
		if abi_changes(k) and exprs ~= nil then
			for _, e in pairs(exprs) do
				if arg:find(e) then
					return true
				end
			end
		end
	end

	return false
end

local function process_args(args)
	local funcargs = {}

	for arg in args:gmatch("([^,]+)") do
		local abi_change = not isptrtype(arg) or check_abi_changes(arg)

		arg = strip_arg_annotations(arg)

		local argname = arg:match("([^* ]+)$")

		-- argtype is... everything else.
		local argtype = trim(arg:gsub(argname .. "$", ""), nil)

		if argtype == "" and argname == "void" then
			goto out
		end

		argtype = argtype:gsub("intptr_t", config["abi_intptr_t"])
		argtype = argtype:gsub("semid_t", config["abi_semid_t"])
		if isptrtype(argtype) then
			argtype = argtype:gsub("size_t", config["abi_size_t"])
			argtype = argtype:gsub("^long", config["abi_long"]);
			argtype = argtype:gsub("^u_long", config["abi_u_long"]);
			argtype = argtype:gsub("^const u_long", "const " .. config["abi_u_long"]);
		elseif argtype:find("^long$") then
			argtype = config["abi_long"]
		end
		if isptrarraytype(argtype) and config["abi_ptr_array_t"] ~= "" then
			-- `* const *` -> `**`
			argtype = argtype:gsub("[*][ ]*const[ ]*[*]", "**")
			-- e.g., `struct aiocb **` -> `uint32_t *`
			argtype = argtype:gsub("[^*]*[*]", config["abi_ptr_array_t"] .. " ", 1)
		end

		-- XX TODO: Forward declarations? See: sysstubfwd in CheriBSD
		if abi_change then
			local abi_type_suffix = config["abi_type_suffix"]
			argtype = argtype:gsub("(struct [^ ]*)", "%1" ..
			    abi_type_suffix)
			argtype = argtype:gsub("(union [^ ]*)", "%1" ..
			    abi_type_suffix)
		end

		funcargs[#funcargs + 1] = {
			type = argtype,
			name = argname,
		}
	end

	::out::
	return funcargs
end

local function handle_noncompat(sysnum, thr_flag, flags, sysflags, rettype,
    auditev, syscallret, funcname, funcalias, funcargs, argalias)
	local argssize

	if #funcargs > 0 or flags & known_flags["NODEF"] ~= 0 then
		argssize = "AS(" .. argalias .. ")"
	else
		argssize = "0"
	end

	write_line("systrace", string.format([[
	/* %s */
	case %d: {
]], funcname, sysnum))
	write_line("systracetmp", string.format([[
	/* %s */
	case %d:
]], funcname, sysnum))
	write_line("systraceret", string.format([[
	/* %s */
	case %d:
]], funcname, sysnum))

	if #funcargs > 0 then
		write_line("systracetmp", "\t\tswitch (ndx) {\n")
		write_line("systrace", string.format(
		    "\t\tstruct %s *p = params;\n", argalias))

		local argtype, argname
		for idx, arg in ipairs(funcargs) do
			argtype = arg["type"]
			argname = arg["name"]

			argtype = trim(argtype:gsub("__restrict$", ""), nil)
			-- Pointer arg?
			if argtype:find("*") then
				write_line("systracetmp", string.format(
				    "\t\tcase %d:\n\t\t\tp = \"userland %s\";\n\t\t\tbreak;\n",
				    idx - 1, argtype))
			else
				write_line("systracetmp", string.format(
				    "\t\tcase %d:\n\t\t\tp = \"%s\";\n\t\t\tbreak;\n",
				    idx - 1, argtype))
			end

			if isptrtype(argtype) then
				write_line("systrace", string.format(
				    "\t\tuarg[%d] = (%s)p->%s; /* %s */\n",
				    idx - 1, config["ptr_intptr_t_cast"],
				    argname, argtype))
			elseif argtype == "union l_semun" then
				write_line("systrace", string.format(
				    "\t\tuarg[%d] = p->%s.buf; /* %s */\n",
				    idx - 1, argname, argtype))
			elseif argtype:sub(1,1) == "u" or argtype == "size_t" then
				write_line("systrace", string.format(
				    "\t\tuarg[%d] = p->%s; /* %s */\n",
				    idx - 1, argname, argtype))
			else
				write_line("systrace", string.format(
				    "\t\tiarg[%d] = p->%s; /* %s */\n",
				    idx - 1, argname, argtype))
			end
		end

		write_line("systracetmp",
		    "\t\tdefault:\n\t\t\tbreak;\n\t\t};\n")

		write_line("systraceret", string.format([[
		if (ndx == 0 || ndx == 1)
			p = "%s";
		break;
]], syscallret))
	end
	write_line("systrace", string.format(
	    "\t\t*n_args = %d;\n\t\tbreak;\n\t}\n", #funcargs))
	write_line("systracetmp", "\t\tbreak;\n")

	local nargflags = get_mask({"NOARGS", "NOPROTO", "NODEF"})
	if flags & nargflags == 0 then
		if #funcargs > 0 then
			write_line("sysarg", string.format("struct %s {\n",
			    argalias))
			for _, v in ipairs(funcargs) do
				local argname, argtype = v["name"], v["type"]
				write_line("sysarg", string.format(
				    "\tchar %s_l_[PADL_(%s)]; %s %s; char %s_r_[PADR_(%s)];\n",
				    argname, argtype,
				    argtype, argname,
				    argname, argtype))
			end
			write_line("sysarg", "};\n")
		else
			write_line("sysarg", string.format(
			    "struct %s {\n\tregister_t dummy;\n};\n", argalias))
		end
	end

	local protoflags = get_mask({"NOPROTO", "NODEF"})
	if flags & protoflags == 0 then
		if funcname == "nosys" or funcname == "lkmnosys" or
		    funcname == "sysarch" or funcname:find("^freebsd") or
		    funcname:find("^linux") then
			write_line("sysdcl", string.format(
			    "%s\t%s(struct thread *, struct %s *)",
			    rettype, funcname, argalias))
		else
			write_line("sysdcl", string.format(
			    "%s\tsys_%s(struct thread *, struct %s *)",
			    rettype, funcname, argalias))
		end
		write_line("sysdcl", ";\n")
		write_line("sysaue", string.format("#define\t%sAUE_%s\t%s\n",
		    config['syscallprefix'], funcalias, auditev))
	end

	write_line("sysent",
	    string.format("\t{ .sy_narg = %s, .sy_call = (sy_call_t *)", argssize))
	local column = 8 + 2 + #argssize + 15

	if flags & known_flags["NOSTD"] ~= 0 then
		write_line("sysent", string.format(
		    "lkmressys, .sy_auevent = AUE_NULL, " ..
		    ".sy_flags = %s, .sy_thrcnt = SY_THR_ABSENT },",
		    sysflags))
		column = column + #"lkmressys" + #"AUE_NULL" + 3
	else
		if funcname == "nosys" or funcname == "lkmnosys" or
		    funcname == "sysarch" or funcname:find("^freebsd") or
		    funcname:find("^linux") then
			write_line("sysent", string.format(
			    "%s, .sy_auevent = %s, .sy_flags = %s, .sy_thrcnt = %s },",
			    funcname, auditev, sysflags, thr_flag))
			column = column + #funcname + #auditev + #sysflags + 3
		else
			write_line("sysent", string.format(
			    "sys_%s, .sy_auevent = %s, .sy_flags = %s, .sy_thrcnt = %s },",
			    funcname, auditev, sysflags, thr_flag))
			column = column + #funcname + #auditev + #sysflags + 7
		end
	end

	align_sysent_comment(column)
	write_line("sysent", string.format("/* %d = %s */\n",
	    sysnum, funcalias))
	write_line("sysnames", string.format("\t\"%s\",\t\t\t/* %d = %s */\n",
	    funcalias, sysnum, funcalias))

	if flags & known_flags["NODEF"] == 0 then
		write_line("syshdr", string.format("#define\t%s%s\t%d\n",
		    config['syscallprefix'], funcalias, sysnum))
		write_line("sysmk", string.format(" \\\n\t%s.o",
		    funcalias))
	end
end

local function handle_obsol(sysnum, funcname, comment)
	write_line("sysent",
	    "\t{ .sy_narg = 0, .sy_call = (sy_call_t *)nosys, " ..
	    ".sy_auevent = AUE_NULL, .sy_flags = 0, .sy_thrcnt = SY_THR_ABSENT },")
	align_sysent_comment(34)

	write_line("sysent", string.format("/* %d = obsolete %s */\n",
	    sysnum, comment))
	write_line("sysnames", string.format(
	    "\t\"obs_%s\",\t\t\t/* %d = obsolete %s */\n",
	    funcname, sysnum, comment))
	write_line("syshdr", string.format("\t\t\t\t/* %d is obsolete %s */\n",
	    sysnum, comment))
end

local function handle_compat(sysnum, thr_flag, flags, sysflags, rettype,
    auditev, funcname, funcalias, funcargs, argalias)
	local argssize, out, outdcl, wrap, prefix, descr

	if #funcargs > 0 or flags & known_flags["NODEF"] ~= 0 then
		argssize = "AS(" .. argalias .. ")"
	else
		argssize = "0"
	end

	for _, v in pairs(compat_options) do
		if flags & v["mask"] ~= 0 then
			if config["mincompat"] > v["compatlevel"] then
				funcname = strip_abi_prefix(funcname)
				funcname = v["prefix"] .. funcname
				return handle_obsol(sysnum, funcname, funcname)
			end
			v["count"] = v["count"] + 1
			out = v["tmp"]
			outdcl = v["dcltmp"]
			wrap = v["flag"]:lower()
			prefix = v["prefix"]
			descr = v["descr"]
			goto compatdone
		end
	end

	::compatdone::
	local dprotoflags = get_mask({"NOPROTO", "NODEF"})
	local nargflags = dprotoflags | known_flags["NOARGS"]
	if #funcargs > 0 and flags & nargflags == 0 then
		write_line(out, string.format("struct %s {\n", argalias))
		for _, v in ipairs(funcargs) do
			local argname, argtype = v["name"], v["type"]
			write_line(out, string.format(
			    "\tchar %s_l_[PADL_(%s)]; %s %s; char %s_r_[PADR_(%s)];\n",
			    argname, argtype,
			    argtype, argname,
			    argname, argtype))
		end
		write_line(out, "};\n")
	elseif flags & nargflags == 0 then
		write_line("sysarg", string.format(
		    "struct %s {\n\tregister_t dummy;\n};\n", argalias))
	end
	if flags & dprotoflags == 0 then
		write_line(outdcl, string.format(
		    "%s\t%s%s(struct thread *, struct %s *);\n",
		    rettype, prefix, funcname, argalias))
		write_line("sysaue", string.format(
		    "#define\t%sAUE_%s%s\t%s\n", config['syscallprefix'],
		    prefix, funcname, auditev))
	end

	if flags & known_flags['NOSTD'] ~= 0 then
		write_line("sysent", string.format(
		    "\t{ .sy_narg = %s, .sy_call = (sy_call_t *)%s, " ..
		    ".sy_auevent = %s, .sy_flags = 0, " ..
		    ".sy_thrcnt = SY_THR_ABSENT },",
		    "0", "lkmressys", "AUE_NULL"))
		align_sysent_comment(8 + 2 + #"0" + 15 + #"lkmressys" +
		    #"AUE_NULL" + 3)
	else
		write_line("sysent", string.format(
		    "\t{ %s(%s,%s), .sy_auevent = %s, .sy_flags = %s, .sy_thrcnt = %s },",
		    wrap, argssize, funcname, auditev, sysflags, thr_flag))
		align_sysent_comment(8 + 9 + #argssize + 1 + #funcname +
		    #auditev + #sysflags + 4)
	end

	write_line("sysent", string.format("/* %d = %s %s */\n",
	    sysnum, descr, funcalias))
	write_line("sysnames", string.format(
	    "\t\"%s.%s\",\t\t/* %d = %s %s */\n",
	    wrap, funcalias, sysnum, descr, funcalias))
	-- Do not provide freebsdN_* symbols in libc for < FreeBSD 7
	local nosymflags = get_mask({"COMPAT", "COMPAT4", "COMPAT6"})
	if flags & nosymflags ~= 0 then
		write_line("syshdr", string.format(
		    "\t\t\t\t/* %d is %s %s */\n",
		    sysnum, descr, funcalias))
	elseif flags & known_flags["NODEF"] == 0 then
		write_line("syshdr", string.format("#define\t%s%s%s\t%d\n",
		    config['syscallprefix'], prefix, funcalias, sysnum))
		write_line("sysmk", string.format(" \\\n\t%s%s.o",
		    prefix, funcalias))
	end
end

local function handle_unimpl(sysnum, sysstart, sysend, comment)
	if sysstart == nil and sysend == nil then
		sysstart = tonumber(sysnum)
		sysend = tonumber(sysnum)
	end

	sysnum = sysstart
	while sysnum <= sysend do
		write_line("sysent", string.format(
		    "\t{ .sy_narg = 0, .sy_call = (sy_call_t *)nosys, " ..
		    ".sy_auevent = AUE_NULL, .sy_flags = 0, " ..
		    ".sy_thrcnt = SY_THR_ABSENT },\t\t\t/* %d = %s */\n",
		    sysnum, comment))
		write_line("sysnames", string.format(
		    "\t\"#%d\",\t\t\t/* %d = %s */\n",
		    sysnum, sysnum, comment))
		sysnum = sysnum + 1
	end
end

local function handle_reserved(sysnum, sysstart, sysend, comment)
	handle_unimpl(sysnum, sysstart, sysend, "reserved for local use")
end

process_syscall_def = function(line)
	local sysstart, sysend, flags, funcname, sysflags
	local thr_flag, syscallret
	local orig = line
	flags = 0
	thr_flag = "SY_THR_STATIC"

	-- Parse out the interesting information first
	local initialExpr = "^([^%s]+)%s+([^%s]+)%s+([^%s]+)%s*"
	local sysnum, auditev, allflags = line:match(initialExpr)

	if sysnum == nil or auditev == nil or allflags == nil then
		-- XXX TODO: Better?
		abort(1, "Completely malformed: " .. line)
	end

	if sysnum:find("-") then
		sysstart, sysend = sysnum:match("^([%d]+)-([%d]+)$")
		if sysstart == nil or sysend == nil then
			abort(1, "Malformed range: " .. sysnum)
		end
		sysnum = nil
		sysstart = tonumber(sysstart)
		sysend = tonumber(sysend)
		if sysstart ~= maxsyscall + 1 then
			abort(1, "syscall number out of sync, missing " ..
			    maxsyscall + 1)
		end
	else
		sysnum = tonumber(sysnum)
		if sysnum ~= maxsyscall + 1 then
			abort(1, "syscall number out of sync, missing " ..
			    maxsyscall + 1)
		end
	end

	-- Split flags
	for flag in allflags:gmatch("([^|]+)") do
		if known_flags[flag] == nil then
			abort(1, "Unknown flag " .. flag .. " for " ..  sysnum)
		end
		flags = flags | known_flags[flag]
	end

	if (flags & get_mask({"RESERVED", "UNIMPL"})) == 0 and sysnum == nil then
		abort(1, "Range only allowed with RESERVED and UNIMPL: " .. line)
	end

	if (flags & known_flags["NOTSTATIC"]) ~= 0 then
		thr_flag = "SY_THR_ABSENT"
	end

	-- Strip earlier bits out, leave declaration + alt
	line = line:gsub("^.+" .. allflags .. "%s*", "")

	local decl_fnd = line:find("^{") ~= nil
	if decl_fnd and line:find("}") == nil then
		abort(1, "Malformed, no closing brace: " .. line)
	end

	local decl, alt
	if decl_fnd then
		line = line:gsub("^{", "")
		decl, alt = line:match("([^}]*)}[%s]*(.*)$")
	else
		alt = line
	end

	if decl == nil and alt == nil then
		abort(1, "Malformed bits: " .. line)
	end

	local funcalias, funcomment, argalias, rettype, args
	if not decl_fnd and alt ~= nil and alt ~= "" then
		-- Peel off one entry for name
		funcname = trim(alt:match("^([^%s]+)"), nil)
		alt = alt:gsub("^([^%s]+)[%s]*", "")
	end
	-- Do we even need it?
	if flags & get_mask({"OBSOL", "UNIMPL"}) ~= 0 then
		local NF = 0
		for _ in orig:gmatch("[^%s]+") do
			NF = NF + 1
		end

		funcomment = funcname or ''
		if NF < 6 then
			funcomment = funcomment .. " " .. alt
		end

		funcomment = trim(funcomment)

--		if funcname ~= nil then
--		else
--			funcomment = trim(alt)
--		end
		goto skipalt
	end

	if alt ~= nil and alt ~= "" then
		local altExpr = "^([^%s]+)%s+([^%s]+)%s+([^%s]+)"
		funcalias, argalias, rettype = alt:match(altExpr)
		funcalias = trim(funcalias)
		if funcalias == nil or argalias == nil or rettype == nil then
			abort(1, "Malformed alt: " .. line)
		end
	end
	if decl_fnd then
		-- Don't clobber rettype set in the alt information
		if rettype == nil then
			rettype = "int"
		end
		-- Peel off the return type
		syscallret = line:match("([^%s]+)%s")
		line = line:match("[^%s]+%s(.+)")
		-- Pointer incoming
		if line:sub(1,1) == "*" then
			syscallret = syscallret .. " "
		end
		while line:sub(1,1) == "*" do
			line = line:sub(2)
			syscallret = syscallret .. "*"
		end
		funcname = line:match("^([^(]+)%(")
		if funcname == nil then
			abort(1, "Not a signature? " .. line)
		end
		args = line:match("^[^(]+%((.+)%)[^)]*$")
		args = trim(args, '[,%s]')
	end

	::skipalt::

	if funcname == nil then
		funcname = funcalias
	end

	funcname = trim(funcname)

	sysflags = "0"

	-- NODEF events do not get audited
	if flags & known_flags['NODEF'] ~= 0 then
		auditev = 'AUE_NULL'
	end

	-- If applicable; strip the ABI prefix from the name
	local stripped_name = strip_abi_prefix(funcname)

	if flags & known_flags['CAPENABLED'] ~= 0 or
	    config["capenabled"][funcname] ~= nil or
	    config["capenabled"][stripped_name] ~= nil then
		sysflags = "SYF_CAPENABLED"
	end

	local funcargs = {}
	if args ~= nil then
		funcargs = process_args(args)
	end

	local argprefix = ''
	if abi_changes("pointer_args") then
		for _, v in ipairs(funcargs) do
			if isptrtype(v["type"]) then
				-- argalias should be:
				--   COMPAT_PREFIX + ABI Prefix + funcname
				argprefix = config['abi_func_prefix']
				funcalias = config['abi_func_prefix'] ..
				    funcname
				goto ptrfound
			end
		end
		::ptrfound::
	end
	if funcalias == nil or funcalias == "" then
		funcalias = funcname
	end

	if argalias == nil and funcname ~= nil then
		argalias = argprefix .. funcname .. "_args"
		for _, v in pairs(compat_options) do
			local mask = v["mask"]
			if (flags & mask) ~= 0 then
				-- Multiple aliases doesn't seem to make
				-- sense.
				argalias = v["prefix"] .. argalias
				goto out
			end
		end
		::out::
	elseif argalias ~= nil then
		argalias = argprefix .. argalias
	end

	local ncompatflags = get_mask({"STD", "NODEF", "NOARGS", "NOPROTO",
	    "NOSTD"})
	local compatflags = get_mask_pat("COMPAT.*")
	-- Now try compat...
	if flags & compatflags ~= 0 then
		if flags & known_flags['STD'] ~= 0 then
			abort(1, "Incompatible COMPAT/STD: " .. line)
		end
		handle_compat(sysnum, thr_flag, flags, sysflags, rettype,
		    auditev, funcname, funcalias, funcargs, argalias)
	elseif flags & ncompatflags ~= 0 then
		handle_noncompat(sysnum, thr_flag, flags, sysflags, rettype,
		    auditev, syscallret, funcname, funcalias, funcargs,
		    argalias)
	elseif flags & known_flags["OBSOL"] ~= 0 then
		handle_obsol(sysnum, funcname, funcomment)
	elseif flags & known_flags["RESERVED"] ~= 0 then
		handle_reserved(sysnum, sysstart, sysend)
	elseif flags & known_flags["UNIMPL"] ~= 0 then
		handle_unimpl(sysnum, sysstart, sysend, funcomment)
	else
		abort(1, "Bad flags? " .. line)
	end

	if sysend ~= nil then
		maxsyscall = sysend
	elseif sysnum ~= nil then
		maxsyscall = sysnum
	end
end

-- Entry point

if #arg < 1 or #arg > 2 then
	error("usage: " .. arg[0] .. " input-file <config-file>")
end

local sysfile, configfile = arg[1], arg[2]

-- process_config either returns nil and a message, or a
-- table that we should merge into the global config
if configfile ~= nil then
	local res = assert(process_config(configfile))

	for k, v in pairs(res) do
		if v ~= config[k] then
			config[k] = v
			config_modified[k] = true
		end
	end
end

-- We ignore errors here if we're relying on the default configuration.
if not config_modified["capenabled"] then
	config["capenabled"] = grab_capenabled(config['capabilities_conf'],
	    config_modified["capabilities_conf"] == nil)
elseif config["capenabled"] ~= "" then
	-- Due to limitations in the config format mostly, we'll have a comma
	-- separated list.  Parse it into lines
	local capenabled = {}
	-- print("here: " .. config["capenabled"])
	for sysc in config["capenabled"]:gmatch("([^,]+)") do
		capenabled[sysc] = true
	end
	config["capenabled"] = capenabled
end
process_compat()
process_abi_flags()

if not lfs.mkdir(tmpspace) then
	error("Failed to create tempdir " .. tmpspace)
end

-- XXX Revisit the error handling here, we should probably move the rest of this
-- into a function that we pcall() so we can catch the errors and clean up
-- gracefully.
for _, v in ipairs(temp_files) do
	local tmpname = tmpspace .. v
	files[v] = io.open(tmpname, "w+")
	-- XXX Revisit these with a pcall() + error handler
	if not files[v] then
		abort(1, "Failed to open temp file: " .. tmpname)
	end
end

for _, v in ipairs(output_files) do
	local tmpname = tmpspace .. v
	files[v] = io.open(tmpname, "w+")
	-- XXX Revisit these with a pcall() + error handler
	if not files[v] then
		abort(1, "Failed to open temp output file: " .. tmpname)
	end
end

-- Write out all of the preamble bits
write_line("sysent", string.format([[

/* The casts are bogus but will do for now. */
struct sysent %s[] = {
]], config['switchname']))

write_line("syssw", string.format([[/*
 * System call switch table.
 *
 * DO NOT EDIT-- this file is automatically %s.
 * $%s$
 */

]], generated_tag, config['os_id_keyword']))

write_line("sysarg", string.format([[/*
 * System call prototypes.
 *
 * DO NOT EDIT-- this file is automatically %s.
 * $%s$
 */

#ifndef %s
#define	%s

#include <sys/signal.h>
#include <sys/acl.h>
#include <sys/cpuset.h>
#include <sys/domainset.h>
#include <sys/_ffcounter.h>
#include <sys/_semaphore.h>
#include <sys/ucontext.h>
#include <sys/wait.h>

#include <bsm/audit_kevents.h>

struct proc;

struct thread;

#define	PAD_(t)	(sizeof(register_t) <= sizeof(t) ? \
		0 : sizeof(register_t) - sizeof(t))

#if BYTE_ORDER == LITTLE_ENDIAN
#define	PADL_(t)	0
#define	PADR_(t)	PAD_(t)
#else
#define	PADL_(t)	PAD_(t)
#define	PADR_(t)	0
#endif

]], generated_tag, config['os_id_keyword'], config['sysproto_h'],
    config['sysproto_h']))
for _, v in pairs(compat_options) do
	write_line(v["tmp"], string.format("\n#ifdef %s\n\n", v["definition"]))
end

write_line("sysnames", string.format([[/*
 * System call names.
 *
 * DO NOT EDIT-- this file is automatically %s.
 * $%s$
 */

const char *%s[] = {
]], generated_tag, config['os_id_keyword'], config['namesname']))

write_line("syshdr", string.format([[/*
 * System call numbers.
 *
 * DO NOT EDIT-- this file is automatically %s.
 * $%s$
 */

]], generated_tag, config['os_id_keyword']))

write_line("sysmk", string.format([[# FreeBSD system call object files.
# DO NOT EDIT-- this file is automatically %s.
# $%s$
MIASM = ]], generated_tag, config['os_id_keyword']))

write_line("systrace", string.format([[/*
 * System call argument to DTrace register array converstion.
 *
 * DO NOT EDIT-- this file is automatically %s.
 * $%s$
 * This file is part of the DTrace syscall provider.
 */

static void
systrace_args(int sysnum, void *params, uint64_t *uarg, int *n_args)
{
	int64_t *iarg = (int64_t *)uarg;
	switch (sysnum) {
]], generated_tag, config['os_id_keyword']))

write_line("systracetmp", [[static void
systrace_entry_setargdesc(int sysnum, int ndx, char *desc, size_t descsz)
{
	const char *p = NULL;
	switch (sysnum) {
]])

write_line("systraceret", [[static void
systrace_return_setargdesc(int sysnum, int ndx, char *desc, size_t descsz)
{
	const char *p = NULL;
	switch (sysnum) {
]])

-- Processing the sysfile will parse out the preprocessor bits and put them into
-- the appropriate place.  Any syscall-looking lines get thrown into the sysfile
-- buffer, one per line, for later processing once they're all glued together.
process_sysfile(sysfile)

write_line("sysinc",
    "\n#define AS(name) (sizeof(struct name) / sizeof(register_t))\n")

for _, v in pairs(compat_options) do
	if v["count"] > 0 then
		write_line("sysinc", string.format([[

#ifdef %s
#define %s(n, name) .sy_narg = n, .sy_call = (sy_call_t *)__CONCAT(%s, name)
#else
#define %s(n, name) .sy_narg = 0, .sy_call = (sy_call_t *)nosys
#endif
]], v["definition"], v["flag"]:lower(), v["prefix"], v["flag"]:lower()))
	end

	write_line(v["dcltmp"], string.format("\n#endif /* %s */\n\n",
	    v["definition"]))
end

write_line("sysprotoend", string.format([[

#undef PAD_
#undef PADL_
#undef PADR_

#endif /* !%s */
]], config["sysproto_h"]))

write_line("sysmk", "\n")
write_line("sysent", "};\n")
write_line("sysnames", "};\n")
-- maxsyscall is the highest seen; MAXSYSCALL should be one higher
write_line("syshdr", string.format("#define\t%sMAXSYSCALL\t%d\n",
    config["syscallprefix"], maxsyscall + 1))
write_line("systrace", [[
	default:
		*n_args = 0;
		break;
	};
}
]])

write_line("systracetmp", [[
	default:
		break;
	};
	if (p != NULL)
		strlcpy(desc, p, descsz);
}
]])

write_line("systraceret", [[
	default:
		break;
	};
	if (p != NULL)
		strlcpy(desc, p, descsz);
}
]])

-- Finish up; output
write_line("syssw", read_file("sysinc"))
write_line("syssw", read_file("sysent"))

write_line("sysproto", read_file("sysarg"))
write_line("sysproto", read_file("sysdcl"))
for _, v in pairs(compat_options) do
	write_line("sysproto", read_file(v["tmp"]))
	write_line("sysproto", read_file(v["dcltmp"]))
end
write_line("sysproto", read_file("sysaue"))
write_line("sysproto", read_file("sysprotoend"))

write_line("systrace", read_file("systracetmp"))
write_line("systrace", read_file("systraceret"))

for _, v in ipairs(output_files) do
	local target = config[v]
	if target ~= "/dev/null" then
		local fh = assert(io.open(target, "w+"))
		if fh == nil then
			abort(1, "Failed to open '" .. target .. "'")
		end
		assert(fh:write(read_file(v)))
		assert(fh:close())
	end
end

cleanup()
