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

local sshd_config = root .. "/etc/ssh/sshd_config"

local function setup(content)
	local dir = root .. "/etc/ssh"
	n.mkdir_p(dir)
	local f = assert(io.open(sshd_config, "w"))
	f:write(content)
	f:close()
end

local function read_config()
	local f = assert(io.open(sshd_config, "r"))
	local content = f:read("*a")
	f:close()
	return content
end

-- Key not found: appended
setup("SomeOtherKey yes\n")
n.update_sshd_config("PasswordAuthentication", "yes")
if read_config() ~= "SomeOtherKey yes\nPasswordAuthentication yes\n" then
	n.err("Key not found: should be appended")
end

-- Key with same value: no change
setup("PasswordAuthentication yes\n")
n.update_sshd_config("PasswordAuthentication", "yes")
if read_config() ~= "PasswordAuthentication yes\n" then
	n.err("Same value: should not change")
end

-- Key with different value: changed
setup("PasswordAuthentication no\n")
n.update_sshd_config("PasswordAuthentication", "yes")
if read_config() ~= "PasswordAuthentication yes\n" then
	n.err("Different value: should change")
end

-- Key with comment
setup("PasswordAuthentication no # keep this\n")
n.update_sshd_config("PasswordAuthentication", "yes")
if read_config() ~= "PasswordAuthentication yes\n" then
	n.err("Comment stripped: '" .. read_config() .. "'")
end

-- Case insensitive key matching
setup("passwordauthentication no\n")
n.update_sshd_config("PasswordAuthentication", "yes")
if read_config() ~= "PasswordAuthentication yes\n" then
	n.err("Case insensitive matching failed")
end

-- Extra spaces
setup("   PasswordAuthentication   no   \n")
n.update_sshd_config("PasswordAuthentication", "yes")
if read_config() ~= "PasswordAuthentication yes\n" then
	n.err("Extra spaces handling failed: '" .. read_config() .. "'")
end

-- File does not exist: should be created with key/value
os.remove(sshd_config)
n.update_sshd_config("PasswordAuthentication", "yes")
if read_config() ~= "PasswordAuthentication yes\n" then
	n.err("Missing file: should create: '" .. read_config() .. "'")
end

os.exit(0)
