#/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Dell EMC Isilon
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# ELF32/64 address map randomization test.

# "panic: page 0xfffff8103971b840 has queue state" seen in WiP kernel code.
# https://people.freebsd.org/~pho/stress/log/mark087.txt

old32=`sysctl -ni kern.elf32.aslr.enable`
old64=`sysctl -ni kern.elf64.aslr.enable`

if [ -n "$old32" ]; then
	sysctl kern.elf32.aslr.enable=$((1 - old32))
	./su.sh
	sysctl kern.elf32.aslr.enable=$old32
elif [ -n "$old64" ]; then
	sysctl kern.elf64.aslr.enable=$((1 - old64))
	./su.sh
	sysctl kern.elf64.aslr.enable=$old64
fi
