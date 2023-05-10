#!/bin/sh

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

# Bug 237269 - panic in glabel (g_label_destroy) stop after resizing GPT
# partition.
# "panic: vm_fault_hold: fault on nofault entry, addr: 0 from
# g_slice_spoiled+0x7"
# Test scenario by andrew@tao11.riddles.org.uk

[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1
md_unit=$(mdconfig -t swap -s 30MB)
geom part create -s GPT "$md_unit"
geom part add -s 10M -t linux-swap -l tst0 "$md_unit"
geom part resize -i 1 -s 20M "$md_unit"

# at this point "glabel status" shows two gpt/tst0 entries,
# one of which has no consumer; trying to correct this causes
# a panic:

glabel stop gpt/tst0
glabel stop gpt/tst0  # BOOM

mdconfig -d -u $md_unit

exit 0
