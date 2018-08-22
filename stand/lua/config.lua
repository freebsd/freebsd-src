--
-- SPDX-License-Identifier: BSD-2-Clause-FreeBSD
--
-- Copyright (c) 2015 Pedro Souza <pedrosouza@freebsd.org>
-- Copyright (C) 2018 Kyle Evans <kevans@FreeBSD.org>
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
-- $FreeBSD$
--

local hook = require("hook")

local config = {}
local modules = {}
local carousel_choices = {}
-- Which variables we changed
local env_changed = {}
-- Values to restore env to (nil to unset)
local env_restore = {}

local MSG_FAILEXEC = "Failed to exec '%s'"
local MSG_FAILSETENV = "Failed to '%s' with value: %s"
local MSG_FAILOPENCFG = "Failed to open config: '%s'"
local MSG_FAILREADCFG = "Failed to read config: '%s'"
local MSG_FAILPARSECFG = "Failed to parse config: '%s'"
local MSG_FAILEXBEF = "Failed to execute '%s' before loading '%s'"
local MSG_FAILEXMOD = "Failed to execute '%s'"
local MSG_FAILEXAF = "Failed to execute '%s' after loading '%s'"
local MSG_MALFORMED = "Malformed line (%d):\n\t'%s'"
local MSG_DEFAULTKERNFAIL = "No kernel set, failed to load from module_path"
local MSG_KERNFAIL = "Failed to load kernel '%s'"
local MSG_KERNLOADING = "Loading kernel..."
local MSG_MODLOADING = "Loading configured modules..."
local MSG_MODLOADFAIL = "Could not load one or more modules!"

local MODULEEXPR = '([%w-_]+)'

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

local function setEnv(key, value)
	-- Track the original value for this if we haven't already
	if env_restore[key] == nil then
		env_restore[key] = {value = loader.getenv(key)}
	end

	env_changed[key] = value

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
	for name in value:gmatch("${([^}]+)}") do
		local replacement = loader.getenv(name) or ""
		value = value:gsub("${" .. escapeName(name) .. "}", replacement)
	end
	for name in value:gmatch("$([%w%p]+)%s*") do
		local replacement = loader.getenv(name) or ""
		value = value:gsub("$" .. escapeName(name), replacement)
	end
	return value
end

-- str in this table is a regex pattern.  It will automatically be anchored to
-- the beginning of a line and any preceding whitespace will be skipped.  The
-- pattern should have no more than two captures patterns, which correspond to
-- the two parameters (usually 'key' and 'value') that are passed to the
-- process function.  All trailing characters will be validated.
--
-- We have two special entries in this table: the first is the first entry,
-- a full-line comment.  The second is for 'exec' handling.  Both have a single
-- capture group, but the difference is that the full-line comment pattern will
-- match the entire line.  This does not run afoul of the later end of line
-- validation that we'll do after a match.  However, the 'exec' pattern will.
-- We document the exceptions with a special 'groups' index that indicates
-- the number of capture groups, if not two.  We'll use this later to do
-- validation on the proper entry.
local pattern_table = {
	{
		str = "(#.*)",
		process = function(_, _)  end,
		groups = 1,
	},
	--  module_load="value"
	{
		str = MODULEEXPR .. "_load%s*=%s*\"([%w%s%p]-)\"",
		process = function(k, v)
			if modules[k] == nil then
				modules[k] = {}
			end
			modules[k].load = v:upper()
		end,
	},
	--  module_name="value"
	{
		str = MODULEEXPR .. "_name%s*=%s*\"([%w%s%p]-)\"",
		process = function(k, v)
			setKey(k, "name", v)
		end,
	},
	--  module_type="value"
	{
		str = MODULEEXPR .. "_type%s*=%s*\"([%w%s%p]-)\"",
		process = function(k, v)
			setKey(k, "type", v)
		end,
	},
	--  module_flags="value"
	{
		str = MODULEEXPR .. "_flags%s*=%s*\"([%w%s%p]-)\"",
		process = function(k, v)
			setKey(k, "flags", v)
		end,
	},
	--  module_before="value"
	{
		str = MODULEEXPR .. "_before%s*=%s*\"([%w%s%p]-)\"",
		process = function(k, v)
			setKey(k, "before", v)
		end,
	},
	--  module_after="value"
	{
		str = MODULEEXPR .. "_after%s*=%s*\"([%w%s%p]-)\"",
		process = function(k, v)
			setKey(k, "after", v)
		end,
	},
	--  module_error="value"
	{
		str = MODULEEXPR .. "_error%s*=%s*\"([%w%s%p]-)\"",
		process = function(k, v)
			setKey(k, "error", v)
		end,
	},
	--  exec="command"
	{
		str = "exec%s*=%s*\"([%w%s%p]-)\"",
		process = function(k, _)
			if cli_execute_unparsed(k) ~= 0 then
				print(MSG_FAILEXEC:format(k))
			end
		end,
		groups = 1,
	},
	--  env_var="value"
	{
		str = "([%w%p]+)%s*=%s*\"([%w%s%p]-)\"",
		process = function(k, v)
			if setEnv(k, processEnvVar(v)) ~= 0 then
				print(MSG_FAILSETENV:format(k, v))
			end
		end,
	},
	--  env_var=num
	{
		str = "([%w%p]+)%s*=%s*(-?%d+)",
		process = function(k, v)
			if setEnv(k, processEnvVar(v)) ~= 0 then
				print(MSG_FAILSETENV:format(k, tostring(v)))
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

local function loadModule(mod, silent)
	local status = true
	local pstatus
	for k, v in pairs(mod) do
		if v.load ~= nil and v.load:lower() == "yes" then
			local str = "load "
			if v.type ~= nil then
				str = str .. "-t " .. v.type .. " "
			end
			if v.name ~= nil then
				str = str .. v.name
			else
				str = str .. k
			end
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
				if not silent then
					print(MSG_FAILEXMOD:format(str))
				end
				if v.error ~= nil then
					cli_execute_unparsed(v.error)
				end
				status = false
			end

			if v.after ~= nil then
				pstatus = cli_execute_unparsed(v.after) == 0
				if not pstatus and not silent then
					print(MSG_FAILEXAF:format(v.after, k))
				end
				status = status and pstatus
			end

		end
	end

	return status
end

local function readConfFiles(loaded_files)
	local f = loader.getenv("loader_conf_files")
	if f ~= nil then
		for name in f:gmatch("([%w%p]+)%s*") do
			if loaded_files[name] ~= nil then
				goto continue
			end

			local prefiles = loader.getenv("loader_conf_files")

			print("Loading " .. name)
			-- These may or may not exist, and that's ok. Do a
			-- silent parse so that we complain on parse errors but
			-- not for them simply not existing.
			if not config.processFile(name, true) then
				print(MSG_FAILPARSECFG:format(name))
			end

			loaded_files[name] = true
			local newfiles = loader.getenv("loader_conf_files")
			if prefiles ~= newfiles then
				readConfFiles(loaded_files)
			end
			::continue::
		end
	end
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
	if nextboot_file == nil then
		return
	end

	local text = readFile(nextboot_file, true)
	if text == nil then
		return
	end

	if text:match("^nextboot_enable=\"NO\"") ~= nil then
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

	return config.parse(text)
end

-- silent runs will not return false if we fail to open the file
function config.parse(text)
	local n = 1
	local status = true

	for line in text:gmatch("([^\n]+)") do
		if line:match("^%s*$") == nil then
			for _, val in ipairs(pattern_table) do
				local pattern = '^%s*' .. val.str .. '%s*(.*)';
				local cgroups = val.groups or 2
				local k, v, c = line:match(pattern)
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
		local module_path = config.module_path
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

	if not config.processFile(file) then
		print(MSG_FAILPARSECFG:format(file))
	end

	local loaded_files = {file = true}
	readConfFiles(loaded_files)

	checkNextboot()

	-- Cache the provided module_path at load time for later use
	config.module_path = loader.getenv("module_path")
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
	local kernel = config.kernel_selected or config.kernel_loaded
	local loaded

	print(MSG_KERNLOADING)
	loaded = config.loadKernel(kernel)

	if not loaded then
		return
	end

	print(MSG_MODLOADING)
	if not loadModule(modules, not config.verbose) then
		print(MSG_MODLOADFAIL)
	end
end

hook.registerType("config.loaded")
hook.registerType("config.reloaded")
return config
