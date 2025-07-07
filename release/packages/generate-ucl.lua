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

	if varname == "COMMENT_SUFFIX" and #varvalue > 0 then
		comment_suffix = varvalue
	elseif varname == "DESC_SUFFIX" and #varvalue > 0 then
		desc_suffix = varvalue
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
