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

-- Returns a list of packages to be included in the given media
local function select_packages(pkg, media, all_libcompats)
	-- Note: if you update this list, you must also update the list in
	-- usr.sbin/bsdinstall/scripts/pkgbase.in.
	local kernel_packages = {
		-- Most architectures use this
		["FreeBSD-kernel-generic"] = true,
		-- PowerPC uses either of these, depending on platform
		["FreeBSD-kernel-generic64"] = true,
		["FreeBSD-kernel-generic64le"] = true,
	}

	local components = {}
	local rquery = capture(pkg .. "rquery -U -r FreeBSD-base %n")
	for package in rquery:gmatch("[^\n]+") do
		local set = package:match("^FreeBSD%-set%-(.*)$")
		if set then
			components[set] = package
		elseif kernel_packages[package] then
			components["kernel"] = package
		elseif kernel_packages[package:match("(.*)%-dbg$")] then
			components["kernel-dbg"] = package
		elseif package == "pkg" then
			components["pkg"] = package
		end
	end
	assert(components["kernel"])
	assert(components["base"])
	assert(components["pkg"])

	local selected = {}
	if media == "disc" then
		table.insert(selected, components["pkg"])
		table.insert(selected, components["base"])
		table.insert(selected, components["kernel"])
		table.insert(selected, components["kernel-dbg"])
		table.insert(selected, components["src"])
		table.insert(selected, components["tests"])
		for compat in all_libcompats:gmatch("%S+") do
			table.insert(selected, components["lib" .. compat])
		end
	else
		assert(media == "dvd")
		table.insert(selected, components["pkg"])
		table.insert(selected, components["base"])
		table.insert(selected, components["base-dbg"])
		table.insert(selected, components["kernel"])
		table.insert(selected, components["kernel-dbg"])
		table.insert(selected, components["src"])
		table.insert(selected, components["tests"])
		for compat in all_libcompats:gmatch("%S+") do
			table.insert(selected, components["lib" .. compat])
			table.insert(selected, components["lib" .. compat .. "-dbg"])
		end
	end

	return selected
end

local function main()
	-- Determines package subset selected
	local media = assert(arg[1])
	assert(media == "disc" or media == "dvd")
	-- Directory containing FreeBSD-base repository config
	local repo_dir = assert(arg[2])
	-- Directory to create new repository
	local target = assert(arg[3])
	-- Whitespace separated list of all libcompat names (e.g. "32")
	local all_libcompats = assert(arg[4])
	-- ABI of repository
	local ABI = assert(arg[5])
	-- pkgdb to use
	local PKGDB = assert(arg[6])

	local pkg = "pkg -o ASSUME_ALWAYS_YES=yes -o IGNORE_OSVERSION=yes " ..
	    "-o ABI=" .. ABI .. " " ..
	    "-o INSTALL_AS_USER=1 -o PKG_DBDIR=" .. PKGDB .. " -R " .. repo_dir .. " "

	assert(os.execute(pkg .. "update"))

	local packages = select_packages(pkg, media, all_libcompats)

	assert(os.execute(pkg .. "fetch -d -o " .. target .. " " .. table.concat(packages, " ")))
	assert(os.execute(pkg .. "repo " .. target))
end

main()
