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

include("/boot/screen.lua");
drawer = {};

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

drawer.logo_position = {x = 46, y = 4};
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

drawer.fbsd_logo_shift = {x = 5, y = 6};
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

drawer.orb_shift = {x = 3, y = 0};
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


function drawer.draw(x, y, logo)
    for i = 1, #logo do
        screen.setcursor(x, y + i);
        print(logo[i]);
    end
end

function drawer.drawbrand()
    local x = tonumber(loader.getenv("loader_brand_x"));
    local y = tonumber(loader.getenv("loader_brand_y"));
    
    if not x then x = drawer.brand_position.x; end
    if not y then y = drawer.brand_position.y; end
    
    local logo = load("return " .. tostring(loader.getenv("loader_brand")))();
    if not logo then logo = drawer.fbsd_logo; end
    drawer.draw(x, y, logo);
end

function drawer.drawlogo()
    local x = tonumber(loader.getenv("loader_logo_x"));
    local y = tonumber(loader.getenv("loader_logo_y"));
    
    if not x then x = drawer.logo_position.x; end
    if not y then y = drawer.logo_position.y; end
    
    local logo = loader.getenv("loader_logo");
    local s = {x = 0, y = 0};
    local colored = color.isEnabled();
    
    if logo == "beastie" then
        if colored then logo = drawer.beastie_color; end
    elseif logo == "beastiebw" then
        logo = drawer.beastie;
    elseif logo == "fbsdbw" then
        logo = drawer.fbsd_logo_v;
        s = drawer.fbsd_logo_shift;
    elseif logo == "orb" then
        if colored then logo = drawer.orb_color; end
        s = drawer.orb_shift;
    elseif logo == "orbbw" then
        logo = drawer.orb;
        s = drawer.orb_shift;
    elseif logo == "tribute" then
        logo = drawer.fbsd_logo;
    elseif logo == "tributebw" then
        logo = drawer.fbsd_logo;
    end
    if not logo then
        if colored then logo = drawer.orb_color;
        else logo = drawer.orb; end
    end
    drawer.draw(x + s.x, y + s.y, logo);
end
