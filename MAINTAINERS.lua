#! /usr/bin/lua

--[[

Usage: lua MAINTAINERS.lua <command> <maintainers file> [<argument>]

Commands:
  path_maintainers  prints maintainers to ping for a path. <argument> should be a path.
  person_paths      prints paths that a person is responsible for. <argument> should be a person's ID
  person_info       prints available info for a person. <argument> should be a person's ID.
  update            generates GitHub/Forgejo CODEOWNERS file. No <argument>.

--]]


local json = require 'lunajson'

if arg[1] == nil then
	error('No arguments provided')
end

io.input(arg[2])

local t = json.decode(io.read('a'))

local version = t.version
local people = t.person
local groups = t.group

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

if arg[1] == 'path_maintainers' then
	local path = arg[3]
	local out = {}

	for _, group in pairs(groups) do
		if (group.exclude == nil or not path_has_any(group.exclude, path))
			and path_has_any(group.watch, path) then
			foreach(group.members, function (member) table.insert(out, member) end)
		end
	end

	foreach(out, function(person_id) print(person_id) end)
elseif arg[1] == 'person_paths' then
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
elseif arg[1] == 'person_info' then
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

	-- this stuff sorts the paths so the generated CODEOWNERS files are easier to read
	local paths = {}
	for k, _ in pairs(out) do
		table.insert(paths, k)
	end

	table.sort(paths)

	local output_file = function (file)
		for _, path in ipairs(paths) do
			file:write(path, ' ')
			foreach(out[path], function (m) file:write(people[m].github, ' ') end)
			file:write('\n')
		end
	end

	local file = io.open('.github/CODEOWNERS')
	if file == nil then
		print('could not update .github/CODEOWNERS')
	else
		output_file(file)
		print('.github/CODEOWNERS updated')
	end

	-- copy from the already created file if possible
	if file and os.execute('cp .github/CODEOWNERS .forgejo/CODEOWNERS') == 0 then
		print('.forgejo/CODEOWNERS updated')
	else
		file = io.open('.forgejo/CODEOWNERS')
		if file == nil then
			print('could not update .forgejo/CODEOWNERS')
		else
			output_file(file)
			print('.forgejo/CODEOWNERS updated')
		end
	end
else
	print('Unrecognized command')
end
