#!/usr/libexec/flua

-- MFC candidate script utility - $0 from-file to-file
--
-- from-file specifies hashes that exist only in the "MFC from" branch and
-- to-file specifies the original hashes of commits already merged to the
-- "MFC to" branch.

-- SPDX-License-Identifier: BSD-2-Clause
-- Copyright 2024 The FreeBSD Foundation

-- Read a file and return its content as a table
local function read_file(filename)
	local file = assert(io.open(filename, "r"))
	local content = {}
	for line in file:lines() do
		table.insert(content, line)
	end
	file:close()
	return content
end

-- Remove hashes from 'set1' list that are present in 'set2' list
local function set_difference(set1, set2)
	local set2_values = {}
	for _, value in ipairs(set2) do
		set2_values[value] = true
	end

	local result = {}
	for _, value in ipairs(set1) do
		if not set2_values[value] then
			table.insert(result, value)
		end
	end
	return result
end

-- Main function
local function main()
	local from_file = arg[1]
	local to_file = arg[2]
	local exclude_file = arg[3]

	if not from_file or not to_file then
		print("Usage: flua $0 from-file to-file")
		return
	end

	local from_hashes = read_file(from_file)
	local to_hashes = read_file(to_file)

	local result_hashes = set_difference(from_hashes, to_hashes)

	if exclude_file then
		exclude_hashes = read_file(exclude_file)
		result_hashes = set_difference(result_hashes, exclude_hashes)
	end

	-- Print the result
	for _, hash in ipairs(result_hashes) do
		print(hash)
	end
end

main()
