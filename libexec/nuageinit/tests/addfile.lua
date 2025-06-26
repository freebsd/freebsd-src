#!/bin/libexec/flua

local n = require("nuage")
local lfs = require("lfs")

local f = {
	content = "plop"
}

local r, err = n.addfile(f, false)
if r or err ~= "No path provided for the file to write" then
	n.err("addfile should not accept a file to write without a path")
end

local function addfile_and_getres(file)
	local r, err = n.addfile(file, false)
	if not r then
		n.err(err)
	end
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	if not root then
		root = ""
	end
	local filepath = root .. file.path
	local resf = assert(io.open(filepath, "r"))
	local str = resf:read("*all")
	resf:close()
	return str
end

-- simple file
f.path="/tmp/testnuage"
local str = addfile_and_getres(f)
if str ~= f.content then
	n.err("Invalid file content")
end

-- the file is overwriten
f.content = "test"

str = addfile_and_getres(f)
if str ~= f.content then
	n.err("Invalid file content, not overwritten")
end

-- try to append now
f.content = "more"
f.append = true

str = addfile_and_getres(f)
if str ~= "test" .. f.content then
	n.err("Invalid file content, not appended")
end

-- base64
f.content = "YmxhCg=="
f.encoding = "base64"
f.append = false

str = addfile_and_getres(f)
if str ~= "bla\n" then
	n.err("Invalid file content, base64 decode")
end

-- b64
f.encoding = "b64"
str = addfile_and_getres(f)
if str ~= "bla\n" then
	n.err("Invalid file content, b64 decode")
	print("==>" .. str .. "<==")
end
