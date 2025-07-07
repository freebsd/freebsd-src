#!/usr/libexec/flua

--[[ usage:
generare-ucl.lua [<variablename> <variablevalue>]... <sourceucl> <destucl>

Build a package's UCL configuration by loading the template UCL file
<sourceucl>, replacing any $VARIABLES in the UCL based on the provided
variables, then writing the result to <destucl>.

If COMMENT_SUFFIX or DESC_SUFFIX are set, append these to the generated comment
and desc fields.  We do this here because there's no way to do it in
template.ucl.
]]--

local ucl = require("ucl")

-- Hardcode a list of packages which don't get the automatic pkggenname
-- dependency because the base package doesn't exist.  We should have a better
-- way to handle this.
local no_gen_deps = {
	["libcompat-dev"] = true,
	["libcompat-dev-lib32"] = true,
	["libcompat-man"] = true,
	["libcompiler_rt-dev"] = true,
	["libcompiler_rt-dev-lib32"] = true,
	["liby-dev"] = true,
	["liby-dev-lib32"] = true,
}

-- Return true if the package 'pkgname' should have a dependency on the package
-- pkggenname.
function add_gen_dep(pkgname, pkggenname)
	if pkgname == pkggenname then
		return false
	end
	if pkgname == nil or pkggenname == nil then
		return false
	end
	if no_gen_deps[pkgname] ~= nil then
		return false
	end
	if pkggenname == "kernel" then
		return false
	end

	return true
end

local pkgname = nil
local pkggenname = nil
local pkgprefix = nil
local pkgversion = nil
local comment_suffix = nil
local desc_suffix = nil

-- This parser is the output UCL we want to build.
local parser = ucl.parser()

-- Set any $VARIABLES from the command line in the parser.  This causes ucl to
-- automatically replace them when we load the source ucl.
if #arg < 2 or #arg % 2 ~= 0 then
	io.stderr:write(arg[0] .. ": expected an even number of arguments, got " .. #arg)
	os.exit(1)
end

for i = 2, #arg - 2, 2 do
	local varname = arg[i - 1]
	local varvalue = arg[i]

	if varname == "PKGNAME" and #varvalue > 0 then
		pkgname = varvalue
	elseif varname == "PKGGENNAME" and #varvalue > 0 then
		pkggenname = varvalue
	elseif varname == "VERSION" and #varvalue > 0 then
		pkgversion = varvalue
	elseif varname == "COMMENT_SUFFIX" and #varvalue > 0 then
		comment_suffix = varvalue
	elseif varname == "DESC_SUFFIX" and #varvalue > 0 then
		desc_suffix = varvalue
	elseif varname == "PKG_NAME_PREFIX" and #varvalue > 0 then
		pkgprefix = varvalue
	end

	parser:register_variable(varname, varvalue)
end

-- Load the source ucl file.
local res,err = parser:parse_file(arg[#arg - 1])
if not res then
	io.stderr:write(arg[0] .. ": fail to parse("..arg[#arg - 1].."): "..err)
	os.exit(1)
end

local obj = parser:get_object()

-- If pkgname is different from pkggenname, add a dependency on pkggenname.
-- This means that e.g. -dev packages depend on their respective base package.
if add_gen_dep(pkgname, pkggenname) then
	if obj["deps"] == nil then
		obj["deps"] = {}
	end
	obj["deps"][pkggenname] = {
		["version"] = pkgversion,
		["origin"] = "base"
	}
end

-- If PKG_NAME_PREFIX is provided, rewrite the names of dependency packages.
-- We can't do this in UCL since variable substitution doesn't work in array
-- keys.
if pkgprefix ~= nil and obj["deps"] ~= nil then
	newdeps = {}
	for dep, opts in pairs(obj["deps"]) do
		local newdep = pkgprefix .. "-" .. dep
		newdeps[newdep] = opts
	end
	obj["deps"] = newdeps
end

-- Add comment and desc suffix.
if comment_suffix ~= nil then
	obj["comment"] = obj["comment"] .. comment_suffix
end
if desc_suffix ~= nil then
	obj["desc"] = obj["desc"] .. "\n\n" .. desc_suffix
end

-- Write the output file.
local f,err = io.open(arg[#arg], "w")
if not f then
	io.stderr:write(arg[0] .. ": fail to open("..arg[#arg].."): ".. err)
	os.exit(1)
end

f:write(ucl.to_format(obj, 'ucl', true))
f:close()
