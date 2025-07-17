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

local cli = require("cli")
local core = require("core")
local color = require("color")
local config = require("config")
local screen = require("screen")
local drawer = require("drawer")

local menu = {}

local drawn_menu
local return_menu_entry = {
	entry_type = core.MENU_RETURN,
	name = "Back to main menu" .. color.highlight(" [Backspace]"),
}

local function OnOff(str, value)
	if value then
		return str .. color.escapefg(color.GREEN) .. "On" ..
		    color.resetfg()
	else
		return str .. color.escapefg(color.RED) .. "off" ..
		    color.resetfg()
	end
end

local function bootenvSet(env)
	loader.setenv("vfs.root.mountfrom", env)
	loader.setenv("currdev", env .. ":")
	config.reload()
	if loader.getenv("kernelname") ~= nil then
		loader.perform("unload")
	end
end

local function multiUserPrompt()
	return loader.getenv("loader_menu_multi_user_prompt") or "Multi user"
end

-- Module exports
menu.handlers = {
	-- Menu handlers take the current menu and selected entry as parameters,
	-- and should return a boolean indicating whether execution should
	-- continue or not. The return value may be omitted if this entry should
	-- have no bearing on whether we continue or not, indicating that we
	-- should just continue after execution.
	[core.MENU_ENTRY] = function(_, entry)
		-- run function
		entry.func()
	end,
	[core.MENU_CAROUSEL_ENTRY] = function(_, entry)
		-- carousel (rotating) functionality
		local carid = entry.carousel_id
		local caridx = config.getCarouselIndex(carid)
		local choices = entry.items
		if type(choices) == "function" then
			choices = choices()
		end
		if #choices > 0 then
			caridx = (caridx % #choices) + 1
			config.setCarouselIndex(carid, caridx)
			entry.func(caridx, choices[caridx], choices)
		end
	end,
	[core.MENU_SUBMENU] = function(_, entry)
		menu.process(entry.submenu)
	end,
	[core.MENU_RETURN] = function(_, entry)
		-- allow entry to have a function/side effect
		if entry.func ~= nil then
			entry.func()
		end
		return false
	end,
}
-- loader menu tree is rooted at menu.welcome

menu.boot_environments = {
	entries = {
		-- return to welcome menu
		return_menu_entry,
		{
			entry_type = core.MENU_CAROUSEL_ENTRY,
			carousel_id = "be_active",
			items = core.bootenvList,
			name = function(idx, choice, all_choices)
				if #all_choices == 0 then
					return "Active: "
				end

				local is_default = (idx == 1)
				local bootenv_name = ""
				local name_color
				if is_default then
					name_color = color.escapefg(color.GREEN)
				else
					name_color = color.escapefg(color.CYAN)
				end
				bootenv_name = bootenv_name .. name_color ..
				    choice .. color.resetfg()
				return color.highlight("A").."ctive: " ..
				    bootenv_name .. " (" .. idx .. " of " ..
				    #all_choices .. ")"
			end,
			func = function(_, choice, _)
				bootenvSet(choice)
			end,
			alias = {"a", "A"},
		},
		{
			entry_type = core.MENU_ENTRY,
			visible = function()
				return core.isRewinded() == false
			end,
			name = function()
				return color.highlight("b") .. "ootfs: " ..
				    core.bootenvDefault()
			end,
			func = function()
				-- Reset active boot environment to the default
				config.setCarouselIndex("be_active", 1)
				bootenvSet(core.bootenvDefault())
			end,
			alias = {"b", "B"},
		},
	},
}

menu.boot_options = {
	entries = {
		-- return to welcome menu
		return_menu_entry,
		-- load defaults
		{
			entry_type = core.MENU_ENTRY,
			name = "Load System " .. color.highlight("D") ..
			    "efaults",
			func = core.setDefaults,
			alias = {"d", "D"},
		},
		{
			entry_type = core.MENU_SEPARATOR,
		},
		{
			entry_type = core.MENU_SEPARATOR,
			name = "Boot Options:",
		},
		-- acpi
		{
			entry_type = core.MENU_ENTRY,
			visible = core.hasACPI,
			name = function()
				return OnOff(color.highlight("A") ..
				    "CPI       :", core.acpi)
			end,
			func = core.setACPI,
			alias = {"a", "A"},
		},
		-- safe mode
		{
			entry_type = core.MENU_ENTRY,
			name = function()
				return OnOff("Safe " .. color.highlight("M") ..
				    "ode  :", core.sm)
			end,
			func = core.setSafeMode,
			alias = {"m", "M"},
		},
		-- single user
		{
			entry_type = core.MENU_ENTRY,
			name = function()
				return OnOff(color.highlight("S") ..
				    "ingle user:", core.su)
			end,
			func = core.setSingleUser,
			alias = {"s", "S"},
		},
		-- verbose boot
		{
			entry_type = core.MENU_ENTRY,
			name = function()
				return OnOff(color.highlight("V") ..
				    "erbose    :", core.verbose)
			end,
			func = core.setVerbose,
			alias = {"v", "V"},
		},
	},
}

menu.welcome = {
	entries = function()
		local menu_entries = menu.welcome.all_entries
		local multi_user = menu_entries.multi_user
		local single_user = menu_entries.single_user
		local boot_entry_1, boot_entry_2
		if core.isSingleUserBoot() then
			-- Swap the first two menu items on single user boot.
			-- We'll cache the alternate entries for performance.
			local alts = menu_entries.alts
			if alts == nil then
				single_user = core.deepCopyTable(single_user)
				multi_user = core.deepCopyTable(multi_user)
				single_user.name = single_user.alternate_name
				multi_user.name = multi_user.alternate_name
				menu_entries.alts = {
					single_user = single_user,
					multi_user = multi_user,
				}
			else
				single_user = alts.single_user
				multi_user = alts.multi_user
			end
			boot_entry_1, boot_entry_2 = single_user, multi_user
		else
			boot_entry_1, boot_entry_2 = multi_user, single_user
		end
		return {
			boot_entry_1,
			boot_entry_2,
			menu_entries.prompt,
			menu_entries.reboot,
			menu_entries.console,
			{
				entry_type = core.MENU_SEPARATOR,
			},
			{
				entry_type = core.MENU_SEPARATOR,
				name = "Kernel:",
			},
			menu_entries.kernel_options,
			{
				entry_type = core.MENU_SEPARATOR,
			},
			{
				entry_type = core.MENU_SEPARATOR,
				name = "Options:",
			},
			menu_entries.boot_options,
			menu_entries.zpool_checkpoints,
			menu_entries.boot_envs,
			menu_entries.chainload,
			menu_entries.vendor,
			{
				entry_type = core.MENU_SEPARATOR,
			},
			menu_entries.loader_needs_upgrade,
		}
	end,
	all_entries = {
		multi_user = {
			entry_type = core.MENU_ENTRY,
			name = function()
				return color.highlight("B") .. "oot " ..
				    multiUserPrompt() .. " " ..
				    color.highlight("[Enter]")
			end,
			-- Not a standard menu entry function!
			alternate_name = function()
				return color.highlight("B") .. "oot " ..
				    multiUserPrompt()
			end,
			func = function()
				core.setSingleUser(false)
				core.boot()
			end,
			alias = {"b", "B"},
		},
		single_user = {
			entry_type = core.MENU_ENTRY,
			name = "Boot " .. color.highlight("S") .. "ingle user",
			-- Not a standard menu entry function!
			alternate_name = "Boot " .. color.highlight("S") ..
			    "ingle user " .. color.highlight("[Enter]"),
			func = function()
				core.setSingleUser(true)
				core.boot()
			end,
			alias = {"s", "S"},
		},
		console = {
			entry_type = core.MENU_ENTRY,
			name = function()
				return color.highlight("C") .. "ons: " .. core.getConsoleName()
			end,
			func = function()
				core.nextConsoleChoice()
			end,
			alias = {"c", "C"},
		},
		prompt = {
			entry_type = core.MENU_RETURN,
			name = color.highlight("Esc") .. "ape to loader prompt",
			func = function()
				loader.setenv("autoboot_delay", "NO")
			end,
			alias = {core.KEYSTR_ESCAPE},
		},
		reboot = {
			entry_type = core.MENU_ENTRY,
			name = color.highlight("R") .. "eboot",
			func = function()
				loader.perform("reboot")
			end,
			alias = {"r", "R"},
		},
		kernel_options = {
			entry_type = core.MENU_CAROUSEL_ENTRY,
			carousel_id = "kernel",
			items = core.kernelList,
			name = function(idx, choice, all_choices)
				if #all_choices == 0 then
					return ""
				end

				local kernel_name
				local name_color
				if idx == 1 then
					name_color = color.escapefg(color.GREEN)
				else
					name_color = color.escapefg(color.CYAN)
				end
				kernel_name = name_color .. choice ..
				    color.resetfg()
				return kernel_name .. " (" .. idx .. " of " ..
				    #all_choices .. ")"
			end,
			func = function(_, choice, _)
				if loader.getenv("kernelname") ~= nil then
					loader.perform("unload")
				end
				config.selectKernel(choice)
			end,
			alias = {"k", "K"},
		},
		boot_options = {
			entry_type = core.MENU_SUBMENU,
			name = "Boot " .. color.highlight("O") .. "ptions",
			submenu = menu.boot_options,
			alias = {"o", "O"},
		},
		zpool_checkpoints = {
			entry_type = core.MENU_ENTRY,
			name = function()
				local rewind = "No"
				if core.isRewinded() then
					rewind = "Yes"
				end
				return "Rewind ZFS " .. color.highlight("C") ..
					"heckpoint: " .. rewind
			end,
			func = function()
				core.changeRewindCheckpoint()
				if core.isRewinded() then
					bootenvSet(
					    core.bootenvDefaultRewinded())
				else
					bootenvSet(core.bootenvDefault())
				end
				config.setCarouselIndex("be_active", 1)
			end,
			visible = function()
				return core.isZFSBoot() and
				    core.isCheckpointed()
			end,
			alias = {"c", "C"},
		},
		boot_envs = {
			entry_type = core.MENU_SUBMENU,
			visible = function()
				return core.isZFSBoot() and
				    #core.bootenvList() > 1
			end,
			name = "Boot " .. color.highlight("E") .. "nvironments",
			submenu = menu.boot_environments,
			alias = {"e", "E"},
		},
		chainload = {
			entry_type = core.MENU_ENTRY,
			name = function()
				return 'Chain' .. color.highlight("L") ..
				    "oad " .. loader.getenv('chain_disk')
			end,
			func = function()
				loader.perform("chain " ..
				    loader.getenv('chain_disk'))
			end,
			visible = function()
				return loader.getenv('chain_disk') ~= nil
			end,
			alias = {"l", "L"},
		},
		loader_needs_upgrade = {
			entry_type = core.MENU_SEPARATOR,
			name = function()
				return color.highlight("Loader needs to be updated")
			end,
			visible = function()
				return core.loaderTooOld()
			end
		},
		vendor = {
			entry_type = core.MENU_ENTRY,
			visible = function()
				return false
			end
		},
	},
}

menu.default = menu.welcome
-- current_alias_table will be used to keep our alias table consistent across
-- screen redraws, instead of relying on whatever triggered the redraw to update
-- the local alias_table in menu.process.
menu.current_alias_table = {}

function menu.draw(menudef)
	-- Clear the screen, reset the cursor, then draw
	screen.clear()
	menu.current_alias_table = drawer.drawscreen(menudef)
	drawn_menu = menudef
	screen.defcursor()
end

-- 'keypress' allows the caller to indicate that a key has been pressed that we
-- should process as our initial input.
function menu.process(menudef, keypress)
	assert(menudef ~= nil)

	if drawn_menu ~= menudef then
		menu.draw(menudef)
	end

	while true do
		local key = keypress or io.getchar()
		keypress = nil

		-- Special key behaviors
		if (key == core.KEY_BACKSPACE or key == core.KEY_DELETE) and
		    menudef ~= menu.default then
			break
		elseif key == core.KEY_ENTER then
			core.boot()
			-- Should not return.  If it does, escape menu handling
			-- and drop to loader prompt.
			return false
		end

		key = string.char(key)
		-- check to see if key is an alias
		local sel_entry = nil
		for k, v in pairs(menu.current_alias_table) do
			if key == k then
				sel_entry = v
				break
			end
		end

		-- if we have an alias do the assigned action:
		if sel_entry ~= nil then
			local handler = menu.handlers[sel_entry.entry_type]
			assert(handler ~= nil)
			-- The handler's return value indicates if we
			-- need to exit this menu.  An omitted or true
			-- return value means to continue.
			if handler(menudef, sel_entry) == false then
				return
			end
			-- If we got an alias key the screen is out of date...
			-- redraw it.
			menu.draw(menudef)
		end
	end
end

function menu.run()
	local autoboot_key
	local delay = loader.getenv("autoboot_delay")

	if delay ~= nil and delay:lower() == "no" then
		delay = nil
	else
		delay = tonumber(delay) or 10
	end

	if delay == -1 then
		core.boot()
		return
	end

	menu.draw(menu.default)

	if delay ~= nil then
		autoboot_key = menu.autoboot(delay)

		-- autoboot_key should return the key pressed.  It will only
		-- return nil if we hit the timeout and executed the timeout
		-- command.  Bail out.
		if autoboot_key == nil then
			return
		end
	end

	menu.process(menu.default, autoboot_key)
	drawn_menu = nil

	screen.defcursor()
	-- We explicitly want the newline print adds
	print("Exiting menu!")
end

function menu.autoboot(delay)
	local x = loader.getenv("loader_menu_timeout_x") or 4
	local y = loader.getenv("loader_menu_timeout_y") or 24
	local autoboot_show = loader.getenv("loader_autoboot_show") or "yes"
	local endtime = loader.time() + delay
	local time
	local last
	repeat
		time = endtime - loader.time()
		if last == nil or last ~= time then
			last = time
			if autoboot_show == "yes" then
			   screen.setcursor(x, y)
			   printc("Autoboot in " .. time ..
				  " seconds. [Space] to pause ")
			   screen.defcursor()
			end
		end
		if io.ischar() then
			local ch = io.getchar()
			if ch == core.KEY_ENTER then
				break
			else
				-- Erase autoboot msg.  While real VT100s
				-- wouldn't scroll when receiving a char with
				-- the cursor at (79, 24), bad emulators do.
				-- Avoid the issue by stopping at 79.
				screen.setcursor(1, y)
				printc(string.rep(" ", 79))
				screen.defcursor()
				return ch
			end
		end

		loader.delay(50000)
	until time <= 0

	local cmd = loader.getenv("menu_timeout_command") or "boot"
	cli_execute_unparsed(cmd)
	return nil
end

-- CLI commands
function cli.menu()
	menu.run()
end

return menu
