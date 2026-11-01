## Module `ucl`

This lua module allows to parse objects from strings and to store data into
ucl objects. It uses `libucl` C library to parse and manipulate with ucl objects.

Example:

~~~lua
local ucl = require("ucl")

local parser = ucl.parser()
local res,err = parser:parse_string('{key=value}')

if not res then
	print('parser error: ' .. err)
else
	local obj = parser:get_object()
	local got = ucl.to_format(obj, 'json')
end

local table = {
  str = 'value',
  num = 100500,
  null = ucl.null,
  func = function ()
    return 'huh'
  end
}


print(ucl.to_format(table, 'ucl'))
-- Output:
--[[
num = 100500;
str = "value";
null = null;
func = "huh";
--]]
~~~

###Brief content:

**Functions**:

> [`ucl_object_push_lua(L, obj, allow_array)`](#function-ucl_object_push_lual-obj-allow_array)

> [`ucl.to_format(var, format)`](#function-uclto_formatvar-format)

> [`ucl.untrusted_parser(limits)`](#function-ucluntrusted_parserlimits)



**Methods**:

> [`parser:parse_file(name)`](#method-parserparse_filename)

> [`parser:parse_string(input)`](#method-parserparse_stringinput)

> [`parser:get_object()`](#method-parserget_object)

> [`parser:set_limits(limits)`](#method-parserset_limitslimits)

> [`parser:get_limits()`](#method-parserget_limits)


## Functions

The module `ucl` defines the following functions.

### Function `ucl_object_push_lua(L, obj, allow_array)`

This is a `C` function to push `UCL` object as lua variable. This function
converts `obj` to lua representation using the following conversions:

- *scalar* values are directly presented by lua objects
- *userdata* values are converted to lua function objects using `LUA_REGISTRYINDEX`,
this can be used to pass functions from lua to c and vice-versa
- *arrays* are converted to lua tables with numeric indices suitable for `ipairs` iterations
- *objects* are converted to lua tables with string indices

**Parameters:**

- `L {lua_State}`: lua state pointer
- `obj {ucl_object_t}`: object to push
- `allow_array {bool}`: expand implicit arrays (should be true for all but partial arrays)

**Returns:**

- `{int}`: `1` if an object is pushed to lua

Back to [module description](#module-ucl).

### Function `ucl.to_format(var, format)`

Converts lua variable `var` to the specified `format`. Formats supported are:

- `json` - fine printed json
- `json-compact` - compacted json
- `config` - fine printed configuration
- `ucl` - same as `config`
- `yaml` - embedded yaml

If `var` contains function, they are called during output formatting and if
they return string value, then this value is used for ouptut.

**Parameters:**

- `var {variant}`: any sort of lua variable (if userdata then metafield `__to_ucl` is searched for output)
- `format {string}`: any available format

**Returns:**

- `{string}`: string representation of `var` in the specific `format`.

Example:

~~~lua
local table = {
  str = 'value',
  num = 100500,
  null = ucl.null,
  func = function ()
    return 'huh'
  end
}


print(ucl.to_format(table, 'ucl'))
-- Output:
--[[
num = 100500;
str = "value";
null = null;
func = "huh";
--]]
~~~

Back to [module description](#module-ucl).


### Function `ucl.untrusted_parser(limits)`

Creates a parser with `UCL_PARSER_SAFE_FLAGS` and structural budgets suitable for
input that did not come from the local configuration: a depth of 64, a million
elements, 64MB of tree, 1KB keys and 16MB strings. Any of them can be overridden
per call site, and zero still means unlimited.

`ucl.parser()` is unchanged and keeps the libucl defaults, which only guard
nesting depth.

**Parameters:**

- `limits {table or nil}`: optional overrides, any of `max_depth`, `max_nodes`, `max_alloc`, `max_key_length`, `max_string_length`

**Returns:**

- `{parser}`: new parser object

Back to [module description](#module-ucl).


## Methods

The module `ucl` defines the following methods.

### Method `parser:parse_file(name)`

Parse UCL object from file.

**Parameters:**

- `name {string}`: filename to parse

**Returns:**

- `{bool[, string]}`: if res is `true` then file has been parsed successfully, otherwise an error string is also returned

Example:

~~~lua
local parser = ucl.parser()
local res,err = parser:parse_file('/some/file.conf')

if not res then
	print('parser error: ' .. err)
else
	-- Do something with object
end
~~~

Back to [module description](#module-ucl).

### Method `parser:parse_string(input)`

Parse UCL object from file.

**Parameters:**

- `input {string}`: string to parse

**Returns:**

- `{bool[, string]}`: if res is `true` then file has been parsed successfully, otherwise an error string is also returned

Back to [module description](#module-ucl).

### Method `parser:get_object()`

Get top object from parser and export it to lua representation.

**Parameters:**

	nothing

**Returns:**

- `{variant or nil}`: ucl object as lua native variable

Back to [module description](#module-ucl).


### Method `parser:set_limits(limits)`

Overlays the given limits on the ones already in effect, so tightening a single
field does not silently drop the rest. An unknown key is an error rather than a
quiet no-op. Zero means unlimited for any field.

**Parameters:**

- `limits {table}`: any of `max_depth`, `max_nodes`, `max_alloc`, `max_key_length`, `max_string_length`

**Returns:**

- `{parser}`: the parser itself, for chaining

Back to [module description](#module-ucl).


### Method `parser:get_limits()`

Returns the limits currently in effect for the parser.

**Parameters:**

	nothing

**Returns:**

- `{table}`: table with `max_depth`, `max_nodes`, `max_alloc`, `max_key_length` and `max_string_length`

Back to [module description](#module-ucl).


Back to [top](#).

