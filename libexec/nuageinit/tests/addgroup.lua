#!/usr/libexec/flua

local n = require("nuage")
if n.addgroup() then
	n.err("addgroup should not accept empty value")
end
if n.addgroup("plop") then
	n.err("addgroup should not accept empty value")
end
local gr = {}
gr.name = "impossible_groupname"
local res = n.addgroup(gr)
if not res then
	n.err("valid addgroup should return a path")
end
