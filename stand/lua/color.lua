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

local color = {};

local core = require("core");

color.BLACK   = 0;
color.RED     = 1;
color.GREEN   = 2;
color.YELLOW  = 3;
color.BLUE    = 4;
color.MAGENTA = 5;
color.CYAN    = 6;
color.WHITE   = 7;

color.DEFAULT = 0;
color.BRIGHT  = 1;
color.DIM     = 2;

function color.isEnabled()
	local c = loader.getenv("loader_color");
	if (c ~= nil) then
		if (c:lower() == "no") or (c == "0") then
			return false;
		end
	end
	return (not core.isSerialBoot());
end

color.disabled = (not color.isEnabled());

function color.escapef(c)
	if (color.disabled) then
		return c;
	end
	return "\027[3"..c.."m";
end

function color.escapeb(c)
	if (color.disabled) then
		return c;
	end
	return "\027[4"..c.."m";
end

function color.escape(fg, bg, att)
	if (color.disabled) then
		return "";
	end
	if (not att) then
		att = ""
	else
		att = att..";";
	end
	return "\027["..att.."3"..fg..";4"..bg.."m";
end

function color.default()
	if (color.disabled) then
		return "";
	end
	return "\027[0;37;40m";
end

function color.highlight(str)
	if (color.disabled) then
		return str;
	end
	return "\027[1m"..str.."\027[0m";
end

return color;
