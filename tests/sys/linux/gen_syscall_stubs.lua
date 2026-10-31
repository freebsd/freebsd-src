#!/usr/libexec/flua
--
-- SPDX-License-Identifier: BSD-2-Clause
--
-- Copyright (c) 2026 Vladimir Grebenshchikov <vova@zote.me>
--

if #arg ~= 3 then
	error("usage: gen_syscall_stubs.lua srctop arch syscalls.master")
end

package.path = package.path .. ";" .. arg[1] ..
	"/sys/tools/syscalls/?.lua"

local config = require("config")
local FreeBSDSyscall = require("core.freebsd-syscall")

config.mergeCompat()

local arch = arg[2]
local tbl = FreeBSDSyscall:new({sysfile = arg[3], config = config})
local syscalls = {}

for _, syscall in pairs(tbl.syscalls) do
	if syscall:native() then
		local symbol = syscall.name:gsub("^linux_", "")
		if syscalls[symbol] ~= nil then
			error("duplicate syscall symbol: " .. symbol)
		end
		syscalls[symbol] = syscall.num
	end
end

local function emit_stub(symbol, number)
	local function_type = arch == "amd64" and "@function" or "%function"
	local section_type = arch == "amd64" and "@progbits" or "%progbits"

	io.write("\n\t.section\t.text.", symbol, ",\"ax\",", section_type,
	    "\n")
	io.write("\t.globl\t", symbol, "\n")
	io.write("\t.type\t", symbol, ",", function_type, "\n")
	io.write(symbol, ":\n")
	if arch == "amd64" then
		io.write("\tmovq\t$", number, ",%rax\n")
		io.write("\tmovq\t%rcx,%r10\n")
		io.write("\tsyscall\n")
		io.write("\tcmpq\t$-4095,%rax\n")
		io.write("\tjae\t.Llinux_syscall_error\n")
	elseif arch == "arm64" then
		io.write("\tmov\tx8,#", number, "\n")
		io.write("\tsvc\t#0\n")
		io.write("\tcmn\tx0,#4095\n")
		io.write("\tb.cs\t.Llinux_syscall_error\n")
	else
		error("unsupported architecture: " .. arch)
	end
	io.write("\tret\n")
	io.write("\t.size\t", symbol, ",.-", symbol, "\n")
end

local function emit_error_handler()
	local section_type = arch == "amd64" and "@progbits" or "%progbits"

	io.write("\n\t.section\t.text.linux_syscall_error,\"ax\",",
	    section_type, "\n")
	io.write(".Llinux_syscall_error:\n")
	if arch == "amd64" then
		io.write("\tnegq\t%rax\n")
		io.write("\tmovl\t%eax,errno(%rip)\n")
		io.write("\tmovq\t$-1,%rax\n")
	elseif arch == "arm64" then
		io.write("\tneg\tw1,w0\n")
		io.write("\tadrp\tx2,errno\n")
		io.write("\tstr\tw1,[x2,:lo12:errno]\n")
		io.write("\tmov\tx0,#-1\n")
	end
	io.write("\tret\n")
end

io.write("/* Generated from ", arg[3], "; do not edit. */\n")
local symbols = {}
for symbol in pairs(syscalls) do
	table.insert(symbols, symbol)
end
table.sort(symbols)
for _, symbol in ipairs(symbols) do
	emit_stub(symbol, syscalls[symbol])
end
emit_error_handler()

if arch == "amd64" then
	io.write("\n\t.section .note.GNU-stack,\"\",@progbits\n")
else
	io.write("\n\t.section .note.GNU-stack,\"\",%progbits\n")
end
