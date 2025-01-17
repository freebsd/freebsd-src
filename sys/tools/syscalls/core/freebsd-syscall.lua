--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2024 Tyler Baxter <agge@FreeBSD.org>
-- Copyright (c) 2023 Warner Losh <imp@bsdimp.com>
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--

local syscall = require("core.syscall")
local util = require("tools.util")

local FreeBSDSyscall = {}

FreeBSDSyscall.__index = FreeBSDSyscall

-- For each compat option in the provided config table, process them and insert
-- them into known_flags for class syscall.
function FreeBSDSyscall:processCompat()
	for _, v in pairs(self.config.compat_options) do
		if v.stdcompat ~= nil then
			local stdcompat = v.stdcompat
			v.definition = "COMPAT_" .. stdcompat:upper()
			v.compatlevel = tonumber(stdcompat:match("([0-9]+)$"))
			v.flag = stdcompat:gsub("FREEBSD", "COMPAT")
			v.prefix = stdcompat:lower() .. "_"
			v.descr = stdcompat:lower()
		end

		-- Add compat option to syscall.known_flags.
		table.insert(syscall.known_flags, v.flag)
	end
end

function FreeBSDSyscall:parseSysfile()
	local file = self.sysfile
	local config = self.config
	local commentExpr = "^%s*;.*"

	if file == nil then
		return nil, "No file given"
	end

	self.syscalls = {}

	local fh, msg = io.open(file)
	if fh == nil then
		return nil, msg
	end

	local incs = ""
	local prolog = ""
	local first = true
	local cpp_warned = false
	local s
	for line in fh:lines() do
		line = line:gsub(commentExpr, "") -- Strip any comments.
		-- NOTE: Can't use pure pattern matching here because of
		-- the 's' test and this is shorter than a generic pattern
		-- matching pattern.
		if line == nil or line == "" then
			goto skip	-- Blank line, skip this line.
		elseif s ~= nil then
			-- If we have a partial system call object s,
			-- then feed it one more line.
			if s:add(line) then
				-- Append to system call list.
				for t in s:iter() do
					if t:validate(t.num - 1) then
						table.insert(self.syscalls, t)
					else
						util.abort(1,
						    "Skipped system call " ..
						    "at number " .. t.num)
					end
				end
				s = nil
			end
		elseif line:match("^#%s*include") then
			incs = incs .. line .. "\n"
		elseif line:match("%%ABI_HEADERS%%") then
			local h = self.config.abi_headers
			if h ~= nil and h ~= "" then
				incs = incs .. h .. "\n"
			end
		elseif line:match("^#") then
			if not cpp_warned then
				util.warn("use of non-include cpp " ..
				    "directives is deprecated")
				cpp_warned = true
			end
			prolog = prolog .. line .. "\n"
		else
			s = syscall:new()
			if first then
				self.prolog = prolog
				s.prolog = ""
				first = false
			else
				s.prolog = prolog
			end
			prolog = ""
			if s:add(line) then
				-- Append to system call list.
				for t in s:iter() do
					if t:validate(t.num - 1) then
						table.insert(self.syscalls, t)
					else
						util.abort(1,
						    "Skipped system call " ..
						    "at number " .. t.num)
					end
				end
				s = nil
			end
		end
		::skip::
	end

	-- Special handling for linux nosys.
	if config.syscallprefix:find("LINUX") ~= nil then
		s = nil
	end

	if s ~= nil then
		util.abort(1, "Dangling system call at the end")
	end

	assert(fh:close())
	self.includes = incs
	self.epilog = prolog

	if self.prolog ~= "" then
		util.warn("non-include pre-processor directives in the " ..
		    "config prolog will not appear in generated output:\n" ..
		    self.prolog)
	end
end

function FreeBSDSyscall:findStructs()
	self.structs = {}

	for _, s in pairs(self.syscalls) do
		if s:native() and not s.type.NODEF then
			for _, v in ipairs(s.args) do
				local name = util.structName(v.type)
				if name ~= nil then
					self.structs[name] = name
				end
			end
		end
	end
end

function FreeBSDSyscall:new(obj)
	obj = obj or {}
	setmetatable(obj, self)
	self.__index = self

	obj:processCompat()
	obj:parseSysfile()
	obj:findStructs()

	return obj
end

return FreeBSDSyscall
