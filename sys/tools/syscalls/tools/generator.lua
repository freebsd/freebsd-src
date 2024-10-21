--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2024 Tyler Baxter <agge@FreeBSD.org>
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--

local util = require("tools.util")

local generator = {}

generator.__index = generator

-- Wrapper for Lua write() best practice, for a simpler write call.
function generator:write(line)
	assert(self.gen:write(line))
end

--
-- A write macro for the PAD64 preprocessor directive.
-- Used for 32-bit configurations by passing config.abiChanges("pair_64bit") and
-- padding will be done if necessary.
--
-- PARAM: bool, TRUE to pad.
--
function generator:pad64(bool)
	if bool then
		self:write([[
#if !defined(PAD64_REQUIRED) && !defined(__amd64__)
#define PAD64_REQUIRED
#endif
]])
	end
end

-- Returns the generated tag.
function generator:tag()
	return self.tag
end

generator.storage_levels = {}

-- Optional level to specify which order to store in, which defaults to one if
-- not provided.
function generator:store(str, level)
	level = level or 1
	self.storage_levels[level] = self.storage_levels[level] or {}
	table.insert(self.storage_levels[level], str)
end

-- Write all storage in the order it was stored.
function generator:writeStorage()
	if self.storage_levels ~= nil then
		for _, v in util.ipairsSparse(self.storage_levels) do
			for _, line in ipairs(v) do
				generator:write(line)
			end
		end
	end
end

--
-- Writes the generated preamble. Default comment is C comments.
--
-- PARAM: String str, the title for the file.
--
-- PARAM: String comment, nil or optional to change comment (e.g., "#" for sh
-- comments).
--
-- SEE: style(9)
--
function generator:preamble(str, comment)
	if str ~= nil then
		local comment_start = comment or "/*"
		local comment_middle = comment or " *"
		local comment_end = comment or " */"
		self:write(string.format("%s\n", comment_start))
		-- Splits our string into lines split by newline, or is just the
		-- original string if there's no newlines.
		for line in str:gmatch("[^\n]*") do
			-- Only add a space after the comment if there's
			-- text on this line.
			local space
			if line ~= "" then
				space = " "
			else
				space = ""
			end
			-- Make sure to append the newline back.
			self:write(string.format("%s%s%s\n", comment_middle,
			    space, line))
		end
		self:write(string.format([[%s
%s DO NOT EDIT-- this file is automatically %s.
%s

]], comment_middle, comment_middle, self.tag, comment_end))
	end
end

-- generator binds to the parameter file.
function generator:new(obj, fh)
	obj = obj or { }
	setmetatable(obj, self)
	self.__index = self

	self.gen = assert(io.open(fh, "w+"))
	self.tag = "@" .. "generated"

	return obj
end

return generator
