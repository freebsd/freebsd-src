#! /usr/libexec/flua

--[[

Usage: 
  ./MAINTAINERS.lua <command> [argument] [options]

Options:
  -i | --input     Path to maintainers file. Defaults to 'MAINTAINERS.ucl'.

Commands:
  update           generates GitHub/Forgejo CODEOWNERS file.
  get_maintainers  prints maintainers to ping for a path. Requires argument, a path.
  get_paths        prints paths that a person is responsible for. Requires argument, a person's ID.
  get_info         prints available info for a person. Requires argument, a person's ID.

--]]

if arg[1] == nil then
	error('No arguments provided')
end

local f = 'MAINTAINERS.ucl'
for k, v in ipairs(arg) do
	if v == '-i' or v == '--input' then
		local f = arg[k+1]
		break
	end
end

local ucl = require('ucl').parser()
local res, err = ucl:parse_file(f)

if not res then
	error('Parser error: '..err)
end

local t = ucl:get_object()

local version = t.version
local people = t.people
local groups = t.groups

local split_path = function (path)
	return string.gmatch('..-['..sep..'$]')
end

-- currently does not support character sets
-- also currently globbing patterns match periods even at the beginning of a filename
local luafy_glob = function (s)
	-- escape stuff that is not a glob character, but is a lua match special character
	s = string.gsub(s, '([[%%.^$+-%(%)])', '%%%1')
	-- globstar
	s = string.gsub(s, '([^\\])%*%*', '%1[^.]-')
	-- ? matches any single character that is not /
	s = string.gsub(s, '([^\\])%?', '%1[^/]')
	-- * matches any amount of characters that are not /
	s = string.gsub(s, '([^\\])%*', '%1[^/]-')
	-- handle the escapes
	s = string.gsub(s, '\\([[*?])', '%%%1')
	s = string.gsub(s, '\\(.)', '%1')
	return '^'..s
end

local foreach = function (table, fn)
	for _, v in ipairs(table) do
		fn(v)
	end
end

local path_has_any = function (arr, path)
	for _, glob in ipairs(arr) do
		if string.find(path, luafy_glob(glob)) then
			return true
		end
	end
	return false
end

if arg[1] == 'get_maintainers' then
	local path = arg[3]
	local out = {}

	for _, group in pairs(groups) do
		if (group.exclude == nil or not path_has_any(group.exclude, path))
			and path_has_any(group.watch, path) then
			foreach(group.members, function (member) table.insert(out, member) end)
		end
	end

	foreach(out, function(person_id) print(person_id) end)
elseif arg[1] == 'get_paths' then
	local person_id = arg[3]
	local watch = {}
	local exclude = {}

	for _, group in pairs(groups) do
		for _, member in ipairs(group.members) do
			if member == person_id then
				foreach(group.watch, function (path) table.insert(watch, path) end)
				if group.exclude then
					foreach(group.exclude, function (path) table.insert(exclude, path) end)
				end
				break
			end
		end
	end

	foreach(watch, function (path) print('watch', path) end)
	foreach(exclude, function (path) print('exclude', path) end)
elseif arg[1] == 'get_info' then
	local person_id = arg[3]
	if people[person_id] then
		for k, v in pairs(people[person_id]) do
			print(k, v)
		end
	end
elseif arg[1] == 'update' then
	out = {}
	for _, group in pairs(groups) do
		for _, path in ipairs(group.watch) do
			if not out[path] then
				out[path] = {}
			end
			foreach(group.members, function (m) table.insert(out[path], m) end)
		end
		if group.exclude then
			foreach(group.exclude, function (path) if not out[path] then out[path] = {} end end)
		end
	end
	
	-- sort the paths so the generated CODEOWNERS file is easier to read
	local paths = {}
	for k, _ in pairs(out) do table.insert(paths, k) end
	table.sort(paths)
	
	local gen_codeowners = function (file, field)
		for _, path in ipairs(paths) do
			file:write(path, ' ')
			for _, id in ipairs(out[path]) do
				if people[id] and people[id][field] then
					file:write(people[id][field], ' ')
				end
			end
			file:write('\n')
		end
	end
	
	-- github
	local gh_file = io.open('.github/CODEOWNERS', 'w')
	if gh_file then
		gen_codeowners(gh_file, 'github')
		gh_file:close()
		print('.github/CODEOWNERS updated')
	else
		print('could not update .github/CODEOWNERS')
	end

	-- forgejo
	-- copy from the already created file if possible
	gh_file = io.open('.github/CODEOWNERS', 'r')
	local fj_file = io.open('.forgejo/CODEOWNERS', 'w')
	if fj_file then
		if gh_file then
			fj_file:write(gh_file:read('a'))
			gh_file:close()
		else
			gen_codeowners('.forgejo/CODEOWNERS', 'forgejo')
		end
		fj_file:close()
		print('.forgejo/CODEOWNERS updated')
	else
		print('could not update .forgejo/CODEOWNERS')
	end
else
	print('Unrecognized command')
end
