#!/usr/libexec/flua

local n = require("nuage")

if n.adduser() then
	n.err("adduser should not accept empty value")
end
if n.adduser("plop") then
	n.err("adduser should not accept empty value")
end
local pw = {}
pw.name = "impossible_username"
local res = n.adduser(pw)
if not res then
	n.err("valid adduser should return a path")
end
