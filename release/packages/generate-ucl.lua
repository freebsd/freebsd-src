#!/usr/libexec/flua

--[[ usage:
generare-ucl.lua [<variablename> <variablevalue>]... <sourceucl> <destucl>

Build a package's UCL configuration by loading the template UCL file
<sourceucl>, replacing any $VARIABLES in the UCL based on the provided
variables, then writing the result to <destucl>.
]]--

local ucl = require("ucl")

-- Give subpackages a special comment and description suffix to indicate what
-- they contain, so e.g. "foo-man" has " (manual pages)" appended to its
-- comment.  This avoids having to create a separate ucl files for every
-- subpackage just to set this.
--
-- Note that this is not a key table because the order of the pattern matches
-- is important.
pkg_suffixes = {
	{
		"%-dev%-lib32$", "(32-bit development files)",
		"This package contains development files for compiling "..
		"32-bit applications on a 64-bit host."
	},
	{
		"%-dbg%-lib32$", "(32-bit debugging symbols)",
		"This package contains 32-bit external debugging symbols "..
		"for use with a source-level debugger.",
	},
	{
		"%-man%-lib32$", "(32-bit manual pages)",
		"This package contains the online manual pages for 32-bit "..
		"components on a 64-bit host.",
	},
	{
		"%-lib32$", "(32-bit libraries)",
		"This package contains 32-bit libraries for running 32-bit "..
		"applications on a 64-bit host.",
	},
	{
		"%-dev$", "(development files)",
		"This package contains development files for "..
		"compiling applications."
	},
	{
		"%-man$", "(manual pages)",
		"This package contains the online manual pages."
	},
	{
		"%-dbg$", "(debugging symbols)",
		"This package contains external debugging symbols for use "..
		"with a source-level debugger.",
	},
}

function add_suffixes(obj)
	local pkgname = obj["name"]
	for _,pattern in pairs(pkg_suffixes) do
		if pkgname:match(pattern[1]) ~= nil then
			obj["comment"] = obj["comment"] .. " " .. pattern[2]
			obj["desc"] = obj["desc"] .. "\n\n" .. pattern[3]
			return
		end
	end
end

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
add_suffixes(obj)

-- Write the output file.
local f,err = io.open(arg[#arg], "w")
if not f then
	io.stderr:write(arg[0] .. ": fail to open("..arg[#arg].."): ".. err)
	os.exit(1)
end

f:write(ucl.to_format(obj, 'ucl', true))
f:close()
