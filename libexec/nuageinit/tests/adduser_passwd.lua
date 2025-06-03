#!/usr/libexec/flua

local n = require("nuage")

local pw = {}
pw.name = "foo"
pw.plain_text_passwd = "bar"
local res = n.adduser(pw)
if not res then
	n.err("valid user should return a path")
end

local pw2 = {}
pw2.name = "foocrypted"
-- barcrypted
pw2.passwd = "$6$ZY8faYcEfyoEZnNX$FuAZA2SKhIfYLebhEtbmjptQNrenr6mJhji35Ru.zqdaa6G/gkKiHoQuh0vYZTKrjaykyohR8W4Q5ZF56yt8u1"
res = n.adduser(pw2)
if not res then
	n.err("valid user should return a path")
end
