#!/usr/libexec/flua

-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright(c) 2025 The FreeBSD Foundation.
--
-- This software was developed by Isaac Freund <ifreund@freebsdfoundation.org>
-- under sponsorship from the FreeBSD Foundation.

-- Run a command using the OS shell and capture the stdout
-- Strips exactly one trailing newline if present, does not strip any other whitespace.
-- Asserts that the command exits cleanly
local function capture(command)
	local p = io.popen(command)
	local output = p:read("*a")
	assert(p:close())
	-- Strip exactly one trailing newline from the output, if there is one
	return output:match("(.-)\n$") or output
end

local function append_list(list, other)
	for _, item in ipairs(other) do
		table.insert(list, item)
	end
end

-- Returns a list of packages to be included in the given media
local function select_packages(pkg, media, all_libcompats)
	local components = {
		kernel = {},
		kernel_dbg = {},
		base = {},
		base_dbg = {},
		src = {},
		tests = {},
	}

	for compat in all_libcompats:gmatch("%S+") do
		components["lib" .. compat] = {}
		components["lib" .. compat .. "_dbg"] = {}
	end

	local rquery = capture(pkg .. "rquery -U -r FreeBSD-base %n")
	for package in rquery:gmatch("[^\n]+") do
		if package == "FreeBSD-src" or package:match("^FreeBSD%-src%-.*") then
			table.insert(components["src"], package)
		elseif package == "FreeBSD-tests" or package:match("^FreeBSD%-tests%-.*") then
			table.insert(components["tests"], package)
		elseif package:match("^FreeBSD%-kernel%-.*") and
			package ~= "FreeBSD-kernel-man"
		then
			-- Kernels other than FreeBSD-kernel-generic are ignored
			if package == "FreeBSD-kernel-generic" then
				table.insert(components["kernel"], package)
			elseif package == "FreeBSD-kernel-generic-dbg" then
				table.insert(components["kernel_dbg"], package)
			end
		elseif package:match(".*%-dbg$") then
			table.insert(components["base_dbg"], package)
		else
			local found = false
			for compat in all_libcompats:gmatch("%S+") do
				if package:match(".*%-dbg%-lib" .. compat .. "$") then
					table.insert(components["lib" .. compat .. "_dbg"], package)
					found = true
					break
				elseif package:match(".*%-lib" .. compat .. "$") then
					table.insert(components["lib" .. compat], package)
					found = true
					break
				end
			end
			if not found then
				table.insert(components["base"], package)
			end
		end
	end
	assert(#components["kernel"] == 1)
	assert(#components["base"] > 0)

	local selected = {}
	if media == "disc" then
		append_list(selected, components["base"])
		append_list(selected, components["kernel"])
		append_list(selected, components["kernel_dbg"])
		append_list(selected, components["src"])
		append_list(selected, components["tests"])
		for compat in all_libcompats:gmatch("%S+") do
			append_list(selected, components["lib" .. compat])
		end
	else
		assert(media == "dvd")
		append_list(selected, components["base"])
		append_list(selected, components["base_dbg"])
		append_list(selected, components["kernel"])
		append_list(selected, components["kernel_dbg"])
		append_list(selected, components["src"])
		append_list(selected, components["tests"])
		for compat in all_libcompats:gmatch("%S+") do
			append_list(selected, components["lib" .. compat])
			append_list(selected, components["lib" .. compat .. "_dbg"])
		end
	end

	return selected
end

local function main()
	-- Determines package subset selected
	local media = assert(arg[1])
	assert(media == "disc" or media == "dvd")
	-- Local repository to fetch from
	local source = assert(arg[2])
	-- Directory to create new repository
	local target = assert(arg[3])
	-- =hitespace separated list of all libcompat names (e.g. "32")
	local all_libcompats = assert(arg[4])
	-- ABI of repository
	local ABI = assert(arg[5])

	assert(os.execute("mkdir -p pkgbase-repo-conf"))
	local f <close> = assert(io.open("pkgbase-repo-conf/FreeBSD-base.conf", "w"))
	assert(f:write(string.format([[
	FreeBSD-base: {
	  url: "file://%s",
	  enabled: yes
	}
	]], source)))
	assert(f:close())

	local pkg = "pkg -o ASSUME_ALWAYS_YES=yes -o IGNORE_OSVERSION=yes " ..
	    "-o ABI=" .. ABI .. " " ..
	    "-o INSTALL_AS_USER=1 -o PKG_DBDIR=./pkgdb -R ./pkgbase-repo-conf "

	assert(os.execute(pkg .. "update"))

	local packages = select_packages(pkg, media, all_libcompats)

	assert(os.execute(pkg .. "fetch -o " .. target .. " " .. table.concat(packages, " ")))
	assert(os.execute(pkg .. "repo " .. target))
end

main()
