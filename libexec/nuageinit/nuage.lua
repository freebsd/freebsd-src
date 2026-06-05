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
	if input == nil or #input == 0 then
		return ""
	end
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

local function shell_escape(s)
	return "'" .. string.gsub(s, "'", "'\\''") .. "'"
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
	mode = tonumber(mode, 8)
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
		if oldpath:sub(1, 1) == "/" then
			return "/"
		end
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
	-- Basic hostname validation (RFC 952/1123)
	if #hostname == 0 then
		warnmsg("hostname is empty, ignoring")
		return
	end
	if #hostname > 253 then
		warnmsg("hostname too long (" .. #hostname .. " > 253), ignoring")
		return
	end
	if hostname:match("[^a-zA-Z0-9%.%-]") then
		warnmsg("hostname contains invalid characters: " .. hostname)
		return
	end
	if hostname:match("^[%.%-]") or hostname:match("[%.%-]$") then
		warnmsg("hostname must not start or end with a dot or hyphen: " .. hostname)
		return
	end
	for label in hostname:gmatch("[^.]+") do
		if #label > 63 then
			warnmsg("hostname label too long (" .. #label .. " > 63): " .. label)
			return
		end
		if label:match("^-") or label:match("-$") then
			warnmsg("hostname label starts or ends with hyphen: " .. label)
			return
		end
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
	f:write('hostname="' .. hostname:gsub('"', '\\"') .. '"\n')
	f:close()
end

local function update_etc_hosts(root, hostname)
	if hostname == nil or hostname == "" then
		return
	end
	local hosts_path = root .. "/etc/hosts"
	local lines = {}
	local already_present = false

	local f = io.open(hosts_path, "r")
	if not f then
		-- File doesn't exist, create a minimal one
		local nf = io.open(hosts_path, "w")
		if not nf then
			warnmsg("unable to create " .. hosts_path)
			return
		end
		nf:write("::1\t\tlocalhost " .. hostname .. "\n")
		nf:write("127.0.0.1\t\tlocalhost " .. hostname .. "\n")
		nf:close()
		return
	end

	for line in f:lines() do
		if line:find(hostname, 1, true) then
			already_present = true
		end
		table.insert(lines, line)
	end
	f:close()

	if already_present then
		return
	end

	-- Not present, append to localhost lines
	local new_lines = {}
	local found_localhost = false
	for _, line in ipairs(lines) do
		if (line:match("^127%.0%.0%.1%s") or line:match("^::1%s")) and line:find("localhost", 1, true) then
			table.insert(new_lines, line .. " " .. hostname)
			found_localhost = true
		else
			table.insert(new_lines, line)
		end
	end

	if not found_localhost then
		table.insert(new_lines, "127.0.0.1\t\tlocalhost " .. hostname)
	end

	f = io.open(hosts_path, "w")
	if not f then
		warnmsg("unable to open " .. hosts_path .. " for writing")
		return
	end
	for _, line in ipairs(new_lines) do
		f:write(line .. "\n")
	end
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

local function purge_group(groups)
	local existing = getgroups()
	local ret = {}

	for _, group in ipairs(groups) do
		local found = false
		for _, eg in ipairs(existing) do
			if group == eg then
				found = true
				break
			end
		end
		if found then
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
	local f = io.popen(cmd .. " usershow " .. shell_escape(pwd.name) .. " -7 2> /dev/null")
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
			local escaped_list = {}
			for _, g in ipairs(list) do
				table.insert(escaped_list, shell_escape(g))
			end
			extraargs = " -G " .. table.concat(escaped_list, ",")
		end
	end
	-- pw will automatically create a group named after the username
	-- do not add a -g option in this case
	if pwd.primary_group and pwd.primary_group ~= pwd.name then
		extraargs = extraargs .. " -g " .. shell_escape(pwd.primary_group)
	end
	if not pwd.no_create_home then
		extraargs = extraargs .. " -m "
	end
	if not pwd.shell then
		pwd.shell = "/bin/sh"
	end
	local postcmd = ""
	local input = nil
	if pwd.passwd then
		input = pwd.passwd
		postcmd = " -H 0"
	elseif pwd.plain_text_passwd then
		input = pwd.plain_text_passwd
		postcmd = " -h 0"
	end
	cmd = "pw "
	if root then
		cmd = cmd .. "-R " .. root .. " "
	end
	cmd = cmd .. "useradd -n " .. shell_escape(pwd.name) .. " -M 0755 -w none "
	cmd = cmd .. extraargs .. " -c " .. shell_escape(pwd.gecos)
	cmd = cmd .. " -d " .. shell_escape(pwd.homedir) .. " -s " .. shell_escape(pwd.shell) .. postcmd

	f = io.popen(cmd, "w")
	if input then
		f:write(input)
	end
	local r = f:close()
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
		cmd = cmd .. "lock " .. shell_escape(pwd.name)
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
	local f = io.popen(cmd .. " groupshow " .. shell_escape(grp.name) .. " 2> /dev/null")
	local grpstr = f:read("*a")
	f:close()
	if grpstr:len() ~= 0 then
		return true
	end
	local extraargs = ""
	if grp.members then
		local list = splitlist(grp.members)
		local escaped_list = {}
		for _, m in ipairs(list) do
			table.insert(escaped_list, shell_escape(m))
		end
		extraargs = " -M " .. table.concat(escaped_list, ",")
	end
	cmd = "pw "
	if root then
		cmd = cmd .. "-R " .. root .. " "
	end
	cmd = cmd .. "groupadd -n " .. shell_escape(grp.name) .. extraargs
	local r = os.execute(cmd)
	if not r then
		warnmsg("fail to add group " .. grp.name)
		warnmsg(cmd)
		return false
	end
	return true
end

local function addsshkey(homedir, key)
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	if root then
		homedir = root .. "/" .. homedir
	end
	local ak_path = homedir .. "/.ssh/authorized_keys"
	local dotssh_path = homedir .. "/.ssh"

	-- Check what already exists before creating anything
	local ak_exists = lfs.attributes(ak_path) ~= nil
	local dotssh_exists = lfs.attributes(dotssh_path) ~= nil

	-- Ensure .ssh directory exists
	if not dotssh_exists then
		local r, err = mkdir_p(dotssh_path)
		if not r then
			warnmsg("cannot create " .. dotssh_path .. ": " .. err)
			return
		end
	end

	-- Get homedir attributes for ownership
	local dirattrs = lfs.attributes(homedir)
	if not dirattrs then
		warnmsg("cannot get attributes for " .. homedir)
		return
	end

	local f = io.open(ak_path, "a")
	if not f then
		warnmsg("impossible to open " .. ak_path)
		return
	end
	f:write(key .. "\n")
	f:close()

	-- Set permissions and ownership on newly created files/dirs
	if not ak_exists then
		chmod(ak_path, "0600")
		chown(ak_path, dirattrs.uid, dirattrs.gid)
	end
	if not dotssh_exists then
		chmod(dotssh_path, "0700")
		chown(dotssh_path, dirattrs.uid, dirattrs.gid)
	end
end

local function adddoas(pwd)
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	local localbase = getlocalbase()
	local etcdir = localbase .. "/etc"
	if root then
		etcdir= root .. etcdir
	end
	local doasconf = etcdir .. "/doas.conf"

	local doasconf_exists = lfs.attributes(doasconf) ~= nil
	local etcdir_exists = lfs.attributes(etcdir) ~= nil

	-- Ensure etc directory exists
	if not etcdir_exists then
		local r, err = mkdir_p(etcdir)
		if not r then
			warnmsg("cannot create " .. etcdir .. ": " .. err)
			return
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

	-- Set permissions on newly created files/dirs
	if not doasconf_exists then
		chmod(doasconf, "0640")
	end
	if not etcdir_exists then
		chmod(etcdir, "0755")
	end
end

local function addsudo(pwd)
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	local localbase = getlocalbase()
	local sudoers_dir = localbase .. "/etc/sudoers.d"
	if root then
		sudoers_dir= root .. sudoers_dir
	end
	local sudoers = sudoers_dir .. "/90-nuageinit-users"

	local sudoers_exists = lfs.attributes(sudoers) ~= nil
	local sudoers_dir_exists = lfs.attributes(sudoers_dir) ~= nil

	-- Ensure sudoers.d directory exists
	if not sudoers_dir_exists then
		local r, err = mkdir_p(sudoers_dir)
		if not r then
			warnmsg("cannot create " .. sudoers_dir .. ": " .. err)
			return
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

	-- Set permissions on newly created files/dirs
	if not sudoers_exists then
		chmod(sudoers, "0440")
	end
	if not sudoers_dir_exists then
		chmod(sudoers_dir, "0750")
	end
end

local function update_sshd_config(key, value)
	local sshd_config = "/etc/ssh/sshd_config"
	local root = os.getenv("NUAGE_FAKE_ROOTDIR")
	if root then
		sshd_config = root .. sshd_config
	end
	local f = io.open(sshd_config, "r")
	if not f then
		-- File does not exist, create it with the given key/value
		f = io.open(sshd_config, "w")
		if not f then
			warnmsg("Unable to open " .. sshd_config .. " for writing")
			return
		end
		f:write(key .. " " .. value .. "\n")
		f:close()
		return
	end
	-- Read existing content
	local lines = {}
	local found = false
	local pattern = "^%s*"..key:lower().."%s+(%w+)%s*#?.*$"
	for line in f:lines() do
		local _, _, val = line:lower():find(pattern)
		if val then
			found = true
			if val ~= value then
				table.insert(lines, key .. " " .. value)
			else
				table.insert(lines, line)
			end
		else
			table.insert(lines, line)
		end
	end
	f:close()
	if not found then
		table.insert(lines, key .. " " .. value)
	end
	-- Write back
	f = io.open(sshd_config .. ".nuageinit", "w")
	if not f then
		warnmsg("Unable to open " .. sshd_config .. ".nuageinit for writing")
		return
	end
	for _, l in ipairs(lines) do
		f:write(l .. "\n")
	end
	f:close()
	os.rename(sshd_config .. ".nuageinit", sshd_config)
end

local function delete_ssh_host_keys(root)
	local ssh_dir = root .. "/etc/ssh"
	local attrs = lfs.attributes(ssh_dir)
	if not attrs or attrs.mode ~= "directory" then
		return
	end
	for entry in lfs.dir(ssh_dir) do
		if entry:match("^ssh_host_.*key") or entry:match("^ssh_host_.*key%.pub") then
			os.remove(ssh_dir .. "/" .. entry)
		end
	end
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
	cmd = cmd .. "usermod " .. shell_escape(user) .. postcmd
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
	local r = f:close()
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
		else
			for _, u in ipairs(obj.users) do
				if type(u) ~= "table" then
					warnmsg("Invalid chpasswd.users entry, expecting an object, got a " .. type(u))
				elseif not u.name then
					warnmsg("Invalid entry for chpasswd.users: missing 'name'")
				elseif not u.password then
					warnmsg("Invalid entry for chpasswd.users: missing 'password'")
				else
					exec_change_password(u.name, u.password, u.type, expire)
				end
			end
		end
	end
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

	local f, _, rc = os.execute("tzsetup -s -C " .. shell_escape(root) .. " " .. shell_escape(timezone))

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
	local install_cmd = "pkg install -y " .. shell_escape(package)
	local test_cmd = "pkg info -q " .. shell_escape(package)
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

local function add_fstab_entry(root, device, mount_point, fstype, options, dump_freq, passno)
	local fstab_path = root .. "/etc/fstab"
	local f = io.open(fstab_path, "a")
	if not f then
		warnmsg("unable to open " .. fstab_path .. " for writing")
		return false
	end
	options = options or "rw"
	dump_freq = dump_freq or 0
	passno = passno or 0
	f:write(string.format("%s\t\t%s\t\t%s\t\t%s\t\t%d\t\t%d\n",
	    device, mount_point, fstype, options, dump_freq, passno))
	f:close()
	return true
end

local function remove_fstab_entry(root, mount_point)
	local fstab_path = root .. "/etc/fstab"
	local f = io.open(fstab_path, "r")
	if not f then
		return
	end
	local lines = {}
	for line in f:lines() do
		local fields = {}
		for field in line:gmatch("%S+") do
			table.insert(fields, field)
		end
		if fields[2] ~= mount_point then
			table.insert(lines, line)
		end
	end
	f:close()
	local nf = io.open(fstab_path, "w")
	if not nf then
		warnmsg("unable to open " .. fstab_path .. " for writing")
		return
	end
	for _, line in ipairs(lines) do
		nf:write(line .. "\n")
	end
	nf:close()
end

local n = {
	shell_escape = shell_escape,
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
	delete_ssh_host_keys = delete_ssh_host_keys,
	update_etc_hosts = update_etc_hosts,
	chpasswd = chpasswd,
	pkg_bootstrap = pkg_bootstrap,
	install_package = install_package,
	update_packages = update_packages,
	upgrade_packages = upgrade_packages,
	addsudo = addsudo,
	adddoas = adddoas,
	addfile = addfile,
	add_fstab_entry = add_fstab_entry,
	remove_fstab_entry = remove_fstab_entry,
}

return n
