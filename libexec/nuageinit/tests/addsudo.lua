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

local function read_sudoers()
	local path = root .. get_localbase() .. "/etc/sudoers.d/90-nuageinit-users"
	local f = io.open(path, "r")
	if not f then
		return nil
	end
	local content = f:read("*a")
	f:close()
	return content
end

-- test with a single string rule
n.addsudo({ name = "testuser", sudo = "ALL=(ALL) NOPASSWD:ALL" })
local content = read_sudoers()
if not content then
	n.err("sudoers file not created")
end
if content ~= "testuser ALL=(ALL) NOPASSWD:ALL\n" then
	n.err("unexpected sudoers content for string rule: '" .. content .. "'")
end

-- remove file for next test
os.remove(root .. get_localbase() .. "/etc/sudoers.d/90-nuageinit-users")

-- test with a table of rules
n.addsudo({
	name = "testuser",
	sudo = { "ALL=(ALL) NOPASSWD:/usr/sbin/pw", "ALL=(ALL) ALL" }
})
content = read_sudoers()
if not content then
	n.err("sudoers file not created for table")
end
if content ~= "testuser ALL=(ALL) NOPASSWD:/usr/sbin/pw\ntestuser ALL=(ALL) ALL\n" then
	n.err("unexpected sudoers content for table: '" .. content .. "'")
end

os.exit(0)
