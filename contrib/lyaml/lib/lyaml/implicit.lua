-- LYAML parse implicit type tokens.
-- Written by Gary V. Vaughan, 2015
--
-- Copyright(C) 2015-2022 Gary V. Vaughan
--
-- Permission is hereby granted, free of charge, to any person obtaining
-- a copy of this software and associated documentation files(the
-- "Software"), to deal in the Software without restriction, including
-- without limitation the rights to use, copy, modify, merge, publish,
-- distribute, sublicense, and/or sell copies of the Software, and to
-- permit persons to whom the Software is furnished to do so, subject to
-- the following conditions:
--
-- The above copyright notice and this permission notice shall be
-- included in all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
-- EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
-- MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
-- IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
-- CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
-- TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
-- SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

--- @module lyaml.implicit


local NULL = require 'lyaml.functional'.NULL
local find = string.find
local floor = math.floor
local gsub = string.gsub
local sub = string.sub

local tointeger = (function(f)
   if not tointeger then
      -- No host tointeger implementation, use our own.
      return function(x)
         if type(x) == 'number' and x - floor(x) == 0.0 then
            return x
         end
      end

   elseif f '1' ~= nil then
      -- Don't perform implicit string-to-number conversion!
      return function(x)
         if type(x) == 'number' then
            return tointeger(x)
         end
      end
   end

   -- Host tointeger is good!
   return f
end)(math.tointeger)


local function int(x)
   local r = tonumber(x)
   if r ~= nil then
      return tointeger(r)
   end
end


local is_null = {['']=true, ['~']=true, null=true, Null=true, NULL=true}


--- Parse a null token to a null value.
-- @param value token
-- @return[1] lyaml.null, for an empty string or literal ~
-- @return[2] nil otherwise, nil
-- @usage maybe_null = implicit.null(token)
local function null(value)
   if is_null[value] then
      return NULL
   end
end


local to_bool = {
   ['true']  = true,  True  = true,  TRUE  = true,
   ['false'] = false, False = false, FALSE = false,
   yes = true,  Yes = true,  YES = true,
   no  = false, No  = false, NO  = false,
   on  = true,  On  = true,  ON  = true,
   off = false, Off = false, OFF = false,
}


--- Parse a boolean token to the equivalent value.
-- Treats capilalized, lower and upper-cased variants of true/false,
-- yes/no or on/off tokens as boolean `true` and `false` values.
-- @param value token
-- @treturn[1] bool if a valid boolean token was recognized
-- @treturn[2] nil otherwise, nil
-- @usage maybe_bool = implicit.bool(token)
local function bool(value)
   return to_bool[value]
end


--- Parse a binary token, such as '0b1010\_0111\_0100\_1010\_1110'.
-- @tparam string value token
-- @treturn[1] int integer equivalent, if a valid token was recognized
-- @treturn[2] nil otherwise, nil
-- @usage maybe_int = implicit.binary(value)
local function binary(value)
   local r
   gsub(value, '^([+-]?)0b_*([01][01_]+)$', function(sign, rest)
      r = 0
      gsub(rest, '_*(.)', function(digit)
         r = r * 2 + int(digit)
      end)
      if sign == '-' then
         r = r * -1
      end
   end)
   return r
end


--- Parse an octal token, such as '012345'.
-- @tparam string value token
-- @treturn[1] int integer equivalent, if a valid token was recognized
-- @treturn[2] nil otherwise, nil
-- @usage maybe_int = implicit.octal(value)
local function octal(value)
   local r
   gsub(value, '^([+-]?)0_*([0-7][0-7_]*)$', function(sign, rest)
      r = 0
      gsub(rest, '_*(.)', function(digit)
         r = r * 8 + int(digit)
      end)
      if sign == '-' then
         r = r * -1
      end
   end)
   return r
end


--- Parse a decimal token, such as '0' or '12345'.
-- @tparam string value token
-- @treturn[1] int integer equivalent, if a valid token was recognized
-- @treturn[2] nil otherwise, nil
-- @usage maybe_int = implicit.decimal(value)
local function decimal(value)
   local r
   gsub(value, '^([+-]?)_*([0-9][0-9_]*)$', function(sign, rest)
      rest = gsub(rest, '_', '')
      if rest == '0' or #rest > 1 or sub(rest, 1, 1) ~= '0' then
         r = int(rest)
         if sign == '-' then
            r = r * -1
         end
      end
   end)
   return r
end


--- Parse a hexadecimal token, such as '0xdeadbeef'.
-- @tparam string value token
-- @treturn[1] int integer equivalent, if a valid token was recognized
-- @treturn[2] nil otherwise, nil
-- @usage maybe_int = implicit.hexadecimal(value)
local function hexadecimal(value)
   local r
   gsub(value, '^([+-]?)(0x_*[0-9a-fA-F][0-9a-fA-F_]*)$', function(sign, rest)
      rest = gsub(rest, '_', '')
      r = int(rest)
      if sign == '-' then
         r = r * -1
      end
   end)
   return r
end


--- Parse a sexagesimal token, such as '190:20:30'.
-- Useful for times and angles.
-- @tparam string value token
-- @treturn[1] int integer equivalent, if a valid token was recognized
-- @treturn[2] nil otherwise, nil
-- @usage maybe_int = implicit.sexagesimal(value)
local function sexagesimal(value)
   local r
   gsub(value, '^([+-]?)([0-9]+:[0-5]?[0-9][:0-9]*)$', function(sign, rest)
      r = 0
      gsub(rest, '([0-9]+):?', function(digit)
         r = r * 60 + int(digit)
      end)
      if sign == '-' then
         r = r * -1
      end
   end)
   return r
end


local isnan = {['.nan']=true, ['.NaN']=true, ['.NAN']=true}


--- Parse a `nan` token.
-- @tparam string value token
-- @treturn[1] nan not-a-number, if a valid token was recognized
-- @treturn[2] nil otherwise, nil
-- @usage maybe_nan = implicit.nan(value)
local function nan(value)
   if isnan[value] then
      return 0/0
   end
end


local isinf = {
   ['.inf']  = math.huge,  ['.Inf']  = math.huge,  ['.INF']  = math.huge,
   ['+.inf'] = math.huge,  ['+.Inf'] = math.huge,  ['+.INF'] = math.huge,
   ['-.inf'] = -math.huge, ['-.Inf'] = -math.huge, ['-.INF'] = -math.huge,
}


--- Parse a signed `inf` token.
-- @tparam string value token
-- @treturn[1] number plus/minus-infinity, if a valid token was recognized
-- @treturn[2] nil otherwise, nil
-- @usage maybe_inf = implicit.inf(value)
local function inf(value)
   return isinf[value]
end


--- Parse a floating point number token, such as '1e-3' or '-0.12'.
-- @tparam string value token
-- @treturn[1] number float equivalent, if a valid token was recognized
-- @treturn[2] nil otherwise, nil
-- @usage maybe_float = implicit.float(value)
local function float(value)
   local r = tonumber((gsub(value, '_', '')))
   if r and find(value, '[%.eE]') then
      return r
   end
end


--- Parse a sexagesimal float, such as '190:20:30.15'.
-- Useful for times and angles.
-- @tparam string value token
-- @treturn[1] number float equivalent, if a valid token was recognized
-- @treturn[2] nil otherwise, nil
-- @usage maybe_float = implicit.sexfloat(value)
local function sexfloat(value)
   local r
   gsub(value, '^([+-]?)([0-9]+:[0-5]?[0-9][:0-9]*)(%.[0-9]+)$',
      function(sign, rest, float)
         r = 0
         gsub(rest, '([0-9]+):?', function(digit)
            r = r * 60 + int(digit)
         end)
         r = r + tonumber(float)
         if sign == '-' then
            r = r * -1
         end
      end
   )
   return r
end


--- @export
return {
   binary = binary,
   decimal = decimal,
   float = float,
   hexadecimal = hexadecimal,
   inf = inf,
   nan = nan,
   null = null,
   octal = octal,
   sexagesimal = sexagesimal,
   sexfloat = sexfloat,
   bool = bool,
}
