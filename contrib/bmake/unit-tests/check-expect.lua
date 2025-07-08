#!  /usr/bin/lua
-- $NetBSD: check-expect.lua,v 1.13 2025/04/13 09:29:32 rillig Exp $

--[[

usage: lua ./check-expect.lua *.mk

Check that the various 'expect' comments in the .mk files produce the
expected text in the corresponding .exp file.

# expect: <line>
        All of these lines must occur in the .exp file, in the same order as
        in the .mk file.

# expect-reset
        Search the following 'expect:' comments from the top of the .exp
        file again.

# expect[+-]offset: <message>
        Each message must occur in the .exp file and refer back to the
        source line in the .mk file.

# expect-not: <substring>
        The substring must not occur as part of any line of the .exp file.

# expect-not-matches: <pattern>
        The pattern (see https://lua.org/manual/5.4/manual.html#6.4.1)
        must not occur as part of any line of the .exp file.
]]


local had_errors = false
---@param fmt string
function print_error(fmt, ...)
  print(fmt:format(...))
  had_errors = true
end


---@return nil | string[]
local function load_lines(fname)
  local lines = {}

  local f = io.open(fname, "r")
  if f == nil then return nil end

  for line in f:lines() do
    table.insert(lines, line)
  end
  f:close()

  return lines
end


---@param exp_lines string[]
local function collect_lineno_diagnostics(exp_lines)
  ---@type table<string, string[]>
  local by_location = {}

  for _, line in ipairs(exp_lines) do
    ---@type string | nil, string, string
    local l_fname, l_lineno, l_msg =
      line:match('^make: ([^:]+):(%d+): (.*)')
    if l_fname ~= nil then
      local location = ("%s:%d"):format(l_fname, l_lineno)
      if by_location[location] == nil then
        by_location[location] = {}
      end
      table.insert(by_location[location], l_msg)
    end
  end

  return by_location
end


local function missing(by_location)
  ---@type {filename: string, lineno: number, location: string, message: string}[]
  local missing_expectations = {}

  for location, messages in pairs(by_location) do
    for _, message in ipairs(messages) do
      if message ~= "" and location:find(".mk:") then
        local filename, lineno = location:match("^(%S+):(%d+)$")
        table.insert(missing_expectations, {
          filename = filename,
          lineno = tonumber(lineno),
          location = location,
          message = message
        })
      end
    end
  end
  table.sort(missing_expectations, function(a, b)
    if a.filename ~= b.filename then
      return a.filename < b.filename
    end
    return a.lineno < b.lineno
  end)
  return missing_expectations
end


local function check_mk(mk_fname)
  local exp_fname = mk_fname:gsub("%.mk$", ".exp")
  local mk_lines = load_lines(mk_fname)
  local exp_lines = load_lines(exp_fname)
  if exp_lines == nil then return end
  local by_location = collect_lineno_diagnostics(exp_lines)
  local prev_expect_line = 0

  for mk_lineno, mk_line in ipairs(mk_lines) do

    for text in mk_line:gmatch("#%s*expect%-not:%s*(.*)") do
      local i = 1
      while i <= #exp_lines and not exp_lines[i]:find(text, 1, true) do
        i = i + 1
      end
      if i <= #exp_lines then
        print_error("error: %s:%d: %s must not contain '%s'",
          mk_fname, mk_lineno, exp_fname, text)
      end
    end

    for text in mk_line:gmatch("#%s*expect%-not%-matches:%s*(.*)") do
      local i = 1
      while i <= #exp_lines and not exp_lines[i]:find(text) do
        i = i + 1
      end
      if i <= #exp_lines then
        print_error("error: %s:%d: %s must not match '%s'",
          mk_fname, mk_lineno, exp_fname, text)
      end
    end

    for text in mk_line:gmatch("#%s*expect:%s*(.*)") do
      local i = prev_expect_line
      -- As of 2022-04-15, some lines in the .exp files contain trailing
      -- whitespace.  If possible, this should be avoided by rewriting the
      -- debug logging.  When done, the trailing gsub can be removed.
      -- See deptgt-phony.exp lines 14 and 15.
      while i < #exp_lines and text ~= exp_lines[i + 1]:gsub("^%s*", ""):gsub("%s*$", "") do
        i = i + 1
      end
      if i < #exp_lines then
        prev_expect_line = i + 1
      else
        print_error("error: %s:%d: '%s:%d+' must contain '%s'",
          mk_fname, mk_lineno, exp_fname, prev_expect_line + 1, text)
      end
    end
    if mk_line:match("^#%s*expect%-reset$") then
      prev_expect_line = 0
    end

    ---@param text string
    for offset, text in mk_line:gmatch("#%s*expect([+%-]%d+):%s*(.*)") do
      local location = ("%s:%d"):format(mk_fname, mk_lineno + tonumber(offset))

      local found = false
      if by_location[location] ~= nil then
        for i, message in ipairs(by_location[location]) do
          if message == text then
            by_location[location][i] = ""
            found = true
            break
          elseif message ~= "" then
            print_error("error: %s:%d: out-of-order '%s'",
              mk_fname, mk_lineno, message)
          end
        end
      end

      if not found then
        print_error("error: %s:%d: %s must contain '%s'",
          mk_fname, mk_lineno, exp_fname, text)
      end
    end
  end

  for _, m in ipairs(missing(by_location)) do
    print_error("missing: %s: # expect+1: %s", m.location, m.message)
  end
end

for _, fname in ipairs(arg) do
  check_mk(fname)
end
os.exit(not had_errors)
