--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2024 Tyler Baxter <agge@FreeBSD.org>
-- Copyright (c) 2023 Warner Losh <imp@bsdimp.com>
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--

local scarg = require("core.scarg")
local scret = require("core.scret")
local util = require("tools.util")

local syscall = {}

syscall.__index = syscall

syscall.known_flags = util.set {
	"STD",
	"OBSOL",
	"RESERVED",
	"UNIMPL",
	"NODEF",
	"NOARGS",
	"NOPROTO",
	"NOSTD",
	"NOTSTATIC",
	"CAPENABLED",
	"SYSMUX",
}

--
-- Processes the thread flag for this system call.
-- RETURN: String thr, the appropriate thread flag
--
local function processThr(type)
    local str = "SY_THR_STATIC"
    for k, v in pairs(type) do
        if k == "NOTSTATIC" then
            str = "SY_THR_ABSENT"
        end
    end
    return str
end

--
-- Processes the capability flag for this system call.
-- RETURN: String cap, "SYF_CAPENABLED" for capability enabled, "0" if not
--
local function processCap(name, prefix, type)
    local str = "0"
    local stripped = util.stripAbiPrefix(name, prefix)
    if config.capenabled[name] ~= nil or
       config.capenabled[stripped] ~= nil then
        str = "SYF_CAPENABLED"
    else
        for k, v in pairs(type) do
            if k == "CAPENABLED" then
                str = "SYF_CAPENABLED"
            end
        end
    end
    return str
end

-- Check that this system call has a known type.
local function checkType(line, type)
	for k, v in pairs(type) do
	    if not syscall.known_flags[k] and not
            k:match("^COMPAT") then
			util.abort(1, "Bad type: " .. k)
		end
	end
end

-- Validate that we're not skipping system calls by comparing this system call
-- number to the previous system call number. Called higher up the call stack by 
-- class FreeBSDSyscall.
function syscall:validate(prev)
    return prev + 1 == self.num
end

-- If there are ABI changes from native, process this system call to match the
-- target ABI.
-- RETURN: TRUE if any modifications were done. FALSE if no modifications were 
-- done
function syscall:processChangesAbi()
    -- First, confirm we have a valid changes_abi flag.
    if config.syscall_no_abi_change[self.name] then
        self.changes_abi = false
    end
    -- xxx subject to a rework:
    if config.abiChanges("pointer_args") then
	    for _, v in ipairs(v.args) do
	    	if util.isPtrType(v.type) then
	    		if config.syscall_no_abi_change[self.name] then
	    			print("WARNING: " .. self.name ..
	    			    " in syscall_no_abi_change, but pointers args are present")
	    		end
	    		self.changes_abi = true
	    		goto ptrfound
	    	end
	    end
	    ::ptrfound::
    end

    -- If there are ABI changes from native:
    if self.changes_abi then
        -- argalias should be:
        --   COMPAT_PREFIX + ABI Prefix + funcname
    	self.arg_prefix = config.abi_func_prefix
    	self.prefix = config.abi_func_prefix
    	self.arg_alias = self.prefix .. self.name
    	return true
    end
    return false
end

-- Native is an arbitrarily large number to have a constant and not 
-- interfere with compat numbers.
local native = 1000000

-- Return the symbol name for this system call.
function syscall:symbol()
	local c = self:compat_level()
	if self.type.OBSOL then
		return "obs_" .. self.name
	end
	if self.type.RESERVED then
		return "reserved #" .. tostring(self.num)
	end
	if self.type.UNIMPL then
		return "unimp_" .. self.name
	end
	if c == 3 then
		return "o" .. self.name
	end
	if c < native then
		return "freebsd" .. tostring(c) .. "_" .. self.name
	end
	return self.name
end

-- Return the comment for this system call.
-- TODO: Incomplete/unused
function syscall:comment()
    local c = self:compat_level()
    if self.type.OBSOL then
        return "/* obsolete " .. self.alias .. " */"
    end
    if self.type.RESERVED then
        return "/* reserved for local use */"
    end
    if self.type.UNIMPL then
        return "" -- xxx
    else
        return "/* " .. self.num .. " = " .. self.alias .. " */"
    end
end

--
-- Return the compatibility level for this system call.
-- 0 is obsolete
-- < 0 is this isn't really a system call we care about
-- 3 is 4.3BSD in theory, but anything before FreeBSD 4
-- >= 4 FreeBSD version this system call was replaced with a new version
--
function syscall:compat_level()
	if self.type.UNIMPL or self.type.RESERVED or self.type.NODEF then
		return -1
	elseif self.type.OBSOL then
		return 0
	elseif self.type.COMPAT then
		return 3
	end
	for k, v in pairs(self.type) do
		local l = k:match("^COMPAT(%d+)")
		if l ~= nil then
			return tonumber(l)
		end
	end
	return native
end
    
-- Adds the definition for this system call. Guarded by whether we already have
-- a system call number or not.
function syscall:addDef(line, words)
    if self.num == nil then
	    self.num = words[1]
	    self.audit = words[2]
	    self.type = util.setFromString(words[3], "[^|]+")
	    checkType(line, self.type)
	    self.name = words[4]
	    -- These next three are optional, and either all present or all absent
	    self.altname = words[5]
	    self.alttag = words[6]
	    self.rettype = words[7]
	    return true
    end
    return false
end

-- Adds the function declaration for this system call. If addDef() found an 
-- opening curly brace, then we're looking for a function declaration.
function syscall:addFunc(line, words)
    if self.name == "{" then
	    -- Expect line is "type syscall(" or "type syscall(void);"
        if #words ~= 2 then
            util.abort(1, "Malformed line " .. line)
        end

	    local ret = scret:new({}, line)
        self.ret = ret:add()
		-- Don't clobber rettype set in the alt information
		if self.rettype == nil then
			self.rettype = "int"
        end
    
	    self.name = words[2]:match("([%w_]+)%(")
	    if words[2]:match("%);$") then
            -- now we're looking for ending curly brace
	    	self.expect_rbrace = true
	    end
        return true
    end
    return false
end

-- Adds the argument(s) for this system call. Once addFunc() assigns a name for
-- this system call, arguments are next in syscalls.master.
function syscall:addArgs(line)
	if not self.expect_rbrace then
	    if line:match("%);$") then
	    	self.expect_rbrace = true
	    	return true
	    end
	    local arg = scarg:new({}, line)
        -- We don't want to add this argument if it doesn't process. 
        -- scarg:process() handles those conditions.
        if arg:process() then 
            arg:append(self.args)
            -- Grab ABI change information for this argument.
            self.changes_abi = arg:changesAbi()
        end
        return true
    end
    return false
end

-- Returns TRUE if this system call was added succesfully.
function syscall:isAdded(line)
    if self.expect_rbrace then
	    if not line:match("}$") then
	    	util.abort(1, "Expected '}' found '" .. line .. "' instead.")
	    end
        self:finalize()
        return true
    end
    return false
end

-- Once we have a good syscall, add some final information to it.
function syscall:finalize()
    -- These may be changed by processChangesAbi(), or they'll remain empty for 
    -- native.
    self.prefix = ""
    self.arg_prefix = ""
    self:processChangesAbi()

    -- These need to be done before modifying self.name.
    self.cap = processCap(self.name, self.prefix, self.type) -- capability flag
    self.thr = processThr(self.type) -- thread flag

    -- An empty string would not want a prefix; in that case we want to keep the
    -- empty string.
    if self.name ~= "" then
        self.name = self.prefix .. self.name
    end
    if self.alias == nil or self.alias == "" then
        self.alias = self.name
    end

    -- Assign argument alias.
    if self.arg_alias == nil and self.name ~= nil then
        -- Symbol will either be: (native) the same as the system call name, or 
        -- (non-native) the correct modified symbol for the arg_alias.
        self.arg_alias = self:symbol() .. "_args"
    elseif self.arg_alias ~= nil then 
        self.arg_alias = self.arg_prefix .. self.arg_alias
    end
end

-- Interface to add this system call to the master system call table.
-- The system call is built up one line at a time. The states describe the 
-- current parsing state.
-- Returns TRUE when ready to add and FALSE while still parsing.
function syscall:add(line)
    local words = util.split(line, "%S+")
    if self:addDef(line, words) then
        -- Cases where the syscalls.master entry is one line; we just want to 
        -- exit and add:
        if self.name ~= "{" then
            -- A NIL name should be written as an empty string.
            if self.name == nil then
                self.name = ""
            end
            self.alias = self.name -- set for all cases

            -- This system call is a range.
            if tonumber(self.num) == nil then
                return true
            -- This system call is a loadable system call.
            elseif self.altname ~= nil and self.alttag ~= nil and 
                   self.rettype ~= nil then
                self.cap = "0"
                self.thr = "SY_THR_ABSENT"
                self.arg_alias = self:symbol() .. "_args"
                self.ret = self.rettype
                return true
            -- This system call is some other one line entry.
            else
                return true
            end
        end
        return false -- Otherwise, definition added; keep going.
    end
    if self:addFunc(line, words) then
        return false -- Function added; keep going.
    end
    if self:addArgs(line) then
        return false -- Arguments added; keep going.
    end
    return self:isAdded(line) -- Final validation, before adding.
end

-- Return TRUE if this system call is native.
-- NOTE: The other system call names are also treated as native, so that's why
-- they're being allowed in here.
function syscall:native()
    return self:compat_level() == native or self.name == "lkmnosys" or 
           self.name == "sysarch"
end

function syscall:new(obj)
	obj = obj or { }
	setmetatable(obj, self)
	self.__index = self

	self.expect_rbrace = false
    self.changes_abi = false
	self.args = {}

	return obj
end

-- Make a shallow copy of `self` and replace the system call number with num 
-- (which should be a number).
-- For system call ranges.
function syscall:shallowCopy(num)
	local obj = syscall:new(obj)

	-- shallow copy
	for k, v in pairs(self) do
		obj[k] = v
	end
	obj.num = num	-- except override range
	return obj
end

-- Make a deep copy of the parameter object.
-- CREDIT: http://lua-users.org/wiki/CopyTable
-- For a full system call (the nested arguments table should be a deep copy).
local function deepCopy(orig)
    local type = type(orig)
    local copy

    if orig_type == 'table' then
        copy = {}
        for orig_key, orig_value in next, orig, nil do
            copy[deepCopy(orig_key)] = deepCopy(orig_value)
        end
        setmetatable(copy, deepCopy(getmetatable(orig)))
    else -- number, string, boolean, etc
        copy = orig
    end

    return copy
end

-- CREDIT: http://lua-users.org/wiki/CopyTable
-- Save copied tables in `copies`, indexed by original table.
function deepCopy(orig, copies)
    copies = copies or {}
    local orig_type = type(orig)
    local copy
    if orig_type == 'table' then
        if copies[orig] then
            copy = copies[orig]
        else
            copy = {}
            copies[orig] = copy
            for orig_key, orig_value in next, orig, nil do
                copy[deepCopy(orig_key, copies)] = deepCopy(orig_value, copies)
            end
            setmetatable(copy, deepCopy(getmetatable(orig), copies))
        end
    else -- number, string, boolean, etc
        copy = orig
    end
    return copy
end

--
-- As we're parsing the system calls, there's two types. Either we have a
-- specific one, that's a assigned a number, or we have a range for things like
-- reseved system calls. this function deals with both knowing that the specific
-- ones are more copy and so we should just return the object we just made w/o
-- an extra clone.
--
function syscall:iter()
	local s = tonumber(self.num)
	local e
	if s == nil then
		s, e = string.match(self.num, "(%d+)%-(%d+)")
        s, e = tonumber(s), tonumber(e)
		return function ()
			if s <= e then
				s = s + 1
				return self:shallowCopy(s - 1)
			end
		end
	else
		e = s
		self.num = s	-- Replace string with number, like the clones
		return function ()
			if s == e then
                local deep_copy = deepCopy(self)
				s = e + 1
                return deep_copy
			end
		end
	end
end

return syscall
