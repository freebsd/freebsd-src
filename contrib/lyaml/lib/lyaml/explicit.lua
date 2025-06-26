-- LYAML parse explicit token values.
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

--- @module lyaml.explicit

local functional = require 'lyaml.functional'
local implicit = require 'lyaml.implicit'

local NULL = functional.NULL
local anyof = functional.anyof
local id = functional.id


local yn = {y=true, Y=true, n=false, N=false}


--- Parse the value following an explicit `!!bool` tag.
-- @function bool
-- @param value token
-- @treturn[1] bool boolean equivalent, if a valid value was recognized
-- @treturn[2] nil otherwise, nil
-- @usage maybe_bool = explicit.bool(tagarg)
local bool = anyof {
   implicit.bool,
   function(x) return yn[x] end,
}


--- Return a function that converts integer results to equivalent float.
-- @tparam function fn token parsing function
-- @treturn function new function that converts int results to float
-- @usage maybe_float = maybefloat(implicit.decimal)(tagarg)
local function maybefloat(fn)
   return function(...)
      local r = fn(...)
      if type(r) == 'number' then
         return r + 0.0
      end
   end
end


--- Parse the value following an explicit `!!float` tag.
-- @function float
-- @param value token
-- @treturn[1] number float equivalent, if a valid value was recognized
-- @treturn[2] nil otherwise, nil
-- @usage maybe_float = explicit.float(tagarg)
local float = anyof {
   implicit.float,
   implicit.nan,
   implicit.inf,
   maybefloat(implicit.octal),
   maybefloat(implicit.decimal),
   maybefloat(implicit.hexadecimal),
   maybefloat(implicit.binary),
   implicit.sexfloat,
}


--- Parse the value following an explicit `!!int` tag.
-- @function int
-- @param value token
-- @treturn[1] int integer equivalent, if a valid value was recognized
-- @treturn[2] nil otherwise, nil
-- @usage maybe_int = explicit.int(tagarg)
local int = anyof {
   implicit.octal,
   implicit.decimal,
   implicit.hexadecimal,
   implicit.binary,
   implicit.sexagesimal,
}


--- Parse an explicit `!!null` tag.
-- @treturn lyaml.null
-- @usage null = explicit.null(tagarg)
local function null()
   return NULL
end


--- Parse the value following an explicit `!!str` tag.
-- @function str
-- @tparam string value token
-- @treturn string *value* which was a string already
-- @usage tagarg = explicit.str(tagarg)
local str = id


--- @export
return {
   bool = bool,
   float = float,
   int = int,
   null = null,
   str = str,
}
