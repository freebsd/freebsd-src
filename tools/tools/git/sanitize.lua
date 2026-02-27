-- SPDX-License-Identifier: BSD-2-Clause
-- Copyright 2026 Warner Losh <imp@FreeBSD.org>

--
-- Removes all comments, blank lines and extra whitespace from a C header file
-- and inserts a generated from comment at the top. Generally, this extracts the
-- smallest subset of the file that describes the interface that is necessary to
-- interoperate with that software. The user of this program should check the
-- results, however, to ensure the result minimally describes the public
-- interface.
--
-- When applied to device-tree binding files, this will result in the #defines
-- being extracted, which are needed to generate the .dtb files, as well as for
-- code to interpret the .dtb files. The device-tree files must be written this
-- way to be used for this dual purpose. Other header files may not be so
-- constrained, which makes review necessary for those context.
--

--
-- Useage lua sanitize.lua fn description
--
-- fn will be read in, sanitized and the results printed on stdout.
-- The description will be all remaining args and will be inserted
-- in the first line comment to describe where the source file was
-- obtained from.
--

-- Open the file from the command line
local fn = arg[1]
if not fn then
	print("Usage: sanitize fn")
	os.exit(1)
end

-- read it all in
local f = assert(io.open(fn))
local content = f:read("*all")
f:close()

-- Transform
content = content:gsub("/%*.-%*/", "")		-- Remove block comments, .- is lazy, not greed, match
content = content:gsub("//[^\n]*", "")		-- Remove single line comments 
content = content:gsub("%s*\n", "\n")		-- Remove trailing white space
content = content:gsub("\t+", " ")		-- Convert blocks of tabs to a space
content = content:gsub("\n+", "\n")		-- Remove blank lines
content = content:gsub("\n+$", "")		-- Strip blank lines at the end (print adds one)
content = content:gsub("^\n+", "")		-- Strip leading blank lines

print("/* @" .. "generated from the interface found in " .. fn .. " -- result is in public domain */")
if arg[2] then
	print("/* from " .. table.concat(table.pack(table.unpack(arg, 2)), ' ') .. " */")
end
print(content)
