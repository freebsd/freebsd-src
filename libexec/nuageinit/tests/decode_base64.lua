#!/usr/libexec/flua
---
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2026 Baptiste Daroussin <bapt@FreeBSD.org>

local n = require("nuage")

-- decode_base64 is not exported, test via addfile

local function test_decode(input, expected)
	local r, err = n.addfile({
		content = input,
		encoding = "base64",
		path = "/tmp/nuage_test_b64"
	}, false)
	if not r then
		n.err(err)
	end
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	if not root then
		root = ""
	end
	local f = assert(io.open(root .. "/tmp/nuage_test_b64", "r"))
	local str = f:read("*all")
	f:close()
	if str ~= expected then
		n.err("base64 decode failed: expected '" .. expected
			.. "' got '" .. str .. "'")
	end
end

-- empty input
test_decode("", "")

-- single byte: 'a'
test_decode("YQ==", "a")

-- two bytes: 'ab'
test_decode("YWI=", "ab")

-- three bytes: 'abc'
test_decode("YWJj", "abc")

-- newline in base64
test_decode("YmxhCg==", "bla\n")

-- spaces should be ignored
test_decode("Y Q = =", "a")

-- b64 alias
local r, err = n.addfile({
	content = "YQ==",
	encoding = "b64",
	path = "/tmp/nuage_test_b64_b64"
}, false)
if not r then
	n.err("b64 encoding alias should work: " .. tostring(err))
end

os.exit(0)
