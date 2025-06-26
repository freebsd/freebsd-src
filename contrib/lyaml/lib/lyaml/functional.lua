-- Minimal functional programming utilities.
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

--- @module lyaml.functional


--- `lyaml.null` value.
-- @table NULL
local NULL = setmetatable({}, {_type='LYAML null'})


--- `lyaml.null` predicate.
-- @param x operand
-- @treturn bool `true` if *x* is `lyaml.null`.
local function isnull(x)
   return(getmetatable(x) or {})._type == 'LYAML null'
end


--- Callable predicate.
-- @param x operand
-- @treturn bool `true` if *x* is a function has a __call metamethod
-- @usage r = iscallable(x) and x(...)
local function iscallable(x)
   if type(x) ~= 'function' then
      x =(getmetatable(x) or {}).__call
   end
   if type(x) == 'function' then
      return x
   end
end


--- Compose a function to try each callable with supplied args.
-- @tparam table fns list of functions to try
-- @treturn function a new function to call *...* functions, stopping
--    and returning the first non-nil result, if any
local function anyof(fns)
   return function(...)
      for _, fn in ipairs(fns) do
         if iscallable(fn) then
            local r = fn(...)
            if r ~= nil then
               return r
            end
         end
      end
   end
end


--- Return arguments unchanged.
-- @param ... arguments
-- @return *...*
local function id(...)
   return ...
end

--- @export
return {
   NULL = NULL,
   anyof = anyof,
   id = id,
   iscallable = iscallable,
   isnull = isnull,
}
