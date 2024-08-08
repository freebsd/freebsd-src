--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2024 Tyler Baxter <agge@FreeBSD.org>
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--

util = require("tools.util")

-- TODO: Brooks review (new name): `generator`
local bsdio = {}

bsdio.__index = bsdio

-- Wrapper for lua write() best practice. For a simpler write call.
function bsdio:write(line)
	assert(self.bsdio:write(line))
end

--
-- A write macro for the PAD64 preprocessor directive. 
-- PARAM: bool, TRUE to pad
-- USAGE: Pass config.abiChanges("pair_64bit") and padding will be done if 
-- necessary. Useful for 32-bit configurations.
--
function bsdio:pad64(bool)
    if bool then
	    self:write([[
#if !defined(PAD64_REQUIRED) && !defined(__amd64__)
#define PAD64_REQUIRED
#endif
]])
    end
end

-- Returns the generated tag.
function bsdio:tag()
    return self.tag
end

bsdio.storage_levels = {}

-- Optional level to specify which order to store in, which defaults to one if 
-- not provided.
function bsdio:store(str, level)
    level = level or 1
    self.storage_levels[level] = self.storage_levels[level] or {}
    table.insert(self.storage_levels[level], str)
end

-- Write all storage in the order it was stored.
function bsdio:writeStorage()
    if self.storage_levels ~= nil then
        for k, v in util.ipairs_sparse(self.storage_levels) do
            for _, line in ipairs(v) do
                bsdio:write(line)
            end
        end
    end
end

--
-- Writes the generated tag. Default comment is C comments. 
--
-- PARAM: String str, the title of the file
--
-- PARAM: String comment, nil or optional to change comment (e.g., to sh comments).
-- Will still follow C-style indentation.
-- SEE: style(9)
--
-- NOTE: Handles multi-line titles, deliminated by newlines.
--
function bsdio:generated(str, comment)
    local comment_start = comment or "/*"
    local comment_middle = comment or "*"
    local comment_end = comment or "*/"

    -- Don't enter loop if it's the simple case.
    if str:find("\n") == nil then
        self:write(string.format([[%s
 %s %s
 %s
 %s DO NOT EDIT-- this file is automatically %s.
 %s

]], comment_start, comment_middle, str, comment_middle, comment_middle, 
            self.tag, comment_end)) 

    -- For multi-line comments - expects newline as delimiter.
    else
        self:write(string.format("%s\n", comment_start)) -- "/*"
        for line in str:gmatch("[^\n]+") do
            if line ~= nil then
                -- Write each line with proper comment indentation (strip 
                -- newline), and tag a newline to the end.
                self:write(string.format(" %s %s\n", comment_middle, line))
            end
        end
        -- Continue as normal...
        self:write(string.format([[ %s
 %s DO NOT EDIT-- this file is automatically %s
 %s

]], comment_middle, comment_middle, self.tag, comment_end))
    end
end

-- File is part of bsdio's identity. Different objects with different identities 
-- (files) can be behave differently in a module.
function bsdio:new(obj, fh)
    obj = obj or { }
    setmetatable(obj, self)
    self.__index = self

    self.bsdio = assert(io.open(fh, "w+"))
    self.tag = "@" .. "generated" 

    return obj
end

return bsdio
