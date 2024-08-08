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

    -- Keep track of the system call numbers and make sure there's no skipped 
    -- system calls.
    local num = 0

	if file == nil then
		print "No file"
		return
	end

	self.syscalls = {}

	local fh = io.open(file)
	if fh == nil then
		print("Failed to open " .. file)
		return {}
	end

	local incs = ""
	local defs = ""
	local s
	for line in fh:lines() do
		line = line:gsub(commentExpr, "") -- Strip any comments

		-- NOTE: Can't use pure pattern matching here because of the 's' test
		-- and this is shorter than a generic pattern matching pattern
		if line == nil or line == "" then
			-- nothing blank line or end of file
		elseif s ~= nil then
			-- If we have a partial system call object
			-- s, then feed it one more line
			if s:add(line) then
				-- append to syscall list
				for t in s:iter() do
					table.insert(self.syscalls, t)
				end
				s = nil
			end
		elseif line:match("^%s*%$") then
			-- nothing, obsolete $FreeBSD$ thing
		elseif line:match("^#%s*include") then
			incs = incs .. line .. "\n"
		elseif line:match("%%ABI_HEADERS%%") then
			local h = self.config.abi_headers
			if h ~= nil and h ~= "" then
				incs = incs .. h .. "\n"
			end
		elseif line:match("^#%s*define") then
			defs = defs .. line.. "\n"
		elseif line:match("^#") then
			util.abort(1, "Unsupported cpp op " .. line)
		else
			s = syscall:new()
			if s:add(line) then
				-- append to syscall list
				for t in s:iter() do
                    if t:validate(t.num - 1) then
					    table.insert(self.syscalls, t)
                    else
                        util.abort(1, "Skipped system call at number " .. t.num)
                    end
				end
				s = nil
            end
		end
	end

    -- special handling for linux nosys
    if config.syscallprefix:find("LINUX") ~= nil then
        s = nil
    end

	if s ~= nil then
		util.abort(1, "Dangling system call at the end")
	end

	assert(fh:close())
	self.includes = incs
	self.defines = defs
end

function FreeBSDSyscall:new(obj)
	obj = obj or {}
	setmetatable(obj, self)
	self.__index = self
    
    obj:processCompat()
	obj:parseSysfile()

	return obj
end

return FreeBSDSyscall
