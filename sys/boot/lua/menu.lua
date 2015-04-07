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

include("/boot/core.lua");
include("/boot/config.lua");
include("/boot/screen.lua");
include("/boot/drawer.lua");

menu = {};

function menu.draw(x, y, opts)
    screen.setcursor(x, y);
    print("Boot Menu");
    for k, v in pairs(opts) do
        -- skip alias
        if k ~= "alias" then
            screen.setcursor(x, y + v.index);
            local name = v.name;
            
            if (name == nil) then
                name = v.getName();
            end
            print(k .. " - " .. name);
        end
    end
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
    return c == "yes";

end

function menu.run(opts)

    if menu.skip() then
        core.autoboot();
        return;
    end

    if (opts == nil) then
        opts = menu.options;
    end
    
    local draw = function() 
        screen.clear();
        menu.draw(6, 11, opts);
        menu.drawbox(4, 10, 40, 10);
        drawer.drawbrand();
        drawer.drawlogo();
        screen.defcursor();
    end
    
    draw();
    menu.autoboot();
    while true do
        local ch = string.char(io.getchar());
        if (opts[ch] ~= nil) then
            local ret = opts[ch].func();
            if (ret) then
                print("Exiting menu!");
                return;
            end
        else
        --try alias key
            if opts.alias ~= nil then
                if opts.alias[ch] ~= nil then
                    local ret = opts.alias[ch].func();
                    if (ret) then
                        print("Exiting menu!");
                        return;
                    end
                end
            end
        end
        draw();
    end
end

function menu.drawbox(x, y, w, h)
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
    
    screen.setcursor(x+1, y);
    for i = 0, w-2 do print(hl); end
    screen.setcursor(x+1, y+h);
    for i = 0, w-2 do print(hl); end
    
    
    for i = 1, h-1 do screen.setcursor(x, y+i); print(vl); end
    for i = 1, h-1 do screen.setcursor(x+w, y+i); print(vl); end
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
            if ch == 13 then
                break;
            else
                -- prevent autoboot when escaping to interpreter
                loader.perform("set autoboot_delay=NO");
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

menu.options = {
    -- Boot multi user
    ["1"] = {
        index = 1, 
        name = "Boot Multi user "..color.highlight("[Enter]"), 
        func = function () core.setSingleUser(false); core.boot(); end
    },
    -- boot single user
    ["2"] = {
        index = 2, 
        name = "Boot "..color.highlight("S").."ingle user", 
        func = function () core.setSingleUser(true); core.boot(); end
    },
    -- escape to interpreter
    ["3"] = {
        index = 3,
        name = color.highlight("Esc").."ape to lua interpreter", 
        func = function () return true; end
    },
    -- reboot
    ["4"] = {
        index = 4, 
        name = color.highlight("R").."eboot", 
        func = function () loader.perform("reboot"); end
    },
    -- boot options
    ["5"] = {
        index = 5, 
        name = "Boot "..color.highlight("O").."ptions", 
        func = function () menu.run(boot_options); return false; end
    },
    ["6"] = {
        index = 6,
        getName = function ()
            local k = core.kernelList();
            if #k == 0 then 
                return "Kernels (not availabe)";
            end
            return "Kernels";
        end,
        func = function() 
            local kernels = {};
            local ker = core.kernelList();
            if #ker == 0 then return false; end
            
            kernels["1"] = {
                index = 1,
                name = "Return to menu "..color.highlight("[Backspace]"),
                func = function() return true; end
            };
            kernels.alias = {["\008"] = kernels["1"]};
            for k, v in ipairs(ker) do
                kernels[tostring(k+1)] = {
                    index = k+1,
                    name = v,
                    func = function() config.reload(v); end
                };
            end
            menu.run(kernels);
            return false;
        end
    }
};

menu.options.alias = {
    ["\013"] = menu.options["1"],
    ["s"] = menu.options["2"],
    ["\027"] = menu.options["3"],
    ["r"] = menu.options["4"],
    ["o"] = menu.options["5"]
};

function OnOff(str, b)
    if (b) then
        return str .. color.escapef(color.GREEN).."On"..color.escapef(color.WHITE);
    else
        return str .. color.escapef(color.RED).."off"..color.escapef(color.WHITE);
    end
end

boot_options = {
    ["1"] = {
        index = 1,
        name = "Back to menu"..color.highlight(" [Backspace]"),
        func = function () return true; end
    },
    ["2"] = {
        index = 2,
        name = "Load System "..color.highlight("D").."efaults",
        func = function () core.setDefaults(); return false; end
    },
    ["3"] = {
        index = 3,
        getName = function () 
            return OnOff(color.highlight("A").."CPI       :", core.acpi);
        end,
        func = function () core.setACPI(); return false; end
    },
    ["4"] = {
        index = 4,
        getName = function () 
            return OnOff("Safe "..color.highlight("M").."ode  :", core.sm);
        end,
        func = function () core.setSafeMode(); return false; end
    },
    ["5"] = {
        index = 5,
        getName = function () 
            return OnOff(color.highlight("S").."ingle user:", core.su);
        end,
        func = function () core.setSingleUser(); return false; end
    },
    ["6"] = {
        index = 6,
        getName = function () 
            return OnOff(color.highlight("V").."erbose    :", core.verbose);
        end,
        func = function () core.setVerbose(); return false; end
    }
}

boot_options.alias = {
    ["\08"] = boot_options["1"],
    ["d"] = boot_options["2"],
    ["a"] = boot_options["3"],
    ["m"] = boot_options["4"],
    ["s"] = boot_options["5"],
    ["v"] = boot_options["6"] 
};
