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

local config = require("config")
local hook = require("hook")

local core = {}

local default_acpi = false
local default_safe_mode = false
local default_single_user = false
local default_verbose = false

local bootenv_list = "bootenvs"

local function composeLoaderCmd(cmd_name, argstr)
	if argstr ~= nil then
		cmd_name = cmd_name .. " " .. argstr
	end
	return cmd_name
end

local function recordDefaults()
	local boot_single = loader.getenv("boot_single") or "no"
	local boot_verbose = loader.getenv("boot_verbose") or "no"

	default_acpi = core.getACPI()
	default_single_user = boot_single:lower() ~= "no"
	default_verbose = boot_verbose:lower() ~= "no"

	core.setACPI(default_acpi)
	core.setSingleUser(default_single_user)
	core.setVerbose(default_verbose)
end


-- Globals
-- try_include will return the loaded module on success, or false and the error
-- message on failure.
function try_include(module)
	if module:sub(1, 1) ~= "/" then
		local lua_path = loader.lua_path
		-- XXX Temporary compat shim; this should be removed once the
		-- loader.lua_path export has sufficiently spread.
		if lua_path == nil then
			lua_path = "/boot/lua"
		end
		module = lua_path .. "/" .. module
		-- We only attempt to append an extension if an absolute path
		-- wasn't specified.  This assumes that the caller either wants
		-- to treat this like it would require() and specify just the
		-- base filename, or they know what they're doing as they've
		-- specified an absolute path and we shouldn't impede.
		if module:match(".lua$") == nil then
			module = module .. ".lua"
		end
	end
	if lfs.attributes(module, "mode") ~= "file" then
		return
	end

	return dofile(module)
end

-- Module exports
-- Commonly appearing constants
core.KEY_BACKSPACE	= 8
core.KEY_ENTER		= 13
core.KEY_DELETE		= 127

-- Note that this is a decimal representation, despite the leading 0 that in
-- other contexts (outside of Lua) may mean 'octal'
core.KEYSTR_ESCAPE	= "\027"
core.KEYSTR_CSI		= core.KEYSTR_ESCAPE .. "["
core.KEYSTR_RESET	= core.KEYSTR_ESCAPE .. "c"

core.MENU_RETURN	= "return"
core.MENU_ENTRY		= "entry"
core.MENU_SEPARATOR	= "separator"
core.MENU_SUBMENU	= "submenu"
core.MENU_CAROUSEL_ENTRY	= "carousel_entry"

function core.setVerbose(verbose)
	if verbose == nil then
		verbose = not core.verbose
	end

	if verbose then
		loader.setenv("boot_verbose", "YES")
	else
		loader.unsetenv("boot_verbose")
	end
	core.verbose = verbose
end

function core.setSingleUser(single_user)
	if single_user == nil then
		single_user = not core.su
	end

	if single_user then
		loader.setenv("boot_single", "YES")
	else
		loader.unsetenv("boot_single")
	end
	core.su = single_user
end

function core.hasACPI()
	return loader.getenv("acpi.rsdp") ~= nil
end

function core.isX86()
	return loader.machine_arch == "i386" or loader.machine_arch == "amd64"
end

function core.getACPI()
	if not core.hasACPI() then
		-- x86 requires ACPI pretty much
		return false or core.isX86()
	end

	-- Otherwise, respect disabled if it's set
	local c = loader.getenv("hint.acpi.0.disabled")
	return c == nil or tonumber(c) ~= 1
end

function core.setACPI(acpi)
	if acpi == nil then
		acpi = not core.acpi
	end

	if acpi then
		loader.setenv("acpi_load", "YES")
		loader.setenv("hint.acpi.0.disabled", "0")
		loader.unsetenv("loader.acpi_disabled_by_user")
	else
		loader.unsetenv("acpi_load")
		loader.setenv("hint.acpi.0.disabled", "1")
		loader.setenv("loader.acpi_disabled_by_user", "1")
	end
	core.acpi = acpi
end

function core.setSafeMode(safe_mode)
	if safe_mode == nil then
		safe_mode = not core.sm
	end
	if safe_mode then
		loader.setenv("kern.smp.disabled", "1")
		loader.setenv("hw.ata.ata_dma", "0")
		loader.setenv("hw.ata.atapi_dma", "0")
		loader.setenv("hw.ata.wc", "0")
		loader.setenv("hw.eisa_slots", "0")
		loader.setenv("kern.eventtimer.periodic", "1")
		loader.setenv("kern.geom.part.check_integrity", "0")
	else
		loader.unsetenv("kern.smp.disabled")
		loader.unsetenv("hw.ata.ata_dma")
		loader.unsetenv("hw.ata.atapi_dma")
		loader.unsetenv("hw.ata.wc")
		loader.unsetenv("hw.eisa_slots")
		loader.unsetenv("kern.eventtimer.periodic")
		loader.unsetenv("kern.geom.part.check_integrity")
	end
	core.sm = safe_mode
end

function core.clearCachedKernels()
	-- Clear the kernel cache on config changes, autodetect might have
	-- changed or if we've switched boot environments then we could have
	-- a new kernel set.
	core.cached_kernels = nil
end

function core.kernelList()
	if core.cached_kernels ~= nil then
		return core.cached_kernels
	end

	local default_kernel = loader.getenv("kernel")
	local v = loader.getenv("kernels")
	local autodetect = loader.getenv("kernels_autodetect") or ""

	local kernels = {}
	local unique = {}
	local i = 0

	if default_kernel then
		i = i + 1
		kernels[i] = default_kernel
		unique[default_kernel] = true
	end

	if v ~= nil then
		for n in v:gmatch("([^;, ]+)[;, ]?") do
			if unique[n] == nil then
				i = i + 1
				kernels[i] = n
				unique[n] = true
			end
		end
	end

	-- Do not attempt to autodetect if underlying filesystem
	-- do not support directory listing (e.g. tftp, http)
	if not lfs.attributes("/boot", "mode") then
		autodetect = "no"
		loader.setenv("kernels_autodetect", "NO")
	end

	-- Base whether we autodetect kernels or not on a loader.conf(5)
	-- setting, kernels_autodetect. If it's set to 'yes', we'll add
	-- any kernels we detect based on the criteria described.
	if autodetect:lower() ~= "yes" then
		core.cached_kernels = kernels
		return core.cached_kernels
	end

	local present = {}

	-- Automatically detect other bootable kernel directories using a
	-- heuristic.  Any directory in /boot that contains an ordinary file
	-- named "kernel" is considered eligible.
	for file, ftype in lfs.dir("/boot") do
		local fname = "/boot/" .. file

		if file == "." or file == ".." then
			goto continue
		end

		if ftype then
			if ftype ~= lfs.DT_DIR then
				goto continue
			end
		elseif lfs.attributes(fname, "mode") ~= "directory" then
			goto continue
		end

		if lfs.attributes(fname .. "/kernel", "mode") ~= "file" then
			goto continue
		end

		if unique[file] == nil then
			i = i + 1
			kernels[i] = file
			unique[file] = true
		end

		present[file] = true

		::continue::
	end

	-- If we found more than one kernel, prune the "kernel" specified kernel
	-- off of the list if it wasn't found during traversal.  If we didn't
	-- actually find any kernels, we just assume that they know what they're
	-- doing and leave it alone.
	if default_kernel and not present[default_kernel] and #kernels > 1 then
		for i = 1, #kernels do
			if i == #kernels then
				kernels[i] = nil
			else
				kernels[i] = kernels[i + 1]
			end
		end
	end

	core.cached_kernels = kernels
	return core.cached_kernels
end

function core.bootenvDefault()
	return loader.getenv("zfs_be_active")
end

function core.bootenvList()
	local bootenv_count = tonumber(loader.getenv(bootenv_list .. "_count"))
	local bootenvs = {}
	local curenv
	local envcount = 0
	local unique = {}

	if bootenv_count == nil or bootenv_count <= 0 then
		return bootenvs
	end

	-- Currently selected bootenv is always first/default
	-- On the rewinded list the bootenv may not exists
	if core.isRewinded() then
		curenv = core.bootenvDefaultRewinded()
	else
		curenv = core.bootenvDefault()
	end
	if curenv ~= nil then
		envcount = envcount + 1
		bootenvs[envcount] = curenv
		unique[curenv] = true
	end

	for curenv_idx = 0, bootenv_count - 1 do
		curenv = loader.getenv(bootenv_list .. "[" .. curenv_idx .. "]")
		if curenv ~= nil and unique[curenv] == nil then
			envcount = envcount + 1
			bootenvs[envcount] = curenv
			unique[curenv] = true
		end
	end
	return bootenvs
end

function core.isCheckpointed()
	return loader.getenv("zpool_checkpoint") ~= nil
end

function core.bootenvDefaultRewinded()
	local defname =  "zfs:!" .. string.sub(core.bootenvDefault(), 5)
	local bootenv_count = tonumber("bootenvs_check_count")

	if bootenv_count == nil or bootenv_count <= 0 then
		return defname
	end

	for curenv_idx = 0, bootenv_count - 1 do
		local curenv = loader.getenv("bootenvs_check[" .. curenv_idx .. "]")
		if curenv == defname then
			return defname
		end
	end

	return loader.getenv("bootenvs_check[0]")
end

function core.isRewinded()
	return bootenv_list == "bootenvs_check"
end

function core.changeRewindCheckpoint()
	if core.isRewinded() then
		bootenv_list = "bootenvs"
	else
		bootenv_list = "bootenvs_check"
	end
end

function core.loadEntropy()
	if core.isUEFIBoot() then
		if (loader.getenv("entropy_efi_seed") or "no"):lower() == "yes" then
			loader.perform("efi-seed-entropy")
		end
	end
end

function core.setDefaults()
	core.setACPI(default_acpi)
	core.setSafeMode(default_safe_mode)
	core.setSingleUser(default_single_user)
	core.setVerbose(default_verbose)
end

function core.autoboot(argstr)
	-- loadelf() only if we've not already loaded a kernel
	if loader.getenv("kernelname") == nil then
		config.loadelf()
	end
	core.loadEntropy()
	loader.perform(composeLoaderCmd("autoboot", argstr))
end

function core.boot(argstr)
	-- loadelf() only if we've not already loaded a kernel
	if loader.getenv("kernelname") == nil then
		config.loadelf()
	end
	core.loadEntropy()
	loader.perform(composeLoaderCmd("boot", argstr))
end

function core.hasFeature(name)
	if not loader.has_feature then
		-- Loader too old, no feature support
		return nil, "No feature support in loaded loader"
	end

	return loader.has_feature(name)
end

function core.isSingleUserBoot()
	local single_user = loader.getenv("boot_single")
	return single_user ~= nil and single_user:lower() == "yes"
end

function core.isUEFIBoot()
	local efiver = loader.getenv("efi-version")

	return efiver ~= nil
end

function core.isZFSBoot()
	local c = loader.getenv("currdev")

	if c ~= nil then
		return c:match("^zfs:") ~= nil
	end
	return false
end

function core.isFramebufferConsole()
	local c = loader.getenv("console")
	if c ~= nil then
		if c:find("efi") == nil and c:find("vidconsole") == nil then
			return false
		end
		if loader.getenv("screen.depth") ~= nil then
			return true
		end
	end
	return false
end

function core.isSerialConsole()
	local c = loader.getenv("console")
	if c ~= nil then
		-- serial console is comconsole, but also userboot.
		-- userboot is there, because we have no way to know
		-- if the user terminal can draw unicode box chars or not.
		if c:find("comconsole") ~= nil or c:find("userboot") ~= nil then
			return true
		end
	end
	return false
end

function core.isSerialBoot()
	local s = loader.getenv("boot_serial")
	if s ~= nil then
		return true
	end

	local m = loader.getenv("boot_multicons")
	if m ~= nil then
		return true
	end
	return false
end

-- Is the menu skipped in the environment in which we've booted?
function core.isMenuSkipped()
	return string.lower(loader.getenv("beastie_disable") or "") == "yes"
end

-- This may be a better candidate for a 'utility' module.
function core.deepCopyTable(tbl)
	local new_tbl = {}
	for k, v in pairs(tbl) do
		if type(v) == "table" then
			new_tbl[k] = core.deepCopyTable(v)
		else
			new_tbl[k] = v
		end
	end
	return new_tbl
end

-- XXX This should go away if we get the table lib into shape for importing.
-- As of now, it requires some 'os' functions, so we'll implement this in lua
-- for our uses
function core.popFrontTable(tbl)
	-- Shouldn't reasonably happen
	if #tbl == 0 then
		return nil, nil
	elseif #tbl == 1 then
		return tbl[1], {}
	end

	local first_value = tbl[1]
	local new_tbl = {}
	-- This is not a cheap operation
	for k, v in ipairs(tbl) do
		if k > 1 then
			new_tbl[k - 1] = v
		end
	end

	return first_value, new_tbl
end

function core.getConsoleName()
	if loader.getenv("boot_multicons") ~= nil then
		if loader.getenv("boot_serial") ~= nil then
			return "Dual (Serial primary)"
		else
			return "Dual (Video primary)"
		end
	else
		if loader.getenv("boot_serial") ~= nil then
			return "Serial"
		else
			return "Video"
		end
	end
end

function core.nextConsoleChoice()
	if loader.getenv("boot_multicons") ~= nil then
		if loader.getenv("boot_serial") ~= nil then
			loader.unsetenv("boot_serial")
		else
			loader.unsetenv("boot_multicons")
			loader.setenv("boot_serial", "YES")
		end
	else
		if loader.getenv("boot_serial") ~= nil then
			loader.unsetenv("boot_serial")
		else
			loader.setenv("boot_multicons", "YES")
			loader.setenv("boot_serial", "YES")
		end
	end
end

recordDefaults()
hook.register("config.reloaded", core.clearCachedKernels)
return core
