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

local screen = {};

local color = require("color");
local core = require("core");

-- XXX TODO: This should be fixed in the interpreter to not print decimals
function intstring(num)
	local str = tostring(num);
	local decimal = str:find("%.");

	if (decimal) then
		return str:sub(1, decimal - 1);
	end
	return str;
end

function screen.clear()
	if (core.bootserial()) then
		return;
	end
	loader.printc("\027[H\027[J");
end

function screen.setcursor(x, y)
	if (core.bootserial()) then
		return;
	end

	loader.printc("\027["..intstring(y)..";"..intstring(x).."H");
end

function screen.setforeground(c)
	if (color.disabled) then
		return c;
	end
	loader.printc("\027[3"..c.."m");
end

function screen.setbackground(c)
	if (color.disabled) then
		return c;
	end
	loader.printc("\027[4"..c.."m");
end

function screen.defcolor()
	loader.printc(color.default());
end

function screen.defcursor()
	if (core.bootserial()) then
		return;
	end
	loader.printc("\027[25;0H");
end

return screen;
