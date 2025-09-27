---
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright(c) 2022-2025 Baptiste Daroussin <bapt@FreeBSD.org>
-- Copyright(c) 2025 Jesús Daniel Colmenares Oviedo <dtxdf@FreeBSD.org>

local unistd = require("posix.unistd")
local sys_stat = require("posix.sys.stat")
local lfs = require("lfs")

local function getlocalbase()
	local f = io.popen("sysctl -in user.localbase 2> /dev/null")
	local localbase = f:read("*l")
	f:close()
	if localbase == nil or localbase:len() == 0 then
		-- fallback
		localbase = "/usr/local"
	end
	return localbase
end

local function decode_base64(input)
	local b = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
	input = string.gsub(input, '[^'..b..'=]', '')

	local result = {}
	local bits = ''

	-- convert all characters in bits
	for i = 1, #input do
		local x = input:sub(i, i)
		if x == '=' then
			break
		end
		local f = b:find(x) - 1
		for j = 6, 1, -1 do
			bits = bits .. (f % 2^j - f % 2^(j-1) > 0 and '1' or '0')
		end
	end

	for i = 1, #bits, 8 do
		local byte = bits:sub(i, i + 7)
		if #byte == 8 then
			local c = 0
			for j = 1, 8 do
				c = c + (byte:sub(j, j) == '1' and 2^(8 - j) or 0)
			end
			table.insert(result, string.char(c))
		end
	end

	return table.concat(result)
end

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

local function chmod(path, mode)
	local mode = tonumber(mode, 8)
	local _, err, msg = sys_stat.chmod(path, mode)
	if err then
		errmsg("chmod(" .. path .. ", " .. mode .. ") failed: " .. msg)
	end
end

local function chown(path, owner, group)
	local _, err, msg = unistd.chown(path, owner, group)
	if err then
		errmsg("chown(" .. path .. ", " .. owner .. ", " .. group .. ") failed: " .. msg)
	end
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

local function splitlines(s)
	local ret = {}

	for line in string.gmatch(s, "[^\n]+") do
		ret[#ret + 1] = line
	end

	return ret
end

local function getgroups()
	local ret = {}

	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	local cmd = "pw "
	if root then
		cmd = cmd .. "-R " .. root .. " "
	end

	local f = io.popen(cmd .. "groupshow -a 2> /dev/null | cut -d: -f1")
	local groups = f:read("*a")
	f:close()

	return splitlines(groups)
end

local function checkgroup(group)
	local groups = getgroups()

	for _, group2chk in ipairs(groups) do
		if group == group2chk then
			return true
		end
	end

	return false
end

local function purge_group(groups)
	local ret = {}

	for _, group in ipairs(groups) do
		if checkgroup(group) then
			ret[#ret + 1] = group
		else
			warnmsg("ignoring non-existent group '" .. group .. "'")
		end
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
		-- pw complains if the group does not exist, so if the user
		-- specifies one that cannot be found, nuageinit will generate
		-- an exception and exit, unlike cloud-init, which only issues
		-- a warning but creates the user anyway.
		list = purge_group(list)
		if #list > 0 then
			extraargs = " -G " .. table.concat(list, ",")
		end
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
		chmod(ak_path, "0600")
		chown(ak_path, dirattrs.uid, dirattrs.gid)
	end
	if chowndotssh then
		chmod(dotssh_path, "0700")
		chown(dotssh_path, dirattrs.uid, dirattrs.gid)
	end
end

local function adddoas(pwd)
	local chmodetcdir = false
	local chmoddoasconf = false
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	local localbase = getlocalbase()
	local etcdir = localbase .. "/etc"
	if root then
		etcdir= root .. etcdir
	end
	local doasconf = etcdir .. "/doas.conf"
	local doasconf_attr = lfs.attributes(doasconf)
	if doasconf_attr == nil then
		chmoddoasconf = true
		local dirattrs = lfs.attributes(etcdir)
		if dirattrs == nil then
			local r, err = mkdir_p(etcdir)
			if not r then
				return nil, err .. " (creating " .. etcdir .. ")"
			end
			chmodetcdir = true
		end
	end
	local f = io.open(doasconf, "a")
	if not f then
		warnmsg("impossible to open " .. doasconf)
		return
	end
	if type(pwd.doas) == "string" then
		local rule = pwd.doas
		rule = rule:gsub("%%u", pwd.name)
		f:write(rule .. "\n")
	elseif type(pwd.doas) == "table" then
		for _, str in ipairs(pwd.doas) do
			local rule = str
			rule = rule:gsub("%%u", pwd.name)
			f:write(rule .. "\n")
		end
	end
	f:close()
	if chmoddoasconf then
		chmod(doasconf, "0640")
	end
	if chmodetcdir then
		chmod(etcdir, "0755")
	end
end

local function addsudo(pwd)
	local chmodsudoersd = false
	local chmodsudoers = false
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	local localbase = getlocalbase()
	local sudoers_dir = localbase .. "/etc/sudoers.d"
	if root then
		sudoers_dir= root .. sudoers_dir
	end
	local sudoers = sudoers_dir .. "/90-nuageinit-users"
	local sudoers_attr = lfs.attributes(sudoers)
	if sudoers_attr == nil then
		chmodsudoers = true
		local dirattrs = lfs.attributes(sudoers_dir)
		if dirattrs == nil then
			local r, err = mkdir_p(sudoers_dir)
			if not r then
				return nil, err .. " (creating " .. sudoers_dir .. ")"
			end
			chmodsudoersd = true
		end
	end
	local f = io.open(sudoers, "a")
	if not f then
		warnmsg("impossible to open " .. sudoers)
		return
	end
	if type(pwd.sudo) == "string" then
		f:write(pwd.name .. " " .. pwd.sudo .. "\n")
	elseif type(pwd.sudo) == "table" then
		for _, str in ipairs(pwd.sudo) do
			f:write(pwd.name .. " " .. str .. "\n")
		end
	end
	f:close()
	if chmodsudoers then
		chmod(sudoers, "0440")
	end
	if chmodsudoersd then
		chmod(sudoers_dir, "0750")
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

local function exec_change_password(user, password, type, expire)
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	local cmd = "pw "
	if root then
		cmd = cmd .. "-R " .. root .. " "
	end
	local postcmd = " -H 0"
	local input = password
	if type ~= nil and type == "text" then
		postcmd = " -h 0"
	else
		if password == "RANDOM" then
			input = nil
			postcmd = " -w random"
		end
	end
	cmd = cmd .. "usermod " .. user .. postcmd
	if expire then
		cmd = cmd .. " -p 1"
	else
		cmd = cmd .. " -p 0"
	end
	local f = io.popen(cmd .. " >/dev/null", "w")
	if input then
		f:write(input)
	end
	-- ignore stdout to avoid printing the password in case of random password
	local r = f:close(cmd)
	if not r then
		warnmsg("fail to change user password ".. user)
		warnmsg(cmd)
	end
end

local function change_password_from_line(line, expire)
	local user, password = line:match("%s*(%w+):(%S+)%s*")
	local type = nil
	if user and password then
		if password == "R" then
			password = "RANDOM"
		end
		if not password:match("^%$%d+%$%w+%$") then
			if password ~= "RANDOM" then
				type = "text"
			end
		end
		exec_change_password(user, password, type, expire)
	end
end

local function chpasswd(obj)
	if type(obj) ~= "table" then
		warnmsg("Invalid chpasswd entry, expecting an object")
		return
	end
	local expire = false
	if obj.expire ~= nil then
		if type(obj.expire) == "boolean" then
			expire = obj.expire
		else
			warnmsg("Invalid type for chpasswd.expire, expecting a boolean, got a ".. type(obj.expire))
		end
	end
	if obj.users ~= nil then
		if type(obj.users) ~= "table" then
			warnmsg("Invalid type for chpasswd.users, expecting a list, got a ".. type(obj.users))
			goto list
		end
		for _, u in ipairs(obj.users) do
			if type(u) ~= "table" then
				warnmsg("Invalid chpasswd.users entry, expecting an object, got a " .. type(u))
				goto next
			end
			if not u.name then
				warnmsg("Invalid entry for chpasswd.users: missing 'name'")
				goto next
			end
			if not u.password then
				warnmsg("Invalid entry for chpasswd.users: missing 'password'")
				goto next
			end
			exec_change_password(u.name, u.password, u.type, expire)
			::next::
		end
	end
	::list::
	if obj.list ~= nil then
		warnmsg("chpasswd.list is deprecated consider using chpasswd.users")
		if type(obj.list) == "string" then
			for line in obj.list:gmatch("[^\n]+") do
				change_password_from_line(line, expire)
			end
		elseif type(obj.list) == "table" then
			for _, u in ipairs(obj.list) do
				change_password_from_line(u, expire)
			end
		end
	end
end

local function settimezone(timezone)
	if timezone == nil then
		return
	end
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	if not root then
		root = "/"
	end

	f, _, rc = os.execute("tzsetup -s -C " .. root .. " " .. timezone)

	if not f then
		warnmsg("Impossible to configure time zone ( rc = " .. rc .. " )")
		return
	end
end

local function pkg_bootstrap()
	if os.getenv("NUAGE_RUN_TESTS") then
		return true
	end
	if os.execute("pkg -N 2>/dev/null") then
		return true
	end
	print("Bootstrapping pkg")
	return os.execute("env ASSUME_ALWAYS_YES=YES pkg bootstrap")
end

local function install_package(package)
	if package == nil then
		return true
	end
	local install_cmd = "pkg install -y " .. package
	local test_cmd = "pkg info -q " .. package
	if os.getenv("NUAGE_RUN_TESTS") then
		print(install_cmd)
		print(test_cmd)
		return true
	end
	if os.execute(test_cmd) then
		return true
	end
	return os.execute(install_cmd)
end

local function run_pkg_cmd(subcmd)
	local cmd = "env ASSUME_ALWAYS_YES=yes pkg " .. subcmd
	if os.getenv("NUAGE_RUN_TESTS") then
		print(cmd)
		return true
	end
	return os.execute(cmd)
end
local function update_packages()
	return run_pkg_cmd("update")
end

local function upgrade_packages()
	return run_pkg_cmd("upgrade")
end

local function addfile(file, defer)
	if type(file) ~= "table" then
		return false, "Invalid object"
	end
	if defer and not file.defer then
		return true
	end
	if not defer and file.defer then
		return true
	end
	if not file.path then
		return false, "No path provided for the file to write"
	end
	local content = nil
	if file.content then
		if file.encoding then
			if file.encoding == "b64" or file.encoding == "base64" then
				content = decode_base64(file.content)
			else
				return false, "Unsupported encoding: " .. file.encoding
			end
		else
			content = file.content
		end
	end
	local mode = "w"
	if file.append then
		mode = "a"
	end

	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	if not root then
		root = ""
	end
	local filepath = root .. file.path
	local f = assert(io.open(filepath, mode))
	if content then
		f:write(content)
	end
	f:close()
	if file.permissions then
		chmod(filepath, file.permissions)
	end
	if file.owner then
		local owner, group = string.match(file.owner, "([^:]+):([^:]+)")
		if not owner then
			owner = file.owner
		end
		chown(filepath, owner, group)
	end
	return true
end

local n = {
	warn = warnmsg,
	err = errmsg,
	chmod = chmod,
	chown = chown,
	dirname = dirname,
	mkdir_p = mkdir_p,
	sethostname = sethostname,
	settimezone = settimezone,
	adduser = adduser,
	addgroup = addgroup,
	addsshkey = addsshkey,
	update_sshd_config = update_sshd_config,
	chpasswd = chpasswd,
	pkg_bootstrap = pkg_bootstrap,
	install_package = install_package,
	update_packages = update_packages,
	upgrade_packages = upgrade_packages,
	addsudo = addsudo,
	adddoas = adddoas,
	addfile = addfile
}

return n
