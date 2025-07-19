#!  /usr/bin/lua
-- $NetBSD: check-expect.lua,v 1.17 2025/07/01 05:03:18 rillig Exp $

--[[

usage: lua ./check-expect.lua *.mk

Check that the various 'expect' comments in the .mk files produce the
expected text in the corresponding .exp file.

# expect: <line>
        Each <line> must occur in the .exp file.
        The order in the .mk file must be the same as in the .exp file.

# expect[+-]offset: <message>
        Each <message> must occur in the .exp file and refer back to the
        source line in the .mk file.
        Each such line in the .exp file must have a corresponding expect line
        in the .mk file.
        The order in the .mk file must be the same as in the .exp file.

# expect-reset
        Search the following "expect:" and "expect[+-]offset:" comments
        from the top of the .exp file again.

# expect-not: <substring>
        The <substring> must not occur as part of any line in the .exp file.

# expect-not-matches: <pattern>
        The <pattern> (see https://lua.org/manual/5.4/manual.html#6.4.1)
        must not occur as part of any line in the .exp file.
]]


local had_errors = false
---@param fmt string
local function print_error(fmt, ...)
  print(fmt:format(...))
  had_errors = true
end


---@return nil | string[]
local function load_lines(fname)
  local lines = {}

  local f = io.open(fname, "r")
  if f == nil then
    return nil
  end

  for line in f:lines() do
    table.insert(lines, line)
  end
  f:close()

  return lines
end


--- @shape ExpLine
--- @field filename string | nil
--- @field lineno number | nil
--- @field text string


--- @param lines string[]
--- @return ExpLine[]
local function parse_exp(lines)
  local exp_lines = {}
  for _, line in ipairs(lines) do
    local l_filename, l_lineno, l_text =
      line:match('^make: ([^:]+%.mk):(%d+):%s+(.*)')
    if not l_filename then
      l_text = line
    end
    l_text = l_text:gsub("^%s+", ""):gsub("%s+$", "")
    table.insert(exp_lines, {
      filename = l_filename,
      lineno = tonumber(l_lineno),
      text = l_text,
    })
  end
  return exp_lines
end

---@param exp_lines ExpLine[]
local function detect_missing_expect_lines(exp_fname, exp_lines, s, e)
  for i = s, e do
    local exp_line = exp_lines[i]
    if exp_line.filename then
      print_error("error: %s:%d requires in %s:%d: # expect+1: %s",
        exp_fname, i, exp_line.filename, exp_line.lineno, exp_line.text)
    end
  end
end

local function check_mk(mk_fname)
  local exp_fname = mk_fname:gsub("%.mk$", ".exp")
  local mk_lines = load_lines(mk_fname)
  local exp_raw_lines = load_lines(exp_fname)
  if exp_raw_lines == nil then
    return
  end
  local exp_lines = parse_exp(exp_raw_lines)

  local exp_it = 1

  for mk_lineno, mk_line in ipairs(mk_lines) do

    local function match(pattern, action)
      local _, n = mk_line:gsub(pattern, action)
      if n > 0 then
        match = function() end
      end
    end

    match("^#%s+expect%-not:%s*(.*)", function(text)
      for exp_lineno, exp_line in ipairs(exp_lines) do
        if exp_line.text:find(text, 1, true) then
          print_error("error: %s:%d: %s:%d must not contain '%s'",
            mk_fname, mk_lineno, exp_fname, exp_lineno, text)
        end
      end
    end)

    match("^#%s+expect%-not%-matches:%s*(.*)", function(pattern)
      for exp_lineno, exp_line in ipairs(exp_lines) do
        if exp_line.text:find(pattern) then
          print_error("error: %s:%d: %s:%d must not match '%s'",
            mk_fname, mk_lineno, exp_fname, exp_lineno, pattern)
        end
      end
    end)

    match("^#%s+expect:%s*(.*)", function(text)
      local i = exp_it
      while i <= #exp_lines and text ~= exp_lines[i].text do
        i = i + 1
      end
      if i <= #exp_lines then
        detect_missing_expect_lines(exp_fname, exp_lines, exp_it, i - 1)
        exp_lines[i].text = ""
        exp_it = i + 1
      else
        print_error("error: %s:%d: '%s:%d+' must contain '%s'",
          mk_fname, mk_lineno, exp_fname, exp_it, text)
      end
    end)

    match("^#%s+expect%-reset$", function()
      exp_it = 1
    end)

    match("^#%s+expect([+%-]%d+):%s*(.*)", function(offset, text)
      local msg_lineno = mk_lineno + tonumber(offset)

      local i = exp_it
      while i <= #exp_lines and text ~= exp_lines[i].text do
        i = i + 1
      end

      if i <= #exp_lines and exp_lines[i].lineno == msg_lineno then
        detect_missing_expect_lines(exp_fname, exp_lines, exp_it, i - 1)
        exp_lines[i].text = ""
        exp_it = i + 1
      elseif i <= #exp_lines then
        print_error("error: %s:%d: expect%+d must be expect%+d",
          mk_fname, mk_lineno, tonumber(offset),
          exp_lines[i].lineno - mk_lineno)
      else
        print_error("error: %s:%d: %s:%d+ must contain '%s'",
          mk_fname, mk_lineno, exp_fname, exp_it, text)
      end
    end)

    match("^#%s+expect[+%-:]", function()
      print_error("error: %s:%d: invalid \"expect\" line: %s",
        mk_fname, mk_lineno, mk_line)
    end)

  end
  detect_missing_expect_lines(exp_fname, exp_lines, exp_it, #exp_lines)
end

for _, fname in ipairs(arg) do
  check_mk(fname)
end
os.exit(not had_errors)
