#!/usr/bin/env lua5.1

local lfs = require "lfs"

local en_stat = assert(lfs.attributes "!NetSurf/Resources/en/Messages")
local language = { }
local sorted = { }

io.stderr:write("loading non-en languages...\n");

for dir in lfs.dir "!NetSurf/Resources" do
   local path = "!NetSurf/Resources/" .. dir
   if dir ~= "en" and lfs.attributes(path .. "/Messages") then
      local f = io.open(path .. "/Messages", "r")
      local c = 0
      io.stderr:write(dir, ":")
      language[dir] = { }
      sorted[#sorted + 1] = dir
      for l in f:lines() do
	 if l:sub(1, 1) ~= "#" then
	    local tag, msg = l:match "^([^:]*):(.*)$"
	    if tag then
	       language[dir][tag] = msg
	       c = c + 1
	    end
	 end
      end
      f:close()
      io.stderr:write(tostring(c), " entries.\n")
   end
end

table.sort(sorted)

io.stderr:write("working through en...\n")

local manipulators = {
   { "^(ami.*)", "ami.%1" },
   { "^(gtk.*)", "gtk.%1" },
   { "^(Help.*)", "ro.%1" },
   { "^(ARexx.*)", "ami.%1" },

   { "^(.*)$", "all.%1" } -- must be last
}

local function manipulate_tag(t)
   for _, m in ipairs(manipulators) do
      local r, s = t:gsub(m[1], m[2])
      if s > 0 then return r end
   end
   return t
end

local f = io.open("!NetSurf/Resources/en/Messages", "r")

for l in f:lines() do
   if l:sub(1,1) == "#" then
      print(l)
   else
      local tag, msg = l:match "^([^:]*):(.*)$"
      if not tag  then
	 print(l)
      else
	 local mtag = manipulate_tag(tag)
	 io.stdout:write("en.", mtag, ":", msg, "\n")
	 for _, langname in ipairs(sorted) do
	    local trans = language[langname][tag]
	    if not trans then
	       io.stderr:write("*** language ", langname, " lacks translation for ", mtag, "/", tag, "\n")
	       trans = msg
	    end
	    io.stdout:write(langname, ".", mtag, ":", trans, "\n")
	    language[langname][tag] = nil
	 end
      end
   end
end

for _, langname in ipairs(sorted) do
   for tag in pairs(language[langname]) do
      io.stderr:write("*** language ", langname, " contains orphan tag ", tag, "\n")
   end
end