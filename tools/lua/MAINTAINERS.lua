#! /usr/libexec/flua

--[[

Expects to be run from the directory containing MAINTAINERS.ucl.
Paths to directories should end in a trailing separator.

Usage:
  ./MAINTAINERS.lua <command> <argument>

Commands:
  update       github|forgejo  outputs updated GitHub/Forgejo CODEOWNERS file.
  maintainers  <path>          outputs maintainers for <path> (relative to src).
  watched      <person>        outputs paths that <person> is responsible for.
  userinfo     <person>        outputs available contact info for <person>.

--]]

assert(#arg == 2, 'Expected 2 arguments, got '..#arg)

local ucl = require('ucl').parser()
local fnmatch = require('posix.fnmatch').fnmatch

local res, err = ucl:parse_file('MAINTAINERS.ucl')
if not res then error('UCL parser error: '..err) end

local t = ucl:get_object()
local people = t.people
local groups = t.groups

for _, group in pairs(groups) do
	if group.except == nil then
		group.except = {}
	end
	for k, v in pairs(group) do
		if type(v) ~= 'table' then
			group[k] = {v}
		end
	end
end

--
-- Helper functions
--

local glob = function (pattern, s)
	return fnmatch(pattern, s) == 0
end

local foreach = function (t, fn)
	for _, v in ipairs(t) do fn(v) end
end

local set_insert_all = function (set, input, set_has)
	for _, item in ipairs(input) do
		if set_has[item] == nil then
			table.insert(set, item)
			set_has[item] = true
		end
	end
end

local path_has_any = function (arr, path)
	while path ~= nil do
		for _, match in ipairs(arr) do
			if glob(match, path) then
				return true
			end
		end
		path = path:match('(.+/).+')
	end
	return false
end

-- does not do globstars or character classes
-- but neither does github CODEOWNERS
local regexify_glob = function (s)
	s = s:gsub('([.^$+])', '\\%1')

	local out = ''
	for substr in s:gmatch('([^/]+/?)') do
		local first = substr:sub(1, 1)
		if first == '?' then
			first = '[^./]'
		elseif first == '*' then
			first = '([^./][^/]+|)'
		end
		substr = first..substr:sub(2)

		-- runs twice to handle repeated occurrences
		substr = substr:gsub('([^\\])?', '%1[^/]')
		substr = substr:gsub(']%?', '][^/]')
		substr = substr:gsub('([^\\])%*', '%1[^/]*?')
		substr = substr:gsub('%?%*', '?[^/]*?')
		out = out..substr
	end

	return out
end

--
-- Script functions
--

local fns = {}

fns.maintainers = function (path)
	local out = {}
	local out_has = {}

	for _, group in pairs(groups) do
		if not path_has_any(group.except, path)
			and path_has_any(group.watch, path) then
			set_insert_all(out, group.members, out_has)
		end
	end

	table.sort(out)
	foreach(out, print)
end

fns.watched = function (person_id)
	local watch = {}
	local except = {}
	local watch_has = {}
	local except_has = {}

	for _, group in pairs(groups) do
		for _, member in ipairs(group.members) do
			if member == person_id then
				set_insert_all(watch, group.watch, watch_has)
				set_insert_all(except, group.except, except_has)
				break
			end
		end
	end

	table.sort(watch)
	table.sort(except)

	foreach(watch, function (path) print('watch', path) end)
	foreach(except, function (path) print('except', path) end)
end

fns.userinfo = function (person_id)
	if people[person_id] ~= nil then
		local keys = {}
		for k, _ in pairs(people[person_id]) do
			table.insert(keys, k)
		end
		table.sort(keys)
		for _, key in ipairs(keys) do
			print(key, people[person_id][key])
		end
	end
end

fns.update = function (target)
	local target_fn = {}

	local write_names = function (id_arr, name_path)
		local name_arr = {}
		local has = {}
		for _, id in ipairs(id_arr) do
			if has[id] == nil and people[id] ~= nil and people[id][name_path] ~= nil then
				table.insert(name_arr, people[id][name_path])
				has[id] = true
			end
		end
		table.sort(name_arr)
		foreach(name_arr, function (name) io.write(' ', name) end)
	end -- names_sorted()

	--[[
	github CODEOWNERS has behaviour where a more specific path will
	"steal from" a less specific one. Given:

	foo/ @a
	foo/bar/ @b

	@a will not be notified for any changes in foo/bar/.
	Most of the logic here is to work around that, by adding watchers from
	less specific paths to the more specific paths, unless excluded.
	--]]
	target_fn.github = function ()
		-- checks if adding path to watchers would steal from an existing path
		local checked_add_path = function (path, watchers, sorter)
			if watchers[path] ~= nil then
				return
			end

			-- iterate backward through the path
			-- e.g. 'path/to/a/thing' -> check 'path/to/a/', 'path/to/', etc.
			local to_add = {}
			local ancestor = path
			while ancestor ~= nil do
				for epath, emembers in pairs(watchers) do
					if glob(epath, ancestor) or glob(ancestor, epath) then
						foreach(emembers, function (member) table.insert(to_add, member) end)
					end
				end
				ancestor = ancestor:match('(.+/).+')
			end

			watchers[path] = {}
			table.insert(sorter, path)

			foreach(to_add, function (w) table.insert(watchers[path], w) end)
		end -- checked_add_path()

		local is_child_of = function (maybe_child, of)
			local s = ''
			for sub in maybe_child:gmatch('..-/') do
				s = s..sub
				if glob(of, s) or glob(s, of) then return true end
			end
			return false
		end -- is_child_of()

		local gh_table = {}
		local gh_paths = {}
		for _, group in pairs(groups) do
			for _, except_path in ipairs(group.except) do
				checked_add_path(except_path, gh_table, gh_paths)
			end
			for _, path in ipairs(group.watch) do
				-- check if anybody is stealing from us
				for _, old_path in ipairs(gh_paths) do
					if is_child_of(old_path, path) and not path_has_any(group.except, old_path) then
						foreach(group.members, function (m) table.insert(gh_table[old_path], m) end)
					end
				end

				checked_add_path(path, gh_table, gh_paths)
				foreach(group.members, function (m) table.insert(gh_table[path], m) end)
			end
		end

		table.sort(gh_paths)

		-- output to file
		io.write('# Do not edit this file manually. Edit MAINTAINERS.ucl\n')
		io.write('# and run `tools/lua/MAINTAINERS.lua update github`\n')
		for _, path in ipairs(gh_paths) do
			io.write(path)
			write_names(gh_table[path], 'github')
			io.write('\n')
		end
	end -- github()

	target_fn.forgejo = function ()
		-- sort groups to ensure consistent ordering
		local group_ids = {}
		for group_id, _ in pairs(groups) do
			table.insert(group_ids, group_id)
		end
		table.sort(group_ids)

		io.write('# Do not edit this file manually. Edit MAINTAINERS.ucl\n')
		io.write('# and run `tools/lua/MAINTAINERS.lua update forgejo`\n')

		-- this currently does not handle `except`
		for _, group_id in ipairs(group_ids) do
			local g = groups[group_id]
			for _, path in ipairs(g.watch) do
				io.write(regexify_glob(path))
				write_names(g.members, 'forgejo')
				io.write('\n')
			end
		end
	end -- forgejo()

	io.output(io.stdout)
	if not pcall(target_fn[target]) then
		error('Unknown target for update: '..target)
	end
end -- update()

-- attempts to call fns[arg[1]] and gives error if failed
if not pcall(function() fns[arg[1]](arg[2]:lower()) end) then
	error('Unknown command: '..arg[1])
end
