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

local core = require("core");
local screen = require("screen");

local password = {};

function password.read()
	local str = "";
	local n = 0;

	repeat
		ch = io.getchar();
		if (ch == core.KEY_ENTER) then
			break;
		end
		-- XXX TODO: Evaluate if we really want this or not, as a
		-- security consideration of sorts
		if (ch == core.KEY_BACKSPACE) or (ch == core.KEY_DELETE) then
			if (n > 0) then
				n = n - 1;
				-- loader.printc("\008 \008");
				str = str:sub(1, n);
			end
		else
			-- loader.printc("*");
			str = str .. string.char(ch);
			n = n + 1;
		end
	until (n == 16);
	return str;
end

function password.check()
	screen.clear();
	screen.defcursor();
	-- pwd is optionally supplied if we want to check it
	local function do_prompt(prompt, pwd)
		while (true) do
			loader.printc(prompt);
			local read_pwd = password.read();
			if (not pwd) or (pwd == read_pwd) then
				-- Throw an extra newline after password prompt
				print("");
				return read_pwd;
			end
			print("\n\nloader: incorrect password!\n");
			loader.delay(3*1000*1000);
		end
	end
	local function compare(prompt, pwd)
		if (pwd == nil) then
			return;
		end
		do_prompt(prompt, pwd);
	end

	local boot_pwd = loader.getenv("bootlock_password");
	compare("Boot password: ", boot_pwd);

	local geli_prompt = loader.getenv("geom_eli_passphrase_prompt");
	if (geli_prompt ~= nil) and (geli_prompt:lower() == "yes") then
		local passphrase = do_prompt("GELI Passphrase: ");
		loader.setenv("kern.geom.eli.passphrase", passphrase);
	end

	local pwd = loader.getenv("password");
	if (pwd ~= nil) then
		core.autoboot();
	end
	compare("Password: ", pwd);
end

return password;
