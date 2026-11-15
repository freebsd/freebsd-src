#!/usr/libexec/flua

local n = require("nuage")

local pw1 = {}
pw1.name = "numericuser"
pw1.uid = 1600
local res = n.adduser(pw1)
if not res then
	n.err("adduser with numeric uid should return a path")
end

local pw2 = {}
pw2.name = "stringuser"
pw2.uid = "1601"
res = n.adduser(pw2)
if not res then
	n.err("adduser with string uid should return a path")
end

local pw3 = {}
pw3.name = "baduser"
pw3.uid = -1
res = n.adduser(pw3)
if res then
	n.err("adduser should reject negative uid")
end
