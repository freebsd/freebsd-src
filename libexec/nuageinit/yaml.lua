---
-- SPDX-License-Identifier: MIT
--
-- Copyright (c) 2017 Dominic Letz dominicletz@exosite.com

local table_print_value
table_print_value = function(value, indent, done)
  indent = indent or 0
  done = done or {}
  if type(value) == "table" and not done [value] then
    done [value] = true

    local list = {}
    for key in pairs (value) do
      list[#list + 1] = key
    end
    table.sort(list, function(a, b) return tostring(a) < tostring(b) end)
    local last = list[#list]

    local rep = "{\n"
    local comma
    for _, key in ipairs (list) do
      if key == last then
        comma = ''
      else
        comma = ','
      end
      local keyRep
      if type(key) == "number" then
        keyRep = key
      else
        keyRep = string.format("%q", tostring(key))
      end
      rep = rep .. string.format(
        "%s[%s] = %s%s\n",
        string.rep(" ", indent + 2),
        keyRep,
        table_print_value(value[key], indent + 2, done),
        comma
      )
    end

    rep = rep .. string.rep(" ", indent) -- indent it
    rep = rep .. "}"

    done[value] = false
    return rep
  elseif type(value) == "string" then
    return string.format("%q", value)
  else
    return tostring(value)
  end
end

local table_print = function(tt)
  print('return '..table_print_value(tt))
end

local table_clone = function(t)
  local clone = {}
  for k,v in pairs(t) do
    clone[k] = v
  end
  return clone
end

local string_trim = function(s, what)
  what = what or " "
  return s:gsub("^[" .. what .. "]*(.-)["..what.."]*$", "%1")
end

local push = function(stack, item)
  stack[#stack + 1] = item
end

local pop = function(stack)
  local item = stack[#stack]
  stack[#stack] = nil
  return item
end

local context = function (str)
  if type(str) ~= "string" then
    return ""
  end

  str = str:sub(0,25):gsub("\n","\\n"):gsub("\"","\\\"");
  return ", near \"" .. str .. "\""
end

local Parser = {}
function Parser.new (self, tokens)
  self.tokens = tokens
  self.parse_stack = {}
  self.refs = {}
  self.current = 0
  return self
end

local exports = {version = "1.2"}

local word = function(w) return "^("..w..")([%s$%c])" end

local tokens = {
  {"comment",   "^#[^\n]*"},
  {"indent",    "^\n( *)"},
  {"space",     "^ +"},
  {"true",      word("enabled"),  const = true, value = true},
  {"true",      word("true"),     const = true, value = true},
  {"true",      word("yes"),      const = true, value = true},
  {"true",      word("on"),      const = true, value = true},
  {"false",     word("disabled"), const = true, value = false},
  {"false",     word("false"),    const = true, value = false},
  {"false",     word("no"),       const = true, value = false},
  {"false",     word("off"),      const = true, value = false},
  {"null",      word("null"),     const = true, value = nil},
  {"null",      word("Null"),     const = true, value = nil},
  {"null",      word("NULL"),     const = true, value = nil},
  {"null",      word("~"),        const = true, value = nil},
  {"id",    "^\"([^\"]-)\" *(:[%s%c])"},
  {"id",    "^'([^']-)' *(:[%s%c])"},
  {"string",    "^\"([^\"]-)\"",  force_text = true},
  {"string",    "^'([^']-)'",    force_text = true},
  {"timestamp", "^(%d%d%d%d)-(%d%d?)-(%d%d?)%s+(%d%d?):(%d%d):(%d%d)%s+(%-?%d%d?):(%d%d)"},
  {"timestamp", "^(%d%d%d%d)-(%d%d?)-(%d%d?)%s+(%d%d?):(%d%d):(%d%d)%s+(%-?%d%d?)"},
  {"timestamp", "^(%d%d%d%d)-(%d%d?)-(%d%d?)%s+(%d%d?):(%d%d):(%d%d)"},
  {"timestamp", "^(%d%d%d%d)-(%d%d?)-(%d%d?)%s+(%d%d?):(%d%d)"},
  {"timestamp", "^(%d%d%d%d)-(%d%d?)-(%d%d?)%s+(%d%d?)"},
  {"timestamp", "^(%d%d%d%d)-(%d%d?)-(%d%d?)"},
  {"doc",       "^%-%-%-[^%c]*"},
  {",",         "^,"},
  {"string",    "^%b{} *[^,%c]+", noinline = true},
  {"{",         "^{"},
  {"}",         "^}"},
  {"string",    "^%b[] *[^,%c]+", noinline = true},
  {"[",         "^%["},
  {"]",         "^%]"},
  {"-",         "^%-", noinline = true},
  {":",         "^:"},
  {"pipe",      "^(|)(%d*[+%-]?)", sep = "\n"},
  {"pipe",      "^(>)(%d*[+%-]?)", sep = " "},
  {"id",        "^([%w][%w %-_]*)(:[%s%c])"},
  {"string",    "^[^%c]+", noinline = true},
  {"string",    "^[^,%]}%c ]+"}
};
exports.tokenize = function (str)
  local token
  local row = 0
  local ignore
  local indents = 0
  local lastIndents
  local stack = {}
  local indentAmount = 0
  local inline = false
  str = str:gsub("\r\n","\010")

  while #str > 0 do
    for i in ipairs(tokens) do
      local captures = {}
      if not inline or tokens[i].noinline == nil then
        captures = {str:match(tokens[i][2])}
      end

      if #captures > 0 then
        captures.input = str:sub(0, 25)
        token = table_clone(tokens[i])
        token[2] = captures
        local str2 = str:gsub(tokens[i][2], "", 1)
        token.raw = str:sub(1, #str - #str2)
        str = str2

        if token[1] == "{" or token[1] == "[" then
          inline = true
        elseif token.const then
          -- Since word pattern contains last char we're re-adding it
          str = token[2][2] .. str
          token.raw = token.raw:sub(1, #token.raw - #token[2][2])
        elseif token[1] == "id" then
          -- Since id pattern contains last semi-colon we're re-adding it
          str = token[2][2] .. str
          token.raw = token.raw:sub(1, #token.raw - #token[2][2])
          -- Trim
          token[2][1] = string_trim(token[2][1])
        elseif token[1] == "string" then
          -- Finding numbers
          local snip = token[2][1]
          if not token.force_text then
            if snip:match("^(-?%d+%.%d+)$") or snip:match("^(-?%d+)$") then
              token[1] = "number"
            end
          end

        elseif token[1] == "comment" then
          ignore = true;
        elseif token[1] == "indent" then
          row = row + 1
          inline = false
          lastIndents = indents
          if indentAmount == 0 then
            indentAmount = #token[2][1]
          end

          if indentAmount ~= 0 then
            indents = (#token[2][1] / indentAmount);
          else
            indents = 0
          end

          if indents == lastIndents then
            ignore = true;
          elseif indents > lastIndents + 2 then
            error("SyntaxError: invalid indentation, got " .. tostring(indents)
              .. " instead of " .. tostring(lastIndents) .. context(token[2].input))
          elseif indents > lastIndents + 1 then
            push(stack, token)
          elseif indents < lastIndents then
            local input = token[2].input
            token = {"dedent", {"", input = ""}}
            token.input = input
            while lastIndents > indents + 1 do
              lastIndents = lastIndents - 1
              push(stack, token)
            end
          end
        end -- if token[1] == XXX
        token.row = row
        break
      end -- if #captures > 0
    end

    if not ignore then
      if token then
        push(stack, token)
        token = nil
      else
        error("SyntaxError " .. context(str))
      end
    end

    ignore = false;
  end

  return stack
end

Parser.peek = function (self, offset)
  offset = offset or 1
  return self.tokens[offset + self.current]
end

Parser.advance = function (self)
  self.current = self.current + 1
  return self.tokens[self.current]
end

Parser.advanceValue = function (self)
  return self:advance()[2][1]
end

Parser.accept = function (self, type)
  if self:peekType(type) then
    return self:advance()
  end
end

Parser.expect = function (self, type, msg)
  return self:accept(type) or
    error(msg .. context(self:peek()[1].input))
end

Parser.expectDedent = function (self, msg)
  return self:accept("dedent") or (self:peek() == nil) or
    error(msg .. context(self:peek()[2].input))
end

Parser.peekType = function (self, val, offset)
  return self:peek(offset) and self:peek(offset)[1] == val
end

Parser.ignore = function (self, items)
  local advanced
  repeat
    advanced = false
    for _,v in pairs(items) do
      if self:peekType(v) then
        self:advance()
        advanced = true
      end
    end
  until advanced == false
end

Parser.ignoreSpace = function (self)
  self:ignore{"space"}
end

Parser.ignoreWhitespace = function (self)
  self:ignore{"space", "indent", "dedent"}
end

Parser.parse = function (self)

  local ref = nil
  if self:peekType("string") and not self:peek().force_text then
    local char = self:peek()[2][1]:sub(1,1)
    if char == "&" then
      ref = self:peek()[2][1]:sub(2)
      self:advanceValue()
      self:ignoreSpace()
    elseif char == "*" then
      ref = self:peek()[2][1]:sub(2)
      return self.refs[ref]
    end
  end

  local result
  local c = {
    indent = self:accept("indent") and 1 or 0,
    token = self:peek()
  }
  push(self.parse_stack, c)

  if c.token[1] == "doc" then
    result = self:parseDoc()
  elseif c.token[1] == "-" then
    result = self:parseList()
  elseif c.token[1] == "{" then
    result = self:parseInlineHash()
  elseif c.token[1] == "[" then
    result = self:parseInlineList()
  elseif c.token[1] == "id" then
    result = self:parseHash()
  elseif c.token[1] == "string" then
    result = self:parseString("\n")
  elseif c.token[1] == "timestamp" then
    result = self:parseTimestamp()
  elseif c.token[1] == "number" then
    result = tonumber(self:advanceValue())
  elseif c.token[1] == "pipe" then
    result = self:parsePipe()
  elseif c.token.const == true then
    self:advanceValue();
    result = c.token.value
  else
    error("ParseError: unexpected token '" .. c.token[1] .. "'" .. context(c.token.input))
  end

  pop(self.parse_stack)
  while c.indent > 0 do
    c.indent = c.indent - 1
    local term = "term "..c.token[1]..": '"..c.token[2][1].."'"
    self:expectDedent("last ".. term .." is not properly dedented")
  end

  if ref then
    self.refs[ref] = result
  end
  return result
end

Parser.parseDoc = function (self)
  self:accept("doc")
  return self:parse()
end

Parser.inline = function (self)
  local current = self:peek(0)
  if not current then
    return {}, 0
  end

  local inline = {}
  local i = 0

  while self:peek(i) and not self:peekType("indent", i) and current.row == self:peek(i).row do
    inline[self:peek(i)[1]] = true
    i = i - 1
  end
  return inline, -i
end

Parser.isInline = function (self)
  local _, i = self:inline()
  return i > 0
end

Parser.parent = function(self, level)
  level = level or 1
  return self.parse_stack[#self.parse_stack - level]
end

Parser.parentType = function(self, type, level)
  return self:parent(level) and self:parent(level).token[1] == type
end

Parser.parseString = function (self)
  if self:isInline() then
    local result = self:advanceValue()

    --[[
      - a: this looks
        flowing: but is
        no: string
    --]]
    local types = self:inline()
    if types["id"] and types["-"] then
      if not self:peekType("indent") or not self:peekType("indent", 2) then
        return result
      end
    end

    --[[
      a: 1
      b: this is
        a flowing string
        example
      c: 3
    --]]
    if self:peekType("indent") then
      self:expect("indent", "text block needs to start with indent")
      local addtl = self:accept("indent")

      result = result .. "\n" .. self:parseTextBlock("\n")

      self:expectDedent("text block ending dedent missing")
      if addtl then
        self:expectDedent("text block ending dedent missing")
      end
    end
    return result
  else
    --[[
      a: 1
      b:
        this is also
        a flowing string
        example
      c: 3
    --]]
    return self:parseTextBlock("\n")
  end
end

Parser.parsePipe = function (self)
  local pipe = self:expect("pipe")
  self:expect("indent", "text block needs to start with indent")
  local result = self:parseTextBlock(pipe.sep)
  self:expectDedent("text block ending dedent missing")
  return result
end

Parser.parseTextBlock = function (self, sep)
  local token = self:advance()
  local result = string_trim(token.raw, "\n")
  local indents = 0
  while self:peek() ~= nil and ( indents > 0 or not self:peekType("dedent") ) do
    local newtoken = self:advance()
    while token.row < newtoken.row do
      result = result .. sep
      token.row = token.row + 1
    end
    if newtoken[1] == "indent" then
      indents = indents + 1
    elseif newtoken[1] == "dedent" then
      indents = indents - 1
    else
      result = result .. string_trim(newtoken.raw, "\n")
    end
  end
  return result
end

Parser.parseHash = function (self, hash)
  hash = hash or {}
  local indents = 0

  if self:isInline() then
    local id = self:advanceValue()
    self:expect(":", "expected semi-colon after id")
    self:ignoreSpace()
    if self:accept("indent") then
      indents = indents + 1
      hash[id] = self:parse()
    else
      hash[id] = self:parse()
      if self:accept("indent") then
        indents = indents + 1
      end
    end
    self:ignoreSpace();
  end

  while self:peekType("id") do
    local id = self:advanceValue()
    self:expect(":","expected semi-colon after id")
    self:ignoreSpace()
    hash[id] = self:parse()
    self:ignoreSpace();
  end

  while indents > 0 do
    self:expectDedent("expected dedent")
    indents = indents - 1
  end

  return hash
end

Parser.parseInlineHash = function (self)
  local id
  local hash = {}
  local i = 0

  self:accept("{")
  while not self:accept("}") do
    self:ignoreSpace()
    if i > 0 then
      self:expect(",","expected comma")
    end

    self:ignoreWhitespace()
    if self:peekType("id") then
      id = self:advanceValue()
      if id then
        self:expect(":","expected semi-colon after id")
        self:ignoreSpace()
        hash[id] = self:parse()
        self:ignoreWhitespace()
      end
    end

    i = i + 1
  end
  return hash
end

Parser.parseList = function (self)
  local list = {}
  while self:accept("-") do
    self:ignoreSpace()
    list[#list + 1] = self:parse()

    self:ignoreSpace()
  end
  return list
end

Parser.parseInlineList = function (self)
  local list = {}
  local i = 0
  self:accept("[")
  while not self:accept("]") do
    self:ignoreSpace()
    if i > 0 then
      self:expect(",","expected comma")
    end

    self:ignoreSpace()
    list[#list + 1] = self:parse()
    self:ignoreSpace()
    i = i + 1
  end

  return list
end

Parser.parseTimestamp = function (self)
  local capture = self:advance()[2]

  return os.time{
    year  = capture[1],
    month = capture[2],
    day   = capture[3],
    hour  = capture[4] or 0,
    min   = capture[5] or 0,
    sec   = capture[6] or 0,
    isdst = false,
  } - os.time{year=1970, month=1, day=1, hour=8}
end

exports.eval = function (str)
  return Parser:new(exports.tokenize(str)):parse()
end

exports.dump = table_print

return exports
