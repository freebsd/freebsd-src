--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2021-2024 SRI International
-- Copyright (c) 2024 Tyler Baxter <agge@FreeBSD.org>
-- Copyright (c) 2023 Warner Losh <imp@bsdimp.com>
-- Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
--

local config = require("config")
local util = require("tools.util")

local scarg = {}

scarg.__index = scarg

-- Check this argument against config for ABI changes from native. Return TRUE
-- if there are.
local function checkAbiChanges(arg)
	for k, v in pairs(config.known_abi_flags) do
		if config.abiChanges(k) and v ~= nil then
			for _, e in pairs(v) do
				if arg:find(e) then
					return true
				end
			end
		end
	end
	return false
end

-- Strips the Microsoft(R) SAL annotations from this argument.
local function stripArgAnnotations(arg)
	arg = arg:gsub("_Contains_[^ ]*[_)] ?", "")
	arg = arg:gsub("_In[^ ]*[_)] ?", "")
	arg = arg:gsub("_Out[^ ]*[_)] ?", "")
	return util.trim(arg)
end

-- Preprocessing of this argument.
function scarg:init(line)
	-- Trim whitespace and trailing comma. We don't want them here;
	-- these can mess with our processing of this argument.
	line = util.trim(line)	-- This provides a clearer abort error.
	self.scarg = util.trim(line, ',')

	self.arg_abi_change = checkAbiChanges(self.scarg)
	self.changes_abi = self.arg_abi_change
	self.scarg = stripArgAnnotations(self.scarg)

	self.name = self.scarg:match("([^* ]+)$")
	-- Our pattern might produce a Lua pattern sequence; that's a malformed
	-- declaration.
	local status, type = pcall(function()
		return util.trim(self.scarg:gsub(self.name .. "$", ""), nil)
	end)
	if not status then
		util.abort(1, "Malformed argument line: " .. line)
	end
	self.type = type
end

-- Processes this argument.
-- Flags if there's ABI changes from native, converts this argument to the
-- target ABI, and handles 64-bit argument pairing.
-- Returns TRUE if this argument is processed and ready to add.
-- Returns FALSE if it shouldn't be added (the argument type is void).
function scarg:process()
	if self.type ~= "" and self.name ~= "void" then
		-- util.is64bitType() needs a bare type so check it after
		-- argname is removed.
		self.changes_abi = self.changes_abi or
			(config.abiChanges("pair_64bit") and
			util.is64bitType(self.type))

		self.type = self.type:gsub("intptr_t", config.abi_intptr_t)
		self.type = self.type:gsub("semid_t", config.abi_semid_t)

		if util.isPtrType(self.type) then
			self.type = self.type:gsub("size_t", config.abi_size_t)
			self.type = self.type:gsub("^long", config.abi_long)
			self.type = self.type:gsub("^u_long", config.abi_u_long)
			self.type = self.type:gsub("^const u_long", "const " ..
			   config.abi_u_long)
		elseif self.type:find("^long$") then
			self.type = config.abi_long
		end

		if util.isPtrArrayType(self.type) and
		   config.abi_ptr_array_t ~= "" then
			-- `* const *` -> `**`
			self.type = self.type:gsub("[*][ ]*const[ ]*[*]", "**")
			-- e.g., `struct aiocb **` -> `uint32_t *`
			self.type = self.type:gsub("[^*]*[*]",
			   config.abi_ptr_array_t .. " ", 1)
		end

		if self.arg_abi_change then
			self.type = self.type:gsub("(struct [^ ]*)", "%1" ..
			    config.abi_type_suffix)
			self.type = self.type:gsub("(union [^ ]*)", "%1" ..
			    config.abi_type_suffix)
		end
		return true
	end
	return false
end

-- For pairing 64-bit arguments, pad if necessary.
-- Returns TRUE if this argument was padded.
local function pad(tbl)
	if #tbl % 2 == 1 then
		table.insert(tbl, {
			type = "int",
			name = "_pad",
		})
		return true
	end
	return false
end

-- To append to a system call's argument table. Appends to the end.
function scarg:append(tbl)
	if config.abiChanges("pair_64bit") and util.is64bitType(self.type) then
		pad(tbl)	-- Needs argument padding.
		table.insert(tbl, {
			type = "uint32_t",
			name = self.name .. "1",
		})
		table.insert(tbl, {
			type = "uint32_t",
			name = self.name .. "2",
		})
	else
		table.insert(tbl, {
			type = self.type,
			name = self.name,
		})
	end
end

-- Returns TRUE if this argument has ABI changes from native.
-- EXAMPLE: 32-bit argument for freebsd32.
function scarg:changesAbi()
	return self.changes_abi
end

function scarg:new(obj, line)
	obj = obj or { }
	setmetatable(obj, self)
	self.__index = self

	-- ABI changes that we only want in this scope.
	self.arg_abi_change = false
	-- ABI changes that we want the system call object to see.
	self.changes_abi = false

	obj:init(line)

	return obj
end

return scarg
