---
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright(c) 2022 Baptiste Daroussin <bapt@FreeBSD.org>

local unistd = require("posix.unistd")
local sys_stat = require("posix.sys.stat")
local lfs = require("lfs")

local function warnmsg(str, prepend)
	if not str then
		return
	end
	local tag = ""
	if prepend ~= false then
		tag = "nuageinit: "
	end
	io.stderr:write(tag .. str .. "\n")
end

local function errmsg(str, prepend)
	warnmsg(str, prepend)
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
	local r, err = mkdir_p(dirname(path))
	if not r then
		return nil, err .. " (creating " .. path .. ")"
	end
	return lfs.mkdir(path)
end

local function sethostname(hostname)
	if hostname == nil then
		return
	end
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	if not root then
		root = ""
	end
	local hostnamepath = root .. "/etc/rc.conf.d/hostname"

	mkdir_p(dirname(hostnamepath))
	local f, err = io.open(hostnamepath, "w")
	if not f then
		warnmsg("Impossible to open " .. hostnamepath .. ":" .. err)
		return
	end
	f:write('hostname="' .. hostname .. '"\n')
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
		warnmsg("Invalid type " .. type(list) .. ", expecting table or string")
	end
	return ret
end

local function adduser(pwd)
	if (type(pwd) ~= "table") then
		warnmsg("Argument should be a table")
		return nil
	end
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	local cmd = "pw "
	if root then
		cmd = cmd .. "-R " .. root .. " "
	end
	local f = io.popen(cmd .. " usershow " .. pwd.name .. " -7 2> /dev/null")
	local pwdstr = f:read("*a")
	f:close()
	if pwdstr:len() ~= 0 then
		return pwdstr:match("%a+:.+:%d+:%d+:.*:(.*):.*")
	end
	if not pwd.gecos then
		pwd.gecos = pwd.name .. " User"
	end
	if not pwd.homedir then
		pwd.homedir = "/home/" .. pwd.name
	end
	local extraargs = ""
	if pwd.groups then
		local list = splitlist(pwd.groups)
		extraargs = " -G " .. table.concat(list, ",")
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
	local input = nil
	if pwd.passwd then
		input = pwd.passwd
		postcmd = " -H 0"
	elseif pwd.plain_text_passwd then
		input = pwd.plain_text_passwd
		postcmd = " -h 0"
	end
	cmd = precmd .. "pw "
	if root then
		cmd = cmd .. "-R " .. root .. " "
	end
	cmd = cmd .. "useradd -n " .. pwd.name .. " -M 0755 -w none "
	cmd = cmd .. extraargs .. " -c '" .. pwd.gecos
	cmd = cmd .. "' -d '" .. pwd.homedir .. "' -s " .. pwd.shell .. postcmd

	f = io.popen(cmd, "w")
	if input then
		f:write(input)
	end
	local r = f:close(cmd)
	if not r then
		warnmsg("fail to add user " .. pwd.name)
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
	return pwd.homedir
end

local function addgroup(grp)
	if (type(grp) ~= "table") then
		warnmsg("Argument should be a table")
		return false
	end
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	local cmd = "pw "
	if root then
		cmd = cmd .. "-R " .. root .. " "
	end
	local f = io.popen(cmd .. " groupshow " .. grp.name .. " 2> /dev/null")
	local grpstr = f:read("*a")
	f:close()
	if grpstr:len() ~= 0 then
		return true
	end
	local extraargs = ""
	if grp.members then
		local list = splitlist(grp.members)
		extraargs = " -M " .. table.concat(list, ",")
	end
	cmd = "pw "
	if root then
		cmd = cmd .. "-R " .. root .. " "
	end
	cmd = cmd .. "groupadd -n " .. grp.name .. extraargs
	local r = os.execute(cmd)
	if not r then
		warnmsg("fail to add group " .. grp.name)
		warnmsg(cmd)
		return false
	end
	return true
end

local function addsshkey(homedir, key)
	local chownak = false
	local chowndotssh = false
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	if root then
		homedir = root .. "/" .. homedir
	end
	local ak_path = homedir .. "/.ssh/authorized_keys"
	local dotssh_path = homedir .. "/.ssh"
	local dirattrs = lfs.attributes(ak_path)
	if dirattrs == nil then
		chownak = true
		dirattrs = lfs.attributes(dotssh_path)
		if dirattrs == nil then
			assert(lfs.mkdir(dotssh_path))
			chowndotssh = true
			dirattrs = lfs.attributes(homedir)
		end
	end

	local f = io.open(ak_path, "a")
	if not f then
		warnmsg("impossible to open " .. ak_path)
		return
	end
	f:write(key .. "\n")
	f:close()
	if chownak then
		sys_stat.chmod(ak_path, 384)
		unistd.chown(ak_path, dirattrs.uid, dirattrs.gid)
	end
	if chowndotssh then
		sys_stat.chmod(dotssh_path, 448)
		unistd.chown(dotssh_path, dirattrs.uid, dirattrs.gid)
	end
end

local function update_sshd_config(key, value)
	local sshd_config = "/etc/ssh/sshd_config"
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	if root then
		sshd_config = root .. sshd_config
	end
	local f = assert(io.open(sshd_config, "r+"))
	local tgt = assert(io.open(sshd_config .. ".nuageinit", "w"))
	local found = false
	local pattern = "^%s*"..key:lower().."%s+(%w+)%s*#?.*$"
	while true do
		local line = f:read()
		if line == nil then break end
		local _, _, val = line:lower():find(pattern)
		if val then
			found = true
			if val == value then
				assert(tgt:write(line .. "\n"))
			else
				assert(tgt:write(key .. " " .. value .. "\n"))
			end
		else
			assert(tgt:write(line .. "\n"))
		end
	end
	if not found then
		assert(tgt:write(key .. " " .. value .. "\n"))
	end
	assert(f:close())
	assert(tgt:close())
	os.rename(sshd_config .. ".nuageinit", sshd_config)
end

local n = {
	warn = warnmsg,
	err = errmsg,
	dirname = dirname,
	mkdir_p = mkdir_p,
	sethostname = sethostname,
	adduser = adduser,
	addgroup = addgroup,
	addsshkey = addsshkey,
	update_sshd_config = update_sshd_config
}

return n
