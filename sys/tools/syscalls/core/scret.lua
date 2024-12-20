--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2024 Tyler Baxter <agge@FreeBSD.org>
-- Copyright (c) 2023 Warner Losh <imp@bsdimp.com>
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--

local util = require("tools.util")

local scret = {}

scret.__index = scret

-- Processes this return type.
function scret:process()
	local words = util.split(self.scret, "%S+")
	self.scret = words[1]
	-- Pointer incoming.
	if words[2]:sub(1,1) == "*" then
		self.scret = self.scret .. " "
	end
	while words[2]:sub(1,1) == "*" do
		words[2] = words[2]:sub(2)
		self.scret = self.scret .. "*"
	end
end

-- To add this return type to the system call.
function scret:add()
	self:process()
	return self.scret
end

function scret:new(obj, line)
	obj = obj or { }
	setmetatable(obj, self)
	self.__index = self

	self.scret = line

	return obj
end

return scret
