#!/usr/libexec/flua
--
-- Copyright (c) 2024-2025 Baptiste Daroussin <bapt@FreeBSD.org>
-- Copyright (c) 2025 Lexi Winter <ivy@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

--[[ usage:
generate-set-ucl.lua <template> [<variablename> <variablevalue>]

Generate the UCL for a set metapackage.  The variables provided will be
substituted as UCL variables.
]]--

local ucl = require("ucl")

-- This parser is the output UCL we want to build.
local parser = ucl.parser()

if #arg < 1 then
	io.stderr:write(arg[0] .. ": missing template filename\n")
	os.exit(1)
end

local template = table.remove(arg, 1)

-- Set any $VARIABLES from the command line in the parser.  This causes ucl to
-- automatically replace them when we load the source ucl.
if #arg % 2 ~= 0 then
	io.stderr:write(arg[0] .. ": expected an even number of arguments, "
	    .. "got " .. #arg .. "\n")
	os.exit(1)
end

local pkgprefix = nil
local pkgversion = nil
local pkgdeps = ""

for i = 2, #arg, 2 do
	local varname = arg[i - 1]
	local varvalue = arg[i]

	if varname == "PKG_NAME_PREFIX" and #varvalue > 0 then
		pkgprefix = varvalue
	elseif varname == "VERSION" and #varvalue > 0 then
		pkgversion = varvalue
	elseif varname == "SET_DEPENDS" and #varvalue > 0 then
		pkgdeps = varvalue
	end

	parser:register_variable(varname, varvalue)
end

-- Load the source template.
local res,err = parser:parse_file(template)
if not res then
	io.stderr:write(arg[0] .. ": fail to parse(" .. template .. "): "
	    .. err .. "\n")
	os.exit(1)
end

local obj = parser:get_object()

-- Dependency handling.
obj["deps"] = obj["deps"] or {}

-- If PKG_NAME_PREFIX is provided, rewrite the names of dependency packages.
-- We can't do this in UCL since variable substitution doesn't work in array
-- keys.  Note that this only applies to dependencies from the set UCL files,
-- because SET_DEPENDS already have the correct prefix.
if pkgprefix ~= nil then
	newdeps = {}
	for dep, opts in pairs(obj["deps"]) do
		local newdep = pkgprefix .. "-" .. dep
		newdeps[newdep] = opts
	end
	obj["deps"] = newdeps
end

-- Add dependencies from SET_DEPENDS.
for dep in string.gmatch(pkgdeps, "[^%s]+") do
	obj["deps"][dep] = {
		["origin"] = "base/"..dep
	}
end

-- Add a version and origin key to all dependencies, otherwise pkg
-- doesn't like it.
for dep, opts in pairs(obj["deps"]) do
	obj["deps"][dep]["origin"] = obj["deps"][dep]["origin"] or "base/"..dep
	obj["deps"][dep]["version"] = obj["deps"][dep]["version"] or pkgversion
end

-- If there are no dependencies, remove the deps key, otherwise pkg raises an
-- error trying to read the manifest.
if next(obj["deps"]) == nil then
	obj["deps"] = nil
end

-- Write the output.
io.stdout:write(ucl.to_format(obj, 'ucl', true))
