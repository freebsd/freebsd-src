#!/usr/libexec/flua

--[[
SPDX-License-Identifier: BSD-2-Clause-FreeBSD

Copyright (c) 2022 Stefan Esser <se@FreeBSD.org>

Generate a list of existing and required CONFLICTS_INSTALL lines
for all ports (limited to ports for which official packages are 
provided).

This script depends on the ports-mgmt/pkg-provides port for the list 
of files installed by all pre-built packages for the architecture
the script is run on.

The script generates a list of ports by running "pkg provides ." and 
a mapping from package base name to origin via "pkg rquery '%n %o'".

The existing CONFLICTS and CONFLICTS_INSTALL definitions are fetched
by "make -C $origin -V CONFLICTS -V CONFLICTS_INSTALL". This list is
only representative for the options configured for each port (i.e. 
if non-default options have been selected and registered, these may
lead to a non-default list of conflicts).

The script detects files used by more than one port, than lists by 
origin the existing definition and the list of package base names
that have been detected to cause install conflicts followed by the 
list of duplicate files separated by a hash character "#".

This script uses the "hidden" LUA interpreter in the FreeBSD base
systems and does not need any port except "pkg-provides" to be run.

The run-time on my system checking the ~32000 packages available 
for -CURRENT on amd64 is 150 seconds.
--]]

require "lfs"

local index_file = "/usr/ports/INDEX-14"

local function read_index ()
   local ORIGIN = {}

   local pipe = io.popen("pkg rquery '%n %o'")
   for line in pipe:lines() do
      local pkgbase, origin = string.match(line, "(%S+) (%S+)")
      ORIGIN[pkgbase] = origin
   end
   pipe:close()
   return ORIGIN
end

local function read_files()
   local FILES_TABLE = {}

   local pkgbase, version
   local pipe = io.popen("pkg provides .")
   for line in pipe:lines() do
      local label = string.sub(line, 1, 10)
      if label == "Name    : " then
	 name = string.sub(line, 11)
	 pkgbase, version = string.match(name, "(.*)-([^-]*)")
      elseif label == "          " or label == "Filename: " then
	 local file = string.sub(line, 11)
	 if file:sub(1, 10) == "usr/local/" then
	    file = file:sub(11)
	 else
	    file = "/" .. file
	 end
	 local t = FILES_TABLE[file] or {}
	 t[#t + 1] = pkgbase
	 FILES_TABLE[file] = t
      end
   end
   pipe:close()
   return FILES_TABLE
end

local PKG_PAIRS = {}

for file, pkgbases in pairs(read_files()) do
   if #pkgbases > 1 then
      for i = 1, #pkgbases -1 do
	 local pkg_i = pkgbases[i]
	 for j = i + 1, #pkgbases do
	    local pkg_j = pkgbases[j]
	    if pkg_i ~= pkg_j then
	       p1 = PKG_PAIRS[pkg_i] or {}
	       p2 = p1[pkg_j] or {}
	       p2[#p2 + 1] = file
	       p1[pkg_j] = p2
	       PKG_PAIRS[pkg_i] = p1
	    end
	 end
      end
   end
end

local CONFLICT_PKGS = {} 
local CONFLICT_FILES = {}

for pkg_i, p1 in pairs(PKG_PAIRS) do
   for pkg_j, p2 in pairs(p1) do
      CONFLICT_PKGS[pkg_i] = CONFLICT_PKGS[pkg_i] or {}
      CONFLICT_PKGS[pkg_j] = CONFLICT_PKGS[pkg_j] or {}
      CONFLICT_FILES[pkg_i] = CONFLICT_FILES[pkg_i] or {}
      CONFLICT_FILES[pkg_j] = CONFLICT_FILES[pkg_j] or {}
      table.insert(CONFLICT_PKGS[pkg_i], pkg_j)
      table.insert(CONFLICT_PKGS[pkg_j], pkg_i)
      for _, file in ipairs(p2) do
	 table.insert(CONFLICT_FILES[pkg_i], file)
	 table.insert(CONFLICT_FILES[pkg_j], file)
      end
   end
end

local function table_sorted_keys(t)
   result = {}
   for k, _ in pairs(t) do
      result[#result + 1] = k
   end
   table.sort(result)
   return result
end

local function table_sort_uniq(t)
   local result = {}
   local last

   table.sort(t)
   for _, entry in ipairs(t) do
      if entry ~= last then
	 last = entry
	 result[#result + 1] = entry
      end
   end
   return result
end

local ORIGIN = read_index()

local RESULT_PATTERN = {}

for pkg, pkgs in pairs(CONFLICT_PKGS) do
   local origin = ORIGIN[pkg]

   if origin then
      table.sort(pkgs)
      RESULT_PATTERN[origin] = table.concat(pkgs, " ")
   end
end

local FILE_LIST = {}

for pkg, files in pairs(CONFLICT_FILES) do
   local origin = ORIGIN[pkg]

   if origin then
      FILE_LIST[origin] = table.concat(table_sort_uniq(files), " ")
   end
end

for _, origin in ipairs(table_sorted_keys(RESULT_PATTERN)) do
   local pipe = io.popen("make -C /usr/ports/" .. origin .. " -V CONFLICTS -V CONFLICTS_INSTALL 2>/dev/null")
   local conflicts_table = {}
   local seen = {}
   for line in pipe:lines() do
      for word in line:gmatch("(%S*)%s?") do
	 if word ~= "" and not seen[word] then
	    table.insert(conflicts_table, word)
	    seen[word] = true
	 end
      end
   end
   pipe:close()
   table.sort(conflicts_table)
   conflicts_string = table.concat(conflicts_table, " ")
   local conflicts_new = RESULT_PATTERN[origin]
   if conflicts_string ~= conflicts_new then
      print("< " .. origin, conflicts_string)
      print("> " .. origin, conflicts_new .. " # " .. FILE_LIST[origin])
      print()
   end
end
