--
-- SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
-- $FreeBSD$
--

local color = require("color")
local config = require("config")
local core = require("core")
local screen = require("screen")

local drawer = {}

local fbsd_logo
local beastie_color
local beastie
local fbsd_logo_v
local orb_color
local orb
local none
local none_shifted = false

local function menu_entry_name(drawing_menu, entry)
	local name_handler = drawer.menu_name_handlers[entry.entry_type]

	if name_handler ~= nil then
		return name_handler(drawing_menu, entry)
	end
	if type(entry.name) == "function" then
		return entry.name()
	end
	return entry.name
end

local function shift_brand_text(shift)
	drawer.brand_position.x = drawer.brand_position.x + shift.x
	drawer.brand_position.y = drawer.brand_position.y + shift.y
	drawer.menu_position.x = drawer.menu_position.x + shift.x
	drawer.menu_position.y = drawer.menu_position.y + shift.y
	drawer.box_pos_dim.x = drawer.box_pos_dim.x + shift.x
	drawer.box_pos_dim.y = drawer.box_pos_dim.y + shift.y
end

fbsd_logo = {
"  ______               ____   _____ _____  ",
" |  ____|             |  _ \\ / ____|  __ \\ ",
" | |___ _ __ ___  ___ | |_) | (___ | |  | |",
" |  ___| '__/ _ \\/ _ \\|  _ < \\___ \\| |  | |",
" | |   | | |  __/  __/| |_) |____) | |__| |",
" | |   | | |    |    ||     |      |      |",
" |_|   |_|  \\___|\\___||____/|_____/|_____/ "
}

beastie_color = {
"               \027[31m,        ,",
"              /(        )`",
"              \\ \\___   / |",
"              /- \027[37m_\027[31m  `-/  '",
"             (\027[37m/\\/ \\\027[31m \\   /\\",
"             \027[37m/ /   |\027[31m `    \\",
"             \027[34mO O   \027[37m) \027[31m/    |",
"             \027[37m`-^--'\027[31m`<     '",
"            (_.)  _  )   /",
"             `.___/`    /",
"               `-----' /",
"  \027[33m<----.\027[31m     __ / __   \\",
"  \027[33m<----|====\027[31mO)))\027[33m==\027[31m) \\) /\027[33m====|",
"  \027[33m<----'\027[31m    `--' `.__,' \\",
"               |        |",
"                \\       /       /\\",
"           \027[36m______\027[31m( (_  / \\______/",
"         \027[36m,'  ,-----'   |",
"         `--{__________)\027[37m"
}

beastie = {
"               ,        ,",
"              /(        )`",
"              \\ \\___   / |",
"              /- _  `-/  '",
"             (/\\/ \\ \\   /\\",
"             / /   | `    \\",
"             O O   ) /    |",
"             `-^--'`<     '",
"            (_.)  _  )   /",
"             `.___/`    /",
"               `-----' /",
"  <----.     __ / __   \\",
"  <----|====O)))==) \\) /====|",
"  <----'    `--' `.__,' \\",
"               |        |",
"                \\       /       /\\",
"           ______( (_  / \\______/",
"         ,'  ,-----'   |",
"         `--{__________)"
}

fbsd_logo_v = {
"  ______",
" |  ____| __ ___  ___ ",
" | |__ | '__/ _ \\/ _ \\",
" |  __|| | |  __/  __/",
" | |   | | |    |    |",
" |_|   |_|  \\___|\\___|",
"  ____   _____ _____",
" |  _ \\ / ____|  __ \\",
" | |_) | (___ | |  | |",
" |  _ < \\___ \\| |  | |",
" | |_) |____) | |__| |",
" |     |      |      |",
" |____/|_____/|_____/"
}

orb_color = {
"  \027[31m```                        \027[31;1m`\027[31m",
" s` `.....---...\027[31;1m....--.```   -/\027[31m",
" +o   .--`         \027[31;1m/y:`      +.\027[31m",
"  yo`:.            \027[31;1m:o      `+-\027[31m",
"   y/               \027[31;1m-/`   -o/\027[31m",
"  .-                  \027[31;1m::/sy+:.\027[31m",
"  /                     \027[31;1m`--  /\027[31m",
" `:                          \027[31;1m:`\027[31m",
" `:                          \027[31;1m:`\027[31m",
"  /                          \027[31;1m/\027[31m",
"  .-                        \027[31;1m-.\027[31m",
"   --                      \027[31;1m-.\027[31m",
"    `:`                  \027[31;1m`:`",
"      \027[31;1m.--             `--.",
"         .---.....----.\027[37m"
}

orb = {
"  ```                        `",
" s` `.....---.......--.```   -/",
" +o   .--`         /y:`      +.",
"  yo`:.            :o      `+-",
"   y/               -/`   -o/",
"  .-                  ::/sy+:.",
"  /                     `--  /",
" `:                          :`",
" `:                          :`",
"  /                          /",
"  .-                        -.",
"   --                      -.",
"    `:`                  `:`",
"      .--             `--.",
"         .---.....----."
}

none = {""}

-- Module exports
drawer.menu_name_handlers = {
	-- Menu name handlers should take the menu being drawn and entry being
	-- drawn as parameters, and return the name of the item.
	-- This is designed so that everything, including menu separators, may
	-- have their names derived differently. The default action for entry
	-- types not specified here is to use entry.name directly.
	[core.MENU_SEPARATOR] = function(_, entry)
		if entry.name ~= nil then
			if type(entry.name) == "function" then
				return entry.name()
			end
			return entry.name
		end
		return ""
	end,
	[core.MENU_CAROUSEL_ENTRY] = function(_, entry)
		local carid = entry.carousel_id
		local caridx = config.getCarouselIndex(carid)
		local choices = entry.items
		if type(choices) == "function" then
			choices = choices()
		end
		if #choices < caridx then
			caridx = 1
		end
		return entry.name(caridx, choices[caridx], choices)
	end,
}

drawer.brand_position = {x = 2, y = 1}
drawer.logo_position = {x = 46, y = 1}
drawer.menu_position = {x = 6, y = 11}
drawer.box_pos_dim = {x = 3, y = 10, w = 41, h = 11}

drawer.branddefs = {
	-- Indexed by valid values for loader_brand in loader.conf(5). Valid
	-- keys are: graphic (table depicting graphic)
	["fbsd"] = {
		graphic = fbsd_logo,
	},
	["none"] = {
		graphic = none,
	},
}

drawer.logodefs = {
	-- Indexed by valid values for loader_logo in loader.conf(5). Valid keys
	-- are: requires_color (boolean), graphic (table depicting graphic), and
	-- shift (table containing x and y).
	["beastie"] = {
		requires_color = true,
		graphic = beastie_color,
	},
	["beastiebw"] = {
		graphic = beastie,
	},
	["fbsdbw"] = {
		graphic = fbsd_logo_v,
		shift = {x = 5, y = 4},
	},
	["orb"] = {
		requires_color = true,
		graphic = orb_color,
		shift = {x = 2, y = 4},
	},
	["orbbw"] = {
		graphic = orb,
		shift = {x = 2, y = 4},
	},
	["tribute"] = {
		graphic = fbsd_logo,
	},
	["tributebw"] = {
		graphic = fbsd_logo,
	},
	["none"] = {
		graphic = none,
		shift = {x = 17, y = 0},
	},
}

function drawer.drawscreen(menu_opts)
	-- drawlogo() must go first.
	-- it determines the positions of other elements
	drawer.drawlogo()
	drawer.drawbrand()
	drawer.drawbox()
	return drawer.drawmenu(menu_opts)
end

function drawer.drawmenu(m)
	local x = drawer.menu_position.x
	local y = drawer.menu_position.y

	-- print the menu and build the alias table
	local alias_table = {}
	local entry_num = 0
	local menu_entries = m.entries
	local effective_line_num = 0
	if type(menu_entries) == "function" then
		menu_entries = menu_entries()
	end
	for line_num, e in ipairs(menu_entries) do
		-- Allow menu items to be conditionally visible by specifying
		-- a visible function.
		if e.visible ~= nil and not e.visible() then
			goto continue
		end
		effective_line_num = effective_line_num + 1
		if e.entry_type ~= core.MENU_SEPARATOR then
			entry_num = entry_num + 1
			screen.setcursor(x, y + effective_line_num)

			print(entry_num .. ". " .. menu_entry_name(m, e))

			-- fill the alias table
			alias_table[tostring(entry_num)] = e
			if e.alias ~= nil then
				for _, a in ipairs(e.alias) do
					alias_table[a] = e
				end
			end
		else
			screen.setcursor(x, y + effective_line_num)
			print(menu_entry_name(m, e))
		end
		::continue::
	end
	return alias_table
end


function drawer.drawbox()
	local x = drawer.box_pos_dim.x
	local y = drawer.box_pos_dim.y
	local w = drawer.box_pos_dim.w
	local h = drawer.box_pos_dim.h

	local hl = string.char(0xCD)
	local vl = string.char(0xBA)

	local tl = string.char(0xC9)
	local bl = string.char(0xC8)
	local tr = string.char(0xBB)
	local br = string.char(0xBC)

	screen.setcursor(x, y); print(tl)
	screen.setcursor(x, y+h); print(bl)
	screen.setcursor(x+w, y); print(tr)
	screen.setcursor(x+w, y+h); print(br)

	for i = 1, w-1 do
		screen.setcursor(x+i, y)
		print(hl)
		screen.setcursor(x+i, y+h)
		print(hl)
	end

	for i = 1, h-1 do
		screen.setcursor(x, y+i)
		print(vl)
		screen.setcursor(x+w, y+i)
		print(vl)
	end

	screen.setcursor(x+(w/2)-9, y)
	print("Welcome to FreeBSD")
end

function drawer.draw(x, y, logo)
	for i = 1, #logo do
		screen.setcursor(x, y + i)
		print(logo[i])
	end
end

function drawer.drawbrand()
	local x = tonumber(loader.getenv("loader_brand_x")) or
	    drawer.brand_position.x
	local y = tonumber(loader.getenv("loader_brand_y")) or
	    drawer.brand_position.y

	local graphic = drawer.branddefs[loader.getenv("loader_brand")]
	if graphic == nil then
		graphic = fbsd_logo
	end
	drawer.draw(x, y, graphic)
end

function drawer.drawlogo()
	local x = tonumber(loader.getenv("loader_logo_x")) or
	    drawer.logo_position.x
	local y = tonumber(loader.getenv("loader_logo_y")) or
	    drawer.logo_position.y

	local logo = loader.getenv("loader_logo")
	local colored = color.isEnabled()

	-- Lookup
	local logodef = drawer.logodefs[logo]

	if logodef ~= nil and logodef.graphic == none then
		-- centre brand and text if no logo
		if not none_shifted then
			shift_brand_text(logodef.shift)
			none_shifted = true
		end
	elseif logodef == nil or logodef.graphic == nil or
	    (not colored and logodef.requires_color) then
		-- Choose a sensible default
		if colored then
			logodef = drawer.logodefs["orb"]
		else
			logodef = drawer.logodefs["orbbw"]
		end
	end
	if logodef.shift ~= nil then
		x = x + logodef.shift.x
		y = y + logodef.shift.y
	end
	drawer.draw(x, y, logodef.graphic)
end

return drawer
