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

local drawer = {};

local color = require("color");
local core = require("core");
local screen = require("screen");

drawer.brand_position = {x = 2, y = 1};
drawer.fbsd_logo = {
	"  ______               ____   _____ _____  ",
	" |  ____|             |  _ \\ / ____|  __ \\ ",
	" | |___ _ __ ___  ___ | |_) | (___ | |  | |",
	" |  ___| '__/ _ \\/ _ \\|  _ < \\___ \\| |  | |",
	" | |   | | |  __/  __/| |_) |____) | |__| |",
	" | |   | | |    |    ||     |      |      |",
	" |_|   |_|  \\___|\\___||____/|_____/|_____/ "
};

drawer.logo_position = {x = 46, y = 1};
drawer.beastie_color = {
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
};

drawer.beastie = {
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
};

drawer.fbsd_logo_shift = {x = 5, y = 4};
drawer.fbsd_logo_v = {
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
};

drawer.orb_shift = {x = 2, y = 4};
drawer.orb_color = {
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
};

drawer.orb = {
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
};

drawer.none = {""};

drawer.none_shift = {x = 17, y = 0};

drawer.menu_position = {x = 6, y = 11};

drawer.box_pos_dim = {x = 3, y = 10, w = 41, h = 11};

function drawer.drawscreen(menu_opts)
	-- drawlogo() must go first.
	-- it determines the positions of other elements
	drawer.drawlogo();
        drawer.drawbrand();
        drawer.drawbox();
	return drawer.drawmenu(menu_opts);
end

function drawer.drawmenu(m)
	x = drawer.menu_position.x;
	y = drawer.menu_position.y;

	-- print the menu and build the alias table
	local alias_table = {};
	local entry_num = 0;
	local menu_entries = m.entries;
	if (type(menu_entries) == "function") then
		menu_entries = menu_entries();
	end
	for line_num, e in ipairs(menu_entries) do
		-- Allow menu items to be conditionally visible by specifying
		-- a visible function.
		if (e.visible ~= nil) and (not e.visible()) then
			goto continue;
		end
		if (e.entry_type ~= core.MENU_SEPARATOR) then
			entry_num = entry_num + 1;
			screen.setcursor(x, y + line_num);
			local name = "";

			if (e.entry_type == core.MENU_CAROUSEL_ENTRY) then
				local carid = e.carousel_id;
				local caridx = menu.getCarouselIndex(carid);
				local choices = e.items();

				if (#choices < caridx) then
					caridx = 1;
				end
				name = e.name(caridx, choices[caridx], choices);
			else
				name = e.name();
			end
			print(entry_num .. ". "..name);

			-- fill the alias table
			alias_table[tostring(entry_num)] = e;
			if (e.alias ~= nil) then
				for n, a in ipairs(e.alias) do
					alias_table[a] = e;
				end
			end
		else
			screen.setcursor(x, y + line_num);
			print(e.name());
		end
		::continue::
	end
	return alias_table;
end


function drawer.drawbox()
	x = drawer.box_pos_dim.x;
	y = drawer.box_pos_dim.y;
	w = drawer.box_pos_dim.w;
	h = drawer.box_pos_dim.h;

	local hl = string.char(0xCD);
	local vl = string.char(0xBA);

	local tl = string.char(0xC9);
	local bl = string.char(0xC8);
	local tr = string.char(0xBB);
	local br = string.char(0xBC);

	screen.setcursor(x, y); print(tl);
	screen.setcursor(x, y+h); print(bl);
	screen.setcursor(x+w, y); print(tr);
	screen.setcursor(x+w, y+h); print(br);

	for i = 1, w-1 do
		screen.setcursor(x+i, y);
		print(hl);
		screen.setcursor(x+i, y+h);
		print(hl);
	end

	for i = 1, h-1 do
		screen.setcursor(x, y+i);
		print(vl);
		screen.setcursor(x+w, y+i);
		print(vl);
	end

	screen.setcursor(x+(w/2)-9, y);
	print("Welcome to FreeBSD");
end

function drawer.draw(x, y, logo)
	for i = 1, #logo do
		screen.setcursor(x, y + i);
		print(logo[i]);
	end
end

function drawer.drawbrand()
	local x = tonumber(loader.getenv("loader_brand_x")) or
	    drawer.brand_position.x;
	local y = tonumber(loader.getenv("loader_brand_y")) or
	    drawer.brand_position.y;

	local logo = load("return " .. tostring(loader.getenv("loader_brand")))() or
	    drawer.fbsd_logo;
	drawer.draw(x, y, logo);
end

function drawer.drawlogo()
	local x = tonumber(loader.getenv("loader_logo_x")) or
	    drawer.logo_position.x;
	local y = tonumber(loader.getenv("loader_logo_y")) or
	    drawer.logo_position.y;

	local logo = loader.getenv("loader_logo");
	local s = {x = 0, y = 0};
	local colored = color.isEnabled();

	if (logo == "beastie") then
		if (colored) then
			logo = drawer.beastie_color;
		end
	elseif (logo == "beastiebw") then
		logo = drawer.beastie;
	elseif (logo == "fbsdbw") then
		logo = drawer.fbsd_logo_v;
		s = drawer.fbsd_logo_shift;
	elseif (logo == "orb") then
		if (colored) then
			logo = drawer.orb_color;
		end
		s = drawer.orb_shift;
	elseif (logo == "orbbw") then
		logo = drawer.orb;
		s = drawer.orb_shift;
	elseif (logo == "tribute") then
		logo = drawer.fbsd_logo;
	elseif (logo == "tributebw") then
		logo = drawer.fbsd_logo;
	elseif (logo == "none") then
		--centre brand and text if no logo
		drawer.brand_position.x = drawer.brand_position.x + drawer.none_shift.x;
		drawer.brand_position.y = drawer.brand_position.y + drawer.none_shift.y;
		drawer.menu_position.x = drawer.menu_position.x + drawer.none_shift.x;
		drawer.menu_position.y = drawer.menu_position.y + drawer.none_shift.y;
		drawer.box_pos_dim.x = drawer.box_pos_dim.x + drawer.none_shift.x;
		drawer.box_pos_dim.y = drawer.box_pos_dim.y + drawer.none_shift.y;
		--prevent redraws from moving menu further
		drawer.none_shift.x = 0;
		drawer.none_shift.y = 0;
		logo = drawer.none;
	end
	if (not logo) then
		if (colored) then
			logo = drawer.orb_color;
		else
			logo = drawer.orb;
		end
	end
	drawer.draw(x + s.x, y + s.y, logo);
end

return drawer;
