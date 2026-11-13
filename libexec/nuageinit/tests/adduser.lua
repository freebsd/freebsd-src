#!/usr/libexec/flua

local n = require("nuage")

if n.adduser() then
	n.err("adduser should not accept empty value")
end
if n.adduser("plop") then
	n.err("adduser accepted a string (not table)")
end
local pw = {}
pw.name = "impossible_username"
local res = n.adduser(pw)
if not res then
	n.err("valid adduser should return a path")
end

pw = {}
pw.name = "underscore_"
-- Call adduser twice to test both addding and looking up an existing user.
for i = 1, 2 do
	local res = n.adduser(pw)
	if not res then
		n.err("no path returned for username ending with _")
	end
end
