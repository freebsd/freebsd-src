-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright(c) 2022 Baptiste Daroussin <bapt@FreeBSD.org>

local pu = require("posix.unistd")

local function warnmsg(str)
	io.stderr:write(str.."\n")
end

local function errmsg(str)
	io.stderr:write(str.."\n")
	os.exit(1)
end

local function dirname(oldpath)
	if not oldpath then
		return nil
	end
	local path = oldpath:gsub("[^/]+/*$", "")
	if path == "" then
		return nil
	end
	return path
end

local function mkdir_p(path)
	if lfs.attributes(path, "mode") ~= nil then
		return true
	end
	local r,err = mkdir_p(dirname(path))
	if not r then
		return nil,err.." (creating "..path..")"
	end
	return lfs.mkdir(path)
end

local function sethostname(hostname)
	if hostname == nil then return end
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	if not root then
		root = ""
	end
	local hostnamepath = root .. "/etc/rc.conf.d/hostname"

	mkdir_p(dirname(hostnamepath))
	local f,err = io.open(hostnamepath, "w")
	if not f then
		warnmsg("Impossible to open "..hostnamepath .. ":" ..err)
		return
	end
	f:write("hostname=\""..hostname.."\"\n")
	f:close()
end

local function splitlist(list)
	local ret = {}
	if type(list) == "string" then
		for str in list:gmatch("([^, ]+)") do
			ret[#ret + 1] = str
		end
	elseif type(list) == "table" then
		ret = list
	else
		warnmsg("Invalid type ".. type(list) ..", expecting table or string")
	end
	return ret
end

local function adduser(pwd)
	if (type(pwd) ~= "table") then
		warnmsg("Argument should be a table")
		return nil
	end
	local f = io.popen("getent passwd "..pwd.name)
	local pwdstr = f:read("*a")
	f:close()
	if pwdstr:len() ~= 0 then
		return pwdstr:match("%a+:.+:%d+:%d+:.*:(.*):.*")
	end
	if not pwd.gecos then
		pwd.gecos = pwd.name .. " User"
	end
	if not pwd.home then
		pwd.home = "/home/" .. pwd.name
	end
	local extraargs=""
	if pwd.groups then
		local list = splitlist(pwd.groups)
		extraargs = " -G ".. table.concat(list, ',')
	end
	-- pw will automatically create a group named after the username
	-- do not add a -g option in this case
	if pwd.primary_group and pwd.primary_group ~= pwd.name then
		extraargs = extraargs .. " -g " .. pwd.primary_group
	end
	if not pwd.no_create_home then
		extraargs = extraargs .. " -m "
	end
	if not pwd.shell then
		pwd.shell = "/bin/sh"
	end
	local precmd = ""
	local postcmd = ""
	if pwd.passwd then
		precmd = "echo "..pwd.passwd .. "| "
		postcmd = " -H 0 "
	elseif pwd.plain_text_passwd then
		precmd = "echo "..pwd.plain_text_passwd .. "| "
		postcmd = " -H 0 "
	end
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	local cmd = precmd .. "pw "
	if root then
		cmd = cmd .. "-R " .. root .. " "
	end
	cmd = cmd .. "useradd -n ".. pwd.name .. " -M 0755 -w none "
	cmd = cmd .. extraargs .. " -c '".. pwd.gecos
	cmd = cmd .. "' -d '" .. pwd.home .. "' -s "..pwd.shell .. postcmd

	local r = os.execute(cmd)
	if not r then
		warnmsg("nuageinit: fail to add user "..pwd.name);
		warnmsg(cmd)
		return nil
	end
	if pwd.locked then
		cmd = "pw "
		if root then
			cmd = cmd .. "-R " .. root .. " "
		end
		cmd = cmd .. "lock " .. pwd.name
		os.execute(cmd)
	end
	return pwd.home
end

local function addgroup(grp)
	if (type(grp) ~= "table") then
		warnmsg("Argument should be a table")
		return false
	end
	local f = io.popen("getent group "..grp.name)
	local grpstr = f:read("*a")
	f:close()
	if grpstr:len() ~= 0 then
		return true
	end
	local extraargs = ""
	if grp.members then
		local list = splitlist(grp.members)
		extraargs = " -M " .. table.concat(list, ',')
	end
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	local cmd = "pw "
	if root then
		cmd = cmd .. "-R " .. root .. " "
	end
	cmd = cmd .. "groupadd -n ".. grp.name .. extraargs
	local r = os.execute(cmd)
	if not r then
		warnmsg("nuageinit: fail to add group ".. grp.name);
		warnmsg(cmd)
		return false
	end
	return true
end

local function addsshkey(homedir, key)
	local chownak = false
	local chowndotssh = false
	local ak_path = homedir .. "/.ssh/authorized_keys"
	local dotssh_path = homedir .. "/.ssh"
	local dirattrs = lfs.attributes(ak_path)
	if dirattrs == nil then
		chownak = true
		dirattrs = lfs.attributes(dotssh_path)
		if dirattrs == nil then
			if not lfs.mkdir(dotssh_path) then
				warnmsg("nuageinit: impossible to create ".. dotssh_path)
				return
			end
			chowndotssh = true
			dirattrs = lfs.attributes(homedir)
		end
	end

	local f = io.open(ak_path, "a")
	if not f then
		warnmsg("nuageinit: impossible to open "..ak_path)
		return
	end
	f:write(key .. "\n")
	f:close()
	if chownak then
		pu.chown(ak_path, dirattrs.uid, dirattrs.gid)
	end
	if chowndotssh then
		pu.chown(dotssh_path, dirattrs.uid, dirattrs.gid)
	end
end

local n = {
	warn = warnmsg,
	err = errmsg,
	sethostname = sethostname,
	adduser = adduser,
	addgroup = addgroup,
	addsshkey = addsshkey,
	dirname = dirname,
	mkdir_p = mkdir_p,
}

return n
