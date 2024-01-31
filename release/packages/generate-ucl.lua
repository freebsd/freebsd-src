#!/usr/libexec/flua

--[[ usage:
generare-ucl.lua [<variablename> <variablevalue>]... <sourceucl> <destucl>

In the <destucl> files the variable <variablename> (in the form ${variablename}
in the <sourceucl>) will be expanded to <variablevalue>.

The undefined variables will reamin unmofifier "${variablename}"
]]--

local ucl = require("ucl")

if #arg < 2 or #arg % 2 ~= 0 then
	io.stderr:write(arg[0] .. ": expected an even number of arguments, got " .. #arg)
	os.exit(1)
end

local parser = ucl.parser()
for i = 2, #arg - 2, 2 do
	parser:register_variable(arg[i - 1], arg[i])
end
local res,err = parser:parse_file(arg[#arg - 1])
if not res then
	io.stderr:write(arg[0] .. ": fail to parse("..arg[#arg - 1].."): "..err)
	os.exit(1)
end
local f,err = io.open(arg[#arg], "w")
if not f then
	io.stderr:write(arg[0] .. ": fail to open("..arg[#arg].."): ".. err)
	os.exit(1)
end
local obj = parser:get_object()
f:write(ucl.to_format(obj, 'ucl'))
f:close()
