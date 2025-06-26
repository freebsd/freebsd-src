--[[
 LYAML binding for Lua 5.1, 5.2, 5.3 & 5.4
 Copyright (C) 2013-2022 Gary V. Vaughan
]]

do
   local std = require 'specl.std'
   local spawn = require 'specl.shell'.spawn
   local objdir = spawn('./build-aux/luke --value=objdir').output


   package.path = std.package.normalize(
      './lib/?.lua',
      './lib/?/init.lua',
      package.path
   )
   package.cpath = std.package.normalize(
      './' .. objdir:match("^objdir='(.*)'") .. '/?.so',
      './' .. objdir:match("^objdir='(.*)'") .. '/?.dll',
      package.cpath
   )
end

local hell = require 'specl.shell'


yaml = require 'yaml'

BOM = string.char(254, 255) -- UTF-16 Byte Order Mark

-- Allow use of bare 'pack' and 'unpack' even in Lua > 5.2.
pack = table.pack or function(...) return {n = select('#', ...), ...} end
unpack = table.unpack or unpack
list = pack


function dump(e)
   print(std.string.prettytostring(e))
end


function github_issue(n)
  return 'see http://github.com/gvvaughan/lyaml/issues/' .. tostring(n)
end


-- Output a list of event tables to the given emitter.
function emitevents(emitter, list)
   for _, v in ipairs(list) do
      if type(v) == 'string' then
         ok, msg = emitter.emit {type=v}
      elseif type(v) == 'table' then
         ok, msg = emitter.emit(v)
      else
         error 'expected table or string argument'
      end
 
      if not ok then
         error(msg)
      elseif ok and msg then
         return msg
      end
   end
end


-- Create a new emitter and send STREAM_START, listed events and STREAM_END.
function emit(list)
   local emitter = yaml.emitter()
   emitter.emit {type='STREAM_START'}
   emitevents(emitter, list)
   local _, msg = emitter.emit {type='STREAM_END'}
   return msg
end


-- Create a new parser for STR, and consume the first N events.
function consume(n, str)
   local e = yaml.parser(str)
   for n = 1, n do
      e()
   end
   return e
end


-- Return a new table with only elements of T that have keys listed
-- in the following arguments.
function filter(t, ...)
   local u = {}
   for _, k in ipairs {...} do
      u[k] = t[k]
   end
   return u
end


function iscallable(x)
   return type(x) == 'function' or type((getmetatable(x) or {}).__call) == 'function'
end


local function mkscript(code)
   local f = os.tmpname()
   local h = io.open(f, 'w')
   -- TODO: Move this into specl, or expose arguments so that we can
   --          turn this on and off based on specl `--coverage` arg.
   h:write "pcall(require, 'luacov')"
   h:write(code)
   h:close()
   return f
end


-- Allow user override of LUA binary used by hell.spawn, falling
-- back to environment PATH search for 'lua' if nothing else works.
local LUA = os.getenv 'LUA' or 'lua'


--- Run some Lua code with the given arguments and input.
-- @string code valid Lua code
-- @tparam[opt={}] string|table arg single argument, or table of
--    arguments for the script invocation.
-- @string[opt] stdin standard input contents for the script process
-- @treturn specl.shell.Process|nil status of resulting process if
--    execution was successful, otherwise nil
function luaproc(code, arg, stdin)
   local f = mkscript(code)
   if type(arg) ~= 'table' then arg = {arg} end
   local cmd = {LUA, f, unpack(arg)}
   -- inject env and stdin keys separately to avoid truncating `...` in
   -- cmd constructor
   cmd.env = { LUA_PATH=package.path, LUA_INIT='', LUA_INIT_5_2='' }
   cmd.stdin = stdin
   local proc = hell.spawn(cmd)
   os.remove(f)
   return proc
end


local function tabulate_output(code)
   local proc = luaproc(code)
   if proc.status ~= 0 then return error(proc.errout) end
   local r = {}
   proc.output:gsub('(%S*)[%s]*',
      function(x)
         if x ~= '' then r[x] = true end
      end)
   return r
end


--- Show changes to tables wrought by a require statement.
-- There are a few modes to this function, controlled by what named
-- arguments are given.   Lists new keys in T1 after `require "import"`:
--
--       show_apis {added_to=T1, by=import}
--
-- @tparam table argt one of the combinations above
-- @treturn table a list of keys according to criteria above
function show_apis(argt)
   return tabulate_output([[
      local before, after = {}, {}
      for k in pairs(]] .. argt.added_to .. [[) do
         before[k] = true
      end

      local M = require ']] .. argt.by .. [['
      for k in pairs(]] .. argt.added_to .. [[) do
         after[k] = true
      end

      for k in pairs(after) do
         if not before[k] then print(k) end
      end
   ]])
end



--[[ ========= ]]--
--[[ Call Spy. ]]--
--[[ ========= ]]--


spy = function(fn)
   return setmetatable({}, {
      __call = function(self, ...)
         self[#self + 1] = list(...)
         return fn(...)
      end,
   })
end


do
   --[[ ================ ]]--
   --[[ Custom matchers. ]]--
   --[[ ================ ]]--

   local matchers = require 'specl.matchers'
   local eqv = require 'specl.std'.operator.eqv
   local str = require 'specl.std'.string.tostring

   local Matcher, matchers = matchers.Matcher, matchers.matchers
   local concat = table.concat


   matchers.be_called_with = Matcher {
      function(self, actual, expected)
         for i,v in ipairs(expected or {}) do
            if not eqv(actual[i], v) then
               return false
            end
         end
         return true
      end,

      actual = 'argmuents',

      format_expect = function(self, expect)
         return ' arguments (' .. str(expect) .. '), '
      end,
   }

   matchers.be_callable = Matcher {
      function(self, actual, _)
         return iscallable(actual)
      end,

      actual = 'callable',

      format_expect = function(self, expect)
         return ' callable, '
      end,
   }

   matchers.be_falsey = Matcher {
      function(self, actual, _)
         return not actual and true or false
      end,

      actual = 'falsey',

      format_expect = function(self, expect)
         return ' falsey, '
      end,
   }

   matchers.be_truthy = Matcher {
      function(self, actual, _)
         return actual and true or false
      end,

      actual = 'truthy',

      format_expect = function(self, expect)
         return ' truthy, '
      end,
   }

   matchers.have_type = Matcher {
      function(self, actual, expected)
         return type(actual) == expected or (getmetatable(actual) or {})._type == expected
      end,

      actual = 'type',

      format_expect = function(self, expect)
         local article = 'a'
         if match(expect, '^[aehiou]') then
            article = 'an'
         end
         return concat{' ', article, ' ', expect, ', '}
      end
   }
end
