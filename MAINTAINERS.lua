#! /usr/libexec/flua

--[[

Usage: 
  ./MAINTAINERS.lua <command> [argument]

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

local is_child_of = function (maybe_child, of)
	local s = ''
	for segment in string.gmatch(maybe_child, '..-[/%$]') do
		s = s..segment
		if string.find(of, luafy_glob(s)..'$') and s ~= maybe_child then
			return true
		end
	end
	return false
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

local try_insert = function (arr, item)
	for _, i in ipairs(arr) do
		if i == item then
			return
		end
	end
	table.insert(arr, item)
end

if arg[1] == 'get_maintainers' then
	local path = arg[2]
	local out = {}

	for _, group in pairs(groups) do
		if not (group.except and path_has_any(group.except, path))
			and path_has_any(group.watch, path) then
			foreach(group.members, function (member) try_insert(out, member) end)
		end
	end

	foreach(out, function(person_id) print(person_id) end)
elseif arg[1] == 'get_paths' then
	local person_id = arg[2]
	local watch = {}
	local except = {}

	for _, group in pairs(groups) do
		for _, member in ipairs(group.members) do
			if member == person_id then
				foreach(group.watch, function (path) table.insert(watch, path) end)
				if group.except then
					foreach(group.except, function (path) try_insert(except, path) end)
				end
				break
			end
		end
	end

	foreach(watch, function (path) print('watch', path) end)
	foreach(except, function (path) print('except', path) end)
elseif arg[1] == 'get_info' then
	local person_id = arg[2]
	if people[person_id] then
		for k, v in pairs(people[person_id]) do
			print(k, v)
		end
	end
elseif arg[1] == 'update' then
	--
	-- GitHub
	--
	
	-- check if adding path to t would steal from an existing path
	local checked_add_path = function (path, table)
		table[path] = {}
		for old_path, watchers in pairs(table) do
			if is_child_of(path, old_path) then
				foreach(watchers, function (w) try_insert(table[path], w) end)
			end
		end
	end

	local gh_table = {}
	for _, group in pairs(groups) do
		if group.except then
			for _, except_path in ipairs(group.except) do
				if not gh_table[except_path] then
					checked_add_path(except_path, gh_table)	
				end
			end
		end
		for _, path in ipairs(group.watch) do
			-- check if anybody is stealing from us
			for old_path, _ in pairs(gh_table) do
				if is_child_of(old_path, path) and not (group.except and path_has_any(group.except, old_path)) then
						foreach(group.members, function (m) try_insert(gh_table[old_path], m) end)
				end
			end

			if not gh_table[path] then
				checked_add_path(path, gh_table)
			end

			foreach(group.members, function (m) try_insert(gh_table[path], m) end)
		end
	end
	
	-- sort the paths so the generated file is easier to read
	local paths = {}
	for k, _ in pairs(gh_table) do table.insert(paths, k) end
	table.sort(paths)
	
	local gh_file = io.open('.github/CODEOWNERS', 'w')
	if gh_file then
		for _, path in ipairs(paths) do
			gh_file:write(path)
			for _, id in ipairs(gh_table[path]) do
				if people[id] and people[id].github then
					gh_file:write(' ', people[id].github)
				end
			end
			gh_file:write('\n')
		end
		gh_file:close()
		print('.github/CODEOWNERS updated')
	else
		print('could not update .github/CODEOWNERS')
	end

	--
	-- Forgejo stuff
	--

	local fj_file = io.open('.forgejo/CODEOWNERS', 'w')
	if fj_file then
		-- TODO: write fj table
		fj_file:close()
		print('.forgejo/CODEOWNERS updated')
	else
		print('could not update .forgejo/CODEOWNERS')
	end
else
	print('Unrecognized command')
end
