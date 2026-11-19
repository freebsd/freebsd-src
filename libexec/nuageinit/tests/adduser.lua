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

local function ends_with(s, suffix)
	if not s then
		return false
	end
	return suffix == "" or s:sub(-#suffix) == suffix
end

for _, name in ipairs({"underscore_", "0"}) do
	pw = {}
	pw.name = name
	-- Call adduser twice to test both adding and looking up an existing
	-- user.
	for _ = 1, 2 do
		local res = n.adduser(pw)
		if not ends_with(res, "/" .. name) then
			n.err("expected .../" .. name .. " for username " ..
			    name .. ", got " .. tostring(res))
		end
	end
end
