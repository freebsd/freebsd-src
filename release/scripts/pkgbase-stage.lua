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
	local components = {}
	local rquery = capture(pkg .. "rquery -U -r FreeBSD-base %n")
	for package in rquery:gmatch("[^\n]+") do
		local set = package:match("^FreeBSD%-set%-(.*)$")
		if set then
			components[set] = package
		-- Kernels other than FreeBSD-kernel-generic are ignored
		-- Note that on powerpc64 and powerpc64le the names are
		-- slightly different.
		elseif package:match("^FreeBSD%-kernel%-generic.*-dbg") then
			components["kernel-dbg"] = package
		elseif package:match("^FreeBSD%-kernel%-generic.*") then
			components["kernel"] = package
		end
	end
	assert(components["kernel"])
	assert(components["base"])

	local selected = {}
	if media == "disc" then
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

	assert(os.execute(pkg .. "fetch -d -o " .. target .. " " .. table.concat(packages, " ")))
	assert(os.execute(pkg .. "repo " .. target))
end

main()
