#!/usr/libexec/flua
---
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2026 Baptiste Daroussin <bapt@FreeBSD.org>

local n = require("nuage")

local root = os.getenv("NUAGE_FAKE_ROOTDIR")
if not root then
	root = ""
end

local function get_localbase()
	local f = io.popen("sysctl -in user.localbase 2> /dev/null")
	local lb = f:read("*l")
	f:close()
	if lb == nil or lb:len() == 0 then
		lb = "/usr/local"
	end
	return lb
end

local function read_doasconf()
	local path = root .. get_localbase() .. "/etc/doas.conf"
	local f = io.open(path, "r")
	if not f then
		return nil
	end
	local content = f:read("*a")
	f:close()
	return content
end

-- test with a single string rule with %u substitution
n.adddoas({ name = "testuser", doas = "permit persist %u as root" })
local content = read_doasconf()
if not content then
	n.err("doas.conf not created")
end
if content ~= "permit persist testuser as root\n" then
	n.err("unexpected doas.conf content with %u: '" .. content .. "'")
end

-- remove file for next test
os.remove(root .. get_localbase() .. "/etc/doas.conf")

-- test with a table of rules
n.adddoas({
	name = "testuser",
	doas = {
		"deny %u as foobar",
		"permit persist %u as root cmd whoami"
	}
})
content = read_doasconf()
if not content then
	n.err("doas.conf not created for table")
end
if content ~= "deny testuser as foobar\npermit persist testuser as root cmd whoami\n" then
	n.err("unexpected doas.conf content for table: '" .. content .. "'")
end

os.exit(0)
