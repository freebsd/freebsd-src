--
-- Copyright (c) 2015 Pedro Souza <pedrosouza@freebsd.org>
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


local menu = {};

local core = require("core");
local color = require("color");
local config = require("config");
local screen = require("screen");
local drawer = require("drawer");

local OnOff;
local skip;
local run;
local autoboot;
local carousel_choices = {};

--loader menu tree:
--rooted at menu.welcome
--submenu declarations:
local boot_options;
local welcome;

menu.boot_options = {
	-- return to welcome menu
	{
		entry_type = core.MENU_RETURN,
		name = function()
			return "Back to main menu"..color.highlight(" [Backspace]");
		end
	},

	-- load defaults
	{
		entry_type = core.MENU_ENTRY,
		name = function()
			return "Load System "..color.highlight("D").."efaults";
		end,
		func = function()
			core.setDefaults()
		end,
		alias = {"d", "D"}
	},

	{
		entry_type = core.MENU_SEPARATOR,
		name = function()
			return "";
		end
	},

	{
		entry_type = core.MENU_SEPARATOR,
		name = function()
			return "Boot Options:";
		end
	},

	-- acpi
	{
		entry_type = core.MENU_ENTRY,
		name = function()
			return OnOff(color.highlight("A").."CPI       :", core.acpi);
		end,
		func = function()
			core.setACPI();
		end,
		alias = {"a", "A"}
	},
	-- safe mode
	{
		entry_type = core.MENU_ENTRY,
		name = function()
			return OnOff("Safe "..color.highlight("M").."ode  :", core.sm);
		end,
		func = function()
			core.setSafeMode();
		end,
		alias = {"m", "M"}
	},
	-- single user
	{
		entry_type = core.MENU_ENTRY,
		name = function()
			return OnOff(color.highlight("S").."ingle user:", core.su);
		end,
		func = function()
			core.setSingleUser();
		end,
		alias = {"s", "S"}
	},
	-- verbose boot
	{
		entry_type = core.MENU_ENTRY,
		name = function()
			return OnOff(color.highlight("V").."erbose    :", core.verbose);
		end,
		func = function()
			core.setVerbose();
		end,
		alias = {"v", "V"}
	},
};

menu.welcome = {
	-- boot multi user
	{
		entry_type = core.MENU_ENTRY,
		name = function()
			return color.highlight("B").."oot Multi user "..color.highlight("[Enter]");
		end,
		func = function()
			core.setSingleUser(false);
			core.boot();
		end,
		alias = {"b", "B"}
	},

	-- boot single user
	{
		entry_type = core.MENU_ENTRY,
		name = function()
			return "Boot "..color.highlight("S").."ingle user";
		end,
		func = function()
			core.setSingleUser(true);
			core.boot();
		end,
		alias = {"s", "S"}
	},

	-- escape to interpreter
	{
		entry_type = core.MENU_RETURN,
		name = function()
			return color.highlight("Esc").."ape to loader prompt";
		end,
		func = function()
			loader.setenv("autoboot_delay", "NO")
		end,
		alias = {core.KEYSTR_ESCAPE}
	},

	-- reboot
	{
		entry_type = core.MENU_ENTRY,
		name = function()
			return color.highlight("R").."eboot";
		end,
		func = function()
			loader.perform("reboot");
		end,
		alias = {"r", "R"}
	},


	{
		entry_type = core.MENU_SEPARATOR,
		name = function()
			return "";
		end
	},

	{
		entry_type = core.MENU_SEPARATOR,
		name = function()
			return "Options:";
		end
	},

	-- kernel options
	{
		entry_type = core.MENU_CAROUSEL_ENTRY,
		carousel_id = "kernel",
		items = core.kernelList,
		name = function(idx, choice, all_choices)
			if #all_choices == 0 then
				return "Kernel: ";
			end

			local kernel_name = color.escapef(color.GREEN) ..
			    choice .. color.default();
			if (idx == 1) then
				kernel_name = "default/" .. kernel_name;
			end
			return color.highlight("K").."ernel: " .. kernel_name ..
			    " (" .. idx ..
			    " of " .. #all_choices .. ")";
		end,
		func = function(choice)
			config.reload(choice);
		end,
		alias = {"k", "K"}
	},

	-- boot options
	{
		entry_type = core.MENU_SUBMENU,
		name = function()
			return "Boot "..color.highlight("O").."ptions";
		end,
		submenu = function()
			return menu.boot_options;
		end,
		alias = {"o", "O"}
	}

};

-- The first item in every carousel is always the default item.
function menu.getCarouselIndex(id)
	local val = carousel_choices[id];
	if (val == nil) then
		return 1;
	end
	return val;
end

function menu.setCarouselIndex(id, idx)
	carousel_choices[id] = idx;
end

function menu.run(m)

	if (menu.skip()) then
		core.autoboot();
		return false;
	end

	if (m == nil) then
		m = menu.welcome;
	end

	-- redraw screen
	screen.clear();
	screen.defcursor();
	local alias_table = drawer.drawscreen(m);

--	menu.autoboot();

	cont = true;
	while cont do
		local key = io.getchar();

		-- Special key behaviors
		if ((key == core.KEY_BACKSPACE) or (key == core.KEY_DELETE)) and
		    (m ~= menu.welcome) then
			break
		elseif (key == core.KEY_ENTER) then
			core.boot();
			-- Should not return
		end

		key = string.char(key)
		-- check to see if key is an alias
		local sel_entry = nil;
		for k, v in pairs(alias_table) do
			if (key == k) then
				sel_entry = v;
			end
		end

		-- if we have an alias do the assigned action:
		if(sel_entry ~= nil) then
			if (sel_entry.entry_type == core.MENU_ENTRY) then
				-- run function
				sel_entry.func();
			elseif (sel_entry.entry_type == core.MENU_CAROUSEL_ENTRY) then
				-- carousel (rotating) functionality
				local carid = sel_entry.carousel_id;
				local caridx = menu.getCarouselIndex(carid);
				local choices = sel_entry.items();

				caridx = (caridx % #choices) + 1;
				menu.setCarouselIndex(carid, caridx);
				sel_entry.func(choices[caridx]);
			elseif (sel_entry.entry_type == core.MENU_SUBMENU) then
				-- recurse
				cont = menu.run(sel_entry.submenu());
			elseif (sel_entry.entry_type == core.MENU_RETURN) then
				-- allow entry to have a function/side effect
				if (sel_entry.func ~= nil) then
					sel_entry.func();
				end
				-- break recurse
				cont = false;
			end
			-- if we got an alias key the screen is out of date:
			screen.clear();
			screen.defcursor();
			alias_table = drawer.drawscreen(m);
		end
	end

	if (m == menu.welcome) then
		screen.defcursor();
		print("Exiting menu!");
		return false;
	end

	return true;
end

function menu.skip()
	if core.bootserial() then
		return true;
	end
	local c = string.lower(loader.getenv("console") or "");
	if (c:match("^efi[ ;]") or c:match("[ ;]efi[ ;]")) ~= nil then
		return true;
	end

	c = string.lower(loader.getenv("beastie_disable") or "");
	print("beastie_disable", c);
	return c == "yes";
end

function menu.autoboot()
	if menu.already_autoboot == true then
		return;
	end
	menu.already_autoboot = true;

	local ab = loader.getenv("autoboot_delay");
	if ab == "NO" or ab == "no" then
		core.boot();
	end
	ab = tonumber(ab) or 10;

	local x = loader.getenv("loader_menu_timeout_x") or 5;
	local y = loader.getenv("loader_menu_timeout_y") or 22;

	local endtime = loader.time() + ab;
	local time;

	repeat
		time = endtime - loader.time();
		screen.setcursor(x, y);
		print("Autoboot in "..time.." seconds, hit [Enter] to boot"
			      .." or any other key to stop     ");
		screen.defcursor();
		if io.ischar() then
			local ch = io.getchar();
			if ch == core.KEY_ENTER then
				break;
			else
				-- prevent autoboot when escaping to interpreter
				loader.setenv("autoboot_delay", "NO");
				-- erase autoboot msg
				screen.setcursor(0, y);
				print("                                        "
					      .."                                        ");
				screen.defcursor();
				return;
			end
		end

		loader.delay(50000);
	until time <= 0
	core.boot();

end

function OnOff(str, b)
	if (b) then
		return str .. color.escapef(color.GREEN).."On"..color.escapef(color.WHITE);
	else
		return str .. color.escapef(color.RED).."off"..color.escapef(color.WHITE);
	end
end

return menu
