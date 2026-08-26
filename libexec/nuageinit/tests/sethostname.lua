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

local hostnamepath = root .. "/etc/rc.conf.d/hostname"

local function check_hostname(expected)
	local f = io.open(hostnamepath, "r")
	if not f then
		n.err("hostname file not found, expected: " .. expected)
	end
	local content = f:read("*a")
	f:close()
	local expected_content = "hostname=" .. n.shell_escape(expected) .. "\n"
	if content ~= expected_content then
		n.err("hostname mismatch: got '" .. content ..
		    "', expected '" .. expected_content .. "'")
	end
end

local function check_no_hostname()
	if io.open(hostnamepath, "r") then
		n.err("hostname file should not exist")
	end
end

-- nil hostname: no-op
n.sethostname(nil)
check_no_hostname()

-- Empty hostname: invalid
n.sethostname("")
check_no_hostname()

-- Hostname too long (>253 chars): invalid
n.sethostname(string.rep("a", 254))
check_no_hostname()

-- Invalid characters: invalid
n.sethostname("host;name")
check_no_hostname()

-- Starts with dot: invalid
n.sethostname(".hostname")
check_no_hostname()

-- Ends with hyphen: invalid
n.sethostname("hostname-")
check_no_hostname()

-- Label too long (>63 chars): invalid
n.sethostname(string.rep("a", 64) .. ".example.com")
check_no_hostname()

-- Label starts with hyphen: invalid
n.sethostname("myhost.-label.com")
check_no_hostname()

-- Valid simple hostname
n.sethostname("myhostname")
check_hostname("myhostname")

-- Final: set a valid hostname for the shell test
n.sethostname("myhostname")
