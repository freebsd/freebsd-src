--
-- SPDX-License-Identifier: BSD-2-Clause-FreeBSD
--
-- Copyright (c) 2018 Kyle Evans <kevans@FreeBSD.org>
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

local config = require("config")
local core = require("core")

local cli = {}

if not pager then
	-- shim for the pager module that just doesn't do it.
	-- XXX Remove after 12.2 goes EoL.
	pager = {
		open = function() end,
		close = function() end,
		output = function(str)
			printc(str)
		end,
	}
end

-- Internal function
-- Parses arguments to boot and returns two values: kernel_name, argstr
-- Defaults to nil and "" respectively.
-- This will also parse arguments to autoboot, but the with_kernel argument
-- will need to be explicitly overwritten to false
local function parseBootArgs(argv, with_kernel)
	if with_kernel == nil then
		with_kernel = true
	end
	if #argv == 0 then
		if with_kernel then
			return nil, ""
		else
			return ""
		end
	end
	local kernel_name
	local argstr = ""

	for _, v in ipairs(argv) do
		if with_kernel and v:sub(1,1) ~= "-" then
			kernel_name = v
		else
			argstr = argstr .. " " .. v
		end
	end
	if with_kernel then
		return kernel_name, argstr
	else
		return argstr
	end
end

local function setModule(module, loading)
	if loading and config.enableModule(module) then
		print(module .. " will be loaded")
	elseif not loading and config.disableModule(module) then
		print(module .. " will not be loaded")
	end
end

-- Declares a global function cli_execute that attempts to dispatch the
-- arguments passed as a lua function. This gives lua a chance to intercept
-- builtin CLI commands like "boot"
-- This function intentionally does not follow our general naming guideline for
-- functions. This is global pollution, but the clearly separated 'cli' looks
-- more like a module indicator to serve as a hint of where to look for the
-- corresponding definition.
function cli_execute(...)
	local argv = {...}
	-- Just in case...
	if #argv == 0 then
		return loader.command(...)
	end

	local cmd_name = argv[1]
	local cmd = cli[cmd_name]
	if cmd ~= nil and type(cmd) == "function" then
		-- Pass argv wholesale into cmd. We could omit argv[0] since the
		-- traditional reasons for including it don't necessarily apply,
		-- it may not be totally redundant if we want to have one global
		-- handling multiple commands
		return cmd(...)
	else
		return loader.command(...)
	end

end

function cli_execute_unparsed(str)
	return cli_execute(loader.parse(str))
end

-- Module exports

function cli.boot(...)
	local _, argv = cli.arguments(...)
	local kernel, argstr = parseBootArgs(argv)
	if kernel ~= nil then
		loader.perform("unload")
		config.selectKernel(kernel)
	end
	core.boot(argstr)
end

function cli.autoboot(...)
	local _, argv = cli.arguments(...)
	local argstr = parseBootArgs(argv, false)
	core.autoboot(argstr)
end

cli['boot-conf'] = function(...)
	local _, argv = cli.arguments(...)
	local kernel, argstr = parseBootArgs(argv)
	if kernel ~= nil then
		loader.perform("unload")
		config.selectKernel(kernel)
	end
	core.autoboot(argstr)
end

cli['read-conf'] = function(...)
	local _, argv = cli.arguments(...)
	config.readConf(assert(core.popFrontTable(argv)))
end

cli['reload-conf'] = function()
	config.reload()
end

cli["enable-module"] = function(...)
	local _, argv = cli.arguments(...)
	if #argv == 0 then
		print("usage error: enable-module module")
		return
	end

	setModule(argv[1], true)
end

cli["disable-module"] = function(...)
	local _, argv = cli.arguments(...)
	if #argv == 0 then
		print("usage error: disable-module module")
		return
	end

	setModule(argv[1], false)
end

cli["toggle-module"] = function(...)
	local _, argv = cli.arguments(...)
	if #argv == 0 then
		print("usage error: toggle-module module")
		return
	end

	local module = argv[1]
	setModule(module, not config.isModuleEnabled(module))
end

cli["show-module-options"] = function()
	local module_info = config.getModuleInfo()
	local modules = module_info['modules']
	local blacklist = module_info['blacklist']
	local lines = {}

	for module, info in pairs(modules) do
		if #lines > 0 then
			lines[#lines + 1] = ""
		end

		lines[#lines + 1] = "Name:        " .. module
		if info.name then
			lines[#lines + 1] = "Path:        " .. info.name
		end

		if info.type then
			lines[#lines + 1] = "Type:        " .. info.type
		end

		if info.flags then
			lines[#lines + 1] = "Flags:       " .. info.flags
		end

		if info.before then
			lines[#lines + 1] = "Before load: " .. info.before
		end

		if info.after then
			lines[#lines + 1] = "After load:  " .. info.after
		end

		if info.error then
			lines[#lines + 1] = "Error:       " .. info.error
		end

		local status
		if blacklist[module] and not info.force then
			status = "Blacklisted"
		elseif info.load == "YES" then
			status = "Load"
		else
			status = "Don't load"
		end

		lines[#lines + 1] = "Status:      " .. status
	end

	pager.open()
	for _, v in ipairs(lines) do
		pager.output(v .. "\n")
	end
	pager.close()
end

cli["disable-device"] = function(...)
	local _, argv = cli.arguments(...)
	local d, u

	if #argv == 0 then
		print("usage error: disable-device device")
		return
	end

	d, u = string.match(argv[1], "(%w*%a)(%d+)")
	if d ~= nil then
		loader.setenv("hint." .. d .. "." .. u .. ".disabled", "1")
	end
end

-- Used for splitting cli varargs into cmd_name and the rest of argv
function cli.arguments(...)
	local argv = {...}
	local cmd_name
	cmd_name, argv = core.popFrontTable(argv)
	return cmd_name, argv
end

return cli
