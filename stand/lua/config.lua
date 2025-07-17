--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2015 Pedro Souza <pedrosouza@freebsd.org>
-- Copyright (c) 2018 Kyle Evans <kevans@FreeBSD.org>
-- All rights reserved.
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

local hook = require("hook")

local config = {}
local modules = {}
local carousel_choices = {}
-- Which variables we changed
local env_changed = {}
-- Values to restore env to (nil to unset)
local env_restore = {}

local MSG_FAILDIR = "Failed to load conf dir '%s': not a directory"
local MSG_FAILEXEC = "Failed to exec '%s'"
local MSG_FAILSETENV = "Failed to '%s' with value: %s"
local MSG_FAILOPENCFG = "Failed to open config: '%s'"
local MSG_FAILREADCFG = "Failed to read config: '%s'"
local MSG_FAILPARSECFG = "Failed to parse config: '%s'"
local MSG_FAILEXECLUA = "Failed to execute lua conf '%s': '%s'"
local MSG_FAILPARSEVAR = "Failed to parse variable '%s': %s"
local MSG_FAILEXBEF = "Failed to execute '%s' before loading '%s'"
local MSG_FAILEXAF = "Failed to execute '%s' after loading '%s'"
local MSG_MALFORMED = "Malformed line (%d):\n\t'%s'"
local MSG_DEFAULTKERNFAIL = "No kernel set, failed to load from module_path"
local MSG_KERNFAIL = "Failed to load kernel '%s'"
local MSG_XENKERNFAIL = "Failed to load Xen kernel '%s'"
local MSG_XENKERNLOADING = "Loading Xen kernel..."
local MSG_KERNLOADING = "Loading kernel..."
local MSG_MODLOADING = "Loading configured modules..."
local MSG_MODBLACKLIST = "Not loading blacklisted module '%s'"

local MSG_FAILSYN_QUOTE = "Stray quote at position '%d'"
local MSG_FAILSYN_EOLESC = "Stray escape at end of line"
local MSG_FAILSYN_EOLVAR = "Unescaped $ at end of line"
local MSG_FAILSYN_BADVAR = "Malformed variable expression at position '%d'"

-- MODULEEXPR should more or less allow the exact same set of characters as the
-- env_var entries in the pattern table.  This is perhaps a good target for a
-- little refactoring.
local MODULEEXPR = '([%w%d-_.]+)'
local QVALEXPR = '"([^"]*)"'
local QVALREPL = QVALEXPR:gsub('%%', '%%%%')
local WORDEXPR = "([-%w%d][-%w%d_.]*)"
local WORDREPL = WORDEXPR:gsub('%%', '%%%%')

-- Entries that should never make it into the environment; each one should have
-- a documented reason for its existence, and these should all be implementation
-- details of the config module.
local loader_env_restricted_table = {
	-- loader_conf_files should be considered write-only, and consumers
	-- should not rely on any particular value; it's a loader implementation
	-- detail.  Moreover, it's not a particularly useful variable to have in
	-- the kenv.  Save the overhead, let it get fetched other ways.
	loader_conf_files = true,
}

local function restoreEnv()
	-- Examine changed environment variables
	for k, v in pairs(env_changed) do
		local restore_value = env_restore[k]
		if restore_value == nil then
			-- This one doesn't need restored for some reason
			goto continue
		end
		local current_value = loader.getenv(k)
		if current_value ~= v then
			-- This was overwritten by some action taken on the menu
			-- most likely; we'll leave it be.
			goto continue
		end
		restore_value = restore_value.value
		if restore_value ~= nil then
			loader.setenv(k, restore_value)
		else
			loader.unsetenv(k)
		end
		::continue::
	end

	env_changed = {}
	env_restore = {}
end

-- XXX This getEnv/setEnv should likely be exported at some point.  We can save
-- the call back into loader.getenv for any variable that's been set or
-- overridden by any loader.conf using this implementation with little overhead
-- since we're already tracking the values.
local function getEnv(key)
	if loader_env_restricted_table[key] ~= nil or
	    env_changed[key] ~= nil then
		return env_changed[key]
	end

	return loader.getenv(key)
end

local function setEnv(key, value)
	env_changed[key] = value

	if loader_env_restricted_table[key] ~= nil then
		return 0
	end

	-- Track the original value for this if we haven't already
	if env_restore[key] == nil then
		env_restore[key] = {value = loader.getenv(key)}
	end

	return loader.setenv(key, value)
end

-- name here is one of 'name', 'type', flags', 'before', 'after', or 'error.'
-- These are set from lines in loader.conf(5): ${key}_${name}="${value}" where
-- ${key} is a module name.
local function setKey(key, name, value)
	if modules[key] == nil then
		modules[key] = {}
	end
	modules[key][name] = value
end

-- Escapes the named value for use as a literal in a replacement pattern.
-- e.g. dhcp.host-name gets turned into dhcp%.host%-name to remove the special
-- meaning.
local function escapeName(name)
	return name:gsub("([%p])", "%%%1")
end

local function processEnvVar(value)
	local pval, vlen = '', #value
	local nextpos, vdelim, vinit = 1
	local vpat
	for i = 1, vlen do
		if i < nextpos then
			goto nextc
		end

		local c = value:sub(i, i)
		if c == '\\' then
			if i == vlen then
				return nil, MSG_FAILSYN_EOLESC
			end
			nextpos = i + 2
			pval = pval .. value:sub(i + 1, i + 1)
		elseif c == '"' then
			return nil, MSG_FAILSYN_QUOTE:format(i)
		elseif c == "$" then
			if i == vlen then
				return nil, MSG_FAILSYN_EOLVAR
			else
				if value:sub(i + 1, i + 1) == "{" then
					-- Skip ${
					vinit = i + 2
					vdelim = '}'
					vpat = "^([^}]+)}"
				else
					-- Skip the $
					vinit = i + 1
					vdelim = nil
					vpat = "^([%w][-%w%d_.]*)"
				end

				local name = value:match(vpat, vinit)
				if not name then
					return nil, MSG_FAILSYN_BADVAR:format(i)
				else
					nextpos = vinit + #name
					if vdelim then
						nextpos = nextpos + 1
					end

					local repl = loader.getenv(name) or ""
					pval = pval .. repl
				end
			end
		else
			pval = pval .. c
		end
		::nextc::
	end

	return pval
end

local function checkPattern(line, pattern)
	local function _realCheck(_line, _pattern)
		return _line:match(_pattern)
	end

	if pattern:find('$VALUE') then
		local k, v, c
		k, v, c = _realCheck(line, pattern:gsub('$VALUE', QVALREPL))
		if k ~= nil then
			return k,v, c
		end
		return _realCheck(line, pattern:gsub('$VALUE', WORDREPL))
	else
		return _realCheck(line, pattern)
	end
end

-- str in this table is a regex pattern.  It will automatically be anchored to
-- the beginning of a line and any preceding whitespace will be skipped.  The
-- pattern should have no more than two captures patterns, which correspond to
-- the two parameters (usually 'key' and 'value') that are passed to the
-- process function.  All trailing characters will be validated.  Any $VALUE
-- token included in a pattern will be tried first with a quoted value capture
-- group, then a single-word value capture group.  This is our kludge for Lua
-- regex not supporting branching.
--
-- We have two special entries in this table: the first is the first entry,
-- a full-line comment.  The second is for 'exec' handling.  Both have a single
-- capture group, but the difference is that the full-line comment pattern will
-- match the entire line.  This does not run afoul of the later end of line
-- validation that we'll do after a match.  However, the 'exec' pattern will.
-- We document the exceptions with a special 'groups' index that indicates
-- the number of capture groups, if not two.  We'll use this later to do
-- validation on the proper entry.
--
local pattern_table = {
	{
		luaexempt = true,
		str = "(#.*)",
		process = function(_, _)  end,
		groups = 1,
	},
	--  module_load="value"
	{
		name = MODULEEXPR .. "_load%s*",
		val = "%s*$VALUE",
		process = function(k, v)
			if modules[k] == nil then
				modules[k] = {}
			end
			modules[k].load = v:upper()
			setEnv(k .. "_load", v:upper())
		end,
	},
	--  module_name="value"
	{
		name = MODULEEXPR .. "_name%s*",
		val = "%s*$VALUE",
		process = function(k, v)
			setKey(k, "name", v)
			setEnv(k .. "_name", v)
		end,
	},
	--  module_type="value"
	{
		name = MODULEEXPR .. "_type%s*",
		val = "%s*$VALUE",
		process = function(k, v)
			setKey(k, "type", v)
			setEnv(k .. "_type", v)
		end,
	},
	--  module_flags="value"
	{
		name = MODULEEXPR .. "_flags%s*",
		val = "%s*$VALUE",
		process = function(k, v)
			setKey(k, "flags", v)
			setEnv(k .. "_flags", v)
		end,
	},
	--  module_before="value"
	{
		name = MODULEEXPR .. "_before%s*",
		val = "%s*$VALUE",
		process = function(k, v)
			setKey(k, "before", v)
			setEnv(k .. "_before", v)
		end,
	},
	--  module_after="value"
	{
		name = MODULEEXPR .. "_after%s*",
		val = "%s*$VALUE",
		process = function(k, v)
			setKey(k, "after", v)
			setEnv(k .. "_after", v)
		end,
	},
	--  module_error="value"
	{
		name = MODULEEXPR .. "_error%s*",
		val = "%s*$VALUE",
		process = function(k, v)
			setKey(k, "error", v)
			setEnv(k .. "_error", v)
		end,
	},
	--  exec="command"
	{
		luaexempt = true,
		name = "exec%s*",
		val = "%s*" .. QVALEXPR,
		process = function(k, _)
			if cli_execute_unparsed(k) ~= 0 then
				print(MSG_FAILEXEC:format(k))
			end
		end,
		groups = 1,
	},
	--  env_var="value" or env_var=[word|num]
	{
		name = "([%w][%w%d-_.]*)%s*",
		val = "%s*$VALUE",
		process = function(k, v)
			local pv, msg = processEnvVar(v)
			if not pv then
				print(MSG_FAILPARSEVAR:format(k, msg))
				return
			end
			if setEnv(k, pv) ~= 0 then
				print(MSG_FAILSETENV:format(k, v))
			else
				return pv
			end
		end,
	},
}

local function isValidComment(line)
	if line ~= nil then
		local s = line:match("^%s*#.*")
		if s == nil then
			s = line:match("^%s*$")
		end
		if s == nil then
			return false
		end
	end
	return true
end

local function getBlacklist()
	local blacklist = {}
	local blacklist_str = loader.getenv('module_blacklist')
	if blacklist_str == nil then
		return blacklist
	end

	for mod in blacklist_str:gmatch("[;, ]?([-%w_]+)[;, ]?") do
		blacklist[mod] = true
	end
	return blacklist
end

local function loadModule(mod, silent)
	local status = true
	local blacklist = getBlacklist()
	local pstatus
	for k, v in pairs(mod) do
		if v.load ~= nil and v.load:lower() == "yes" then
			local module_name = v.name or k
			if not v.force and blacklist[module_name] ~= nil then
				if not silent then
					print(MSG_MODBLACKLIST:format(module_name))
				end
				goto continue
			end
			if not silent then
				loader.printc(module_name .. "...")
			end
			local str = "load "
			if v.type ~= nil then
				str = str .. "-t " .. v.type .. " "
			end
			str = str .. module_name
			if v.flags ~= nil then
				str = str .. " " .. v.flags
			end
			if v.before ~= nil then
				pstatus = cli_execute_unparsed(v.before) == 0
				if not pstatus and not silent then
					print(MSG_FAILEXBEF:format(v.before, k))
				end
				status = status and pstatus
			end

			if cli_execute_unparsed(str) ~= 0 then
				-- XXX Temporary shim: don't break the boot if
				-- loader hadn't been recompiled with this
				-- function exposed.
				if loader.command_error then
					print(loader.command_error())
				end
				if not silent then
					print("failed!")
				end
				if v.error ~= nil then
					cli_execute_unparsed(v.error)
				end
				status = false
			elseif v.after ~= nil then
				pstatus = cli_execute_unparsed(v.after) == 0
				if not pstatus and not silent then
					print(MSG_FAILEXAF:format(v.after, k))
				end
				if not silent then
					print("ok")
				end
				status = status and pstatus
			end
		end
		::continue::
	end

	return status
end

local function readFile(name, silent)
	local f = io.open(name)
	if f == nil then
		if not silent then
			print(MSG_FAILOPENCFG:format(name))
		end
		return nil
	end

	local text, _ = io.read(f)
	-- We might have read in the whole file, this won't be needed any more.
	io.close(f)

	if text == nil and not silent then
		print(MSG_FAILREADCFG:format(name))
	end
	return text
end

local function checkNextboot()
	local nextboot_file = loader.getenv("nextboot_conf")
	local nextboot_enable = loader.getenv("nextboot_enable")

	if nextboot_file == nil then
		return
	end

	-- is nextboot_enable set in nvstore?
	if nextboot_enable == "NO" then
		return
	end

	local text = readFile(nextboot_file, true)
	if text == nil then
		return
	end

	if nextboot_enable == nil and
	    text:match("^nextboot_enable=\"NO\"") ~= nil then
		-- We're done; nextboot is not enabled
		return
	end

	if not config.parse(text) then
		print(MSG_FAILPARSECFG:format(nextboot_file))
	end

	-- Attempt to rewrite the first line and only the first line of the
	-- nextboot_file. We overwrite it with nextboot_enable="NO", then
	-- check for that on load.
	-- It's worth noting that this won't work on every filesystem, so we
	-- won't do anything notable if we have any errors in this process.
	local nfile = io.open(nextboot_file, 'w')
	if nfile ~= nil then
		-- We need the trailing space here to account for the extra
		-- character taken up by the string nextboot_enable="YES"
		-- Or new end quotation mark lands on the S, and we want to
		-- rewrite the entirety of the first line.
		io.write(nfile, "nextboot_enable=\"NO\" ")
		io.close(nfile)
	end
	loader.setenv("nextboot_enable", "NO")
end

local function processEnv(k, v)
	for _, val in ipairs(pattern_table) do
		if not val.luaexempt and val.name then
			local matched = k:match(val.name)

			if matched then
				return val.process(matched, v)
			end
		end
	end
end

-- Module exports
config.verbose = false

-- The first item in every carousel is always the default item.
function config.getCarouselIndex(id)
	return carousel_choices[id] or 1
end

function config.setCarouselIndex(id, idx)
	carousel_choices[id] = idx
end

-- Returns true if we processed the file successfully, false if we did not.
-- If 'silent' is true, being unable to read the file is not considered a
-- failure.
function config.processFile(name, silent)
	if silent == nil then
		silent = false
	end

	local text = readFile(name, silent)
	if text == nil then
		return silent
	end

	if name:match(".lua$") then
		local cfg_env = setmetatable({}, {
			indices = {},
			__index = function(env, key)
				if getmetatable(env).indices[key] then
					return rawget(env, key)
				end

				return loader.getenv(key)
			end,
			__newindex = function(env, key, val)
				getmetatable(env).indices[key] = true
				rawset(env, key, val)
			end,
		})

		-- Give local modules a chance to populate the config
		-- environment.
		hook.runAll("config.buildenv", cfg_env)
		local res, err = pcall(load(text, name, "t", cfg_env))
		if res then
			for k, v in pairs(cfg_env) do
				local t = type(v)
				if t ~= "function" and t ~= "table" then
					if t ~= "string" then
						v = tostring(v)
					end
					local pval = processEnv(k, v)
					if pval then
						setEnv(k, pval)
					end
				end
			end
		else
			print(MSG_FAILEXECLUA:format(name, err))
		end

		return res
	else
		return config.parse(text)
	end
end

-- silent runs will not return false if we fail to open the file
function config.parse(text)
	local n = 1
	local status = true

	for line in text:gmatch("([^\n]+)") do
		if line:match("^%s*$") == nil then
			for _, val in ipairs(pattern_table) do
				if val.str == nil then
					val.str = val.name .. "=" .. val.val
				end
				local pattern = '^%s*' .. val.str .. '%s*(.*)';
				local cgroups = val.groups or 2
				local k, v, c = checkPattern(line, pattern)
				if k ~= nil then
					-- Offset by one, drats
					if cgroups == 1 then
						c = v
						v = nil
					end

					if isValidComment(c) then
						val.process(k, v)
						goto nextline
					end

					break
				end
			end

			print(MSG_MALFORMED:format(n, line))
			status = false
		end
		::nextline::
		n = n + 1
	end

	return status
end

function config.readConf(file, loaded_files)
	if loaded_files == nil then
		loaded_files = {}
	end

	if loaded_files[file] ~= nil then
		return
	end

	local top_level = next(loaded_files) == nil -- Are we the top-level readConf?
	print("Loading " .. file)

	-- The final value of loader_conf_files is not important, so just
	-- clobber it here.  We'll later check if it's no longer nil and process
	-- the new value for files to read.
	setEnv("loader_conf_files", nil)

	-- These may or may not exist, and that's ok. Do a
	-- silent parse so that we complain on parse errors but
	-- not for them simply not existing.
	if not config.processFile(file, true) then
		print(MSG_FAILPARSECFG:format(file))
	end

	loaded_files[file] = true

	-- Going to process "loader_conf_files" extra-files
	local loader_conf_files = getEnv("loader_conf_files")
	if loader_conf_files ~= nil then
		for name in loader_conf_files:gmatch("[%w%p]+") do
			config.readConf(name, loaded_files)
		end
	end

	if top_level then
		local loader_conf_dirs = getEnv("loader_conf_dirs")

		-- If product_vars is set, it must be a list of environment variable names
		-- to walk through to guess product information. The order matters as
		-- reading a config files override the previously defined values.
		--
		-- If product information can be guessed, for each product information
		-- found, also read config files found in /boot/loader.conf.d/PRODUCT/.
		local product_vars = getEnv("product_vars")
		if product_vars then
			local product_conf_dirs = ""
			for var in product_vars:gmatch("%S+") do
				local product = getEnv(var)
				if product then
					product_conf_dirs = product_conf_dirs .. " /boot/loader.conf.d/" .. product
				end
			end

			if loader_conf_dirs then
				loader_conf_dirs = loader_conf_dirs .. product_conf_dirs
			else
				loader_conf_dirs = product_conf_dirs
			end
		end

		-- Process "loader_conf_dirs" extra-directories
		if loader_conf_dirs ~= nil then
			for name in loader_conf_dirs:gmatch("[%w%p]+") do
				if lfs.attributes(name, "mode") ~= "directory" then
					print(MSG_FAILDIR:format(name))
					goto nextdir
				end

				for cfile in lfs.dir(name) do
					if cfile:match(".conf$") then
						local fpath = name .. "/" .. cfile
						if lfs.attributes(fpath, "mode") == "file" then
							config.readConf(fpath, loaded_files)
						end
					end
				end
				::nextdir::
			end
		end

		-- Always allow overriding with local config files, e.g.,
		-- /boot/loader.conf.local.
		local local_loader_conf_files = getEnv("local_loader_conf_files")
		if local_loader_conf_files then
			for name in local_loader_conf_files:gmatch("[%w%p]+") do
				config.readConf(name, loaded_files)
			end
		end
	end
end

-- other_kernel is optionally the name of a kernel to load, if not the default
-- or autoloaded default from the module_path
function config.loadKernel(other_kernel)
	local flags = loader.getenv("kernel_options") or ""
	local kernel = other_kernel or loader.getenv("kernel")

	local function tryLoad(names)
		for name in names:gmatch("([^;]+)%s*;?") do
			local r = loader.perform("load " .. name ..
			     " " .. flags)
			if r == 0 then
				return name
			end
		end
		return nil
	end

	local function getModulePath()
		local module_path = loader.getenv("module_path")
		local kernel_path = loader.getenv("kernel_path")

		if kernel_path == nil then
			return module_path
		end

		-- Strip the loaded kernel path from module_path. This currently assumes
		-- that the kernel path will be prepended to the module_path when it's
		-- found.
		kernel_path = escapeName(kernel_path .. ';')
		return module_path:gsub(kernel_path, '')
	end

	local function loadBootfile()
		local bootfile = loader.getenv("bootfile")

		-- append default kernel name
		if bootfile == nil then
			bootfile = "kernel"
		else
			bootfile = bootfile .. ";kernel"
		end

		return tryLoad(bootfile)
	end

	-- kernel not set, try load from default module_path
	if kernel == nil then
		local res = loadBootfile()

		if res ~= nil then
			-- Default kernel is loaded
			config.kernel_loaded = nil
			return true
		else
			print(MSG_DEFAULTKERNFAIL)
			return false
		end
	else
		-- Use our cached module_path, so we don't end up with multiple
		-- automatically added kernel paths to our final module_path
		local module_path = getModulePath()
		local res

		if other_kernel ~= nil then
			kernel = other_kernel
		end
		-- first try load kernel with module_path = /boot/${kernel}
		-- then try load with module_path=${kernel}
		local paths = {"/boot/" .. kernel, kernel}

		for _, v in pairs(paths) do
			loader.setenv("module_path", v)
			res = loadBootfile()

			-- succeeded, add path to module_path
			if res ~= nil then
				config.kernel_loaded = kernel
				if module_path ~= nil then
					loader.setenv("module_path", v .. ";" ..
					    module_path)
					loader.setenv("kernel_path", v)
				end
				return true
			end
		end

		-- failed to load with ${kernel} as a directory
		-- try as a file
		res = tryLoad(kernel)
		if res ~= nil then
			config.kernel_loaded = kernel
			return true
		else
			print(MSG_KERNFAIL:format(kernel))
			return false
		end
	end
end

function config.selectKernel(kernel)
	config.kernel_selected = kernel
end

function config.load(file, reloading)
	if not file then
		file = "/boot/defaults/loader.conf"
	end

	config.readConf(file)

	checkNextboot()

	local verbose = loader.getenv("verbose_loading") or "no"
	config.verbose = verbose:lower() == "yes"
	if not reloading then
		hook.runAll("config.loaded")
	end
end

-- Reload configuration
function config.reload(file)
	modules = {}
	restoreEnv()
	config.load(file, true)
	hook.runAll("config.reloaded")
end

function config.loadelf()
	local xen_kernel = loader.getenv('xen_kernel')
	local kernel = config.kernel_selected or config.kernel_loaded
	local status

	if xen_kernel ~= nil then
		print(MSG_XENKERNLOADING)
		if cli_execute_unparsed('load ' .. xen_kernel) ~= 0 then
			print(MSG_XENKERNFAIL:format(xen_kernel))
			return false
		end
	end
	print(MSG_KERNLOADING)
	if not config.loadKernel(kernel) then
		return false
	end
	hook.runAll("kernel.loaded")

	print(MSG_MODLOADING)
	status = loadModule(modules, not config.verbose)
	hook.runAll("modules.loaded")
	return status
end

function config.enableModule(modname)
	if modules[modname] == nil then
		modules[modname] = {}
	elseif modules[modname].load == "YES" then
		modules[modname].force = true
		return true
	end

	modules[modname].load = "YES"
	modules[modname].force = true
	return true
end

function config.disableModule(modname)
	if modules[modname] == nil then
		return false
	elseif modules[modname].load ~= "YES" then
		return true
	end

	modules[modname].load = "NO"
	modules[modname].force = nil
	return true
end

function config.isModuleEnabled(modname)
	local mod = modules[modname]
	if not mod or mod.load ~= "YES" then
		return false
	end

	if mod.force then
		return true
	end

	local blacklist = getBlacklist()
	return not blacklist[modname]
end

function config.getModuleInfo()
	return {
		modules = modules,
		blacklist = getBlacklist()
	}
end

hook.registerType("config.buildenv")
hook.registerType("config.loaded")
hook.registerType("config.reloaded")
hook.registerType("kernel.loaded")
hook.registerType("modules.loaded")
return config
