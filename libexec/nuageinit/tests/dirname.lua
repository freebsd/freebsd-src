#!/usr/libexec/flua
---
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2026 Baptiste Daroussin <bapt@FreeBSD.org>

local n = require("nuage")

print(n.dirname("/my/path/path1"))

-- relative path with no directory component: nil
if n.dirname("path") then
	n.err('Expecting nil for n.dirname("path")')
end

-- nil input: nil
if n.dirname() then
	n.err("Expecting nil for n.dirname")
end

-- root path: returns "/"
if n.dirname("/") ~= "/" then
	n.err('Expecting "/" for n.dirname("/"), got: ' .. tostring(n.dirname("/")))
end

-- top-level directory: returns "/"
if n.dirname("/foo") ~= "/" then
	n.err('Expecting "/" for n.dirname("/foo")')
end

-- deep path
if n.dirname("/foo/bar/baz") ~= "/foo/bar/" then
	n.err('Expecting "/foo/bar/" for n.dirname("/foo/bar/baz")')
end
