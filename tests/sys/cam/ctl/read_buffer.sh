# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Axcient
# All rights reserved.
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
# THIS DOCUMENTATION IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Not tested
# * modes other than "Data" and "Desc".  We don't support those.
# * Buffer ID other than 0.  We don't support those.
# * The Mode Specific field.  We don't support it.

. $(atf_get_srcdir)/ctl.subr

atf_test_case basic cleanup
basic_head()
{
	atf_set "descr" "READ BUFFER can retrieve data previously written by WRITE BUFFER"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_read_buffer sg_write_buffer
}
basic_body()
{
	create_ramdisk

	# Write to its buffer
	cp /etc/passwd input
	len=`wc -c input | cut -wf 2`
	atf_check -o ignore sg_write_buffer --mode data --in=input /dev/$dev

	# Read it back
	atf_check -o save:output sg_read_buffer --mode data -l $len --raw /dev/$dev

	# And verify
	if ! diff -q input output; then
		atf_fail "Miscompare!"
	fi
}
basic_cleanup()
{
	cleanup
}

# Read from the Descriptor mode.  Along with Data, these are the only two modes
# we support.
atf_test_case desc cleanup
desc_head()
{
	atf_set "descr" "READ BUFFER can retrieve the buffer size via the DESCRIPTOR mode"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_read_buffer
}
desc_body()
{
	create_ramdisk

	atf_check -o inline:" 00     00 04 00 00\n" sg_read_buffer --hex --mode desc /dev/$dev
}
desc_cleanup()
{
	cleanup
}

atf_test_case length cleanup
length_head()
{
	atf_set "descr" "READ BUFFER can limit its length with the LENGTH field"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_read_buffer sg_write_buffer
}
length_body()
{
	create_ramdisk

	# Write to its buffer
	atf_check -o ignore -e ignore dd if=/dev/random of=input bs=4096 count=1
	atf_check -o ignore -e ignore dd if=input bs=2048 count=1 of=expected
	atf_check -o ignore sg_write_buffer --mode data --in=input /dev/$dev

	# Read it back
	atf_check -o save:output sg_read_buffer --mode data -l 2048 --raw /dev/$dev

	# And verify
	if ! diff -q expected output; then
		atf_fail "Miscompare!"
	fi
}
length_cleanup()
{
	cleanup
}

atf_test_case offset cleanup
offset_head()
{
	atf_set "descr" "READ BUFFER accepts the BUFFER OFFSET field"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_read_buffer sg_write_buffer
}
offset_body()
{
	create_ramdisk

	# Write to its buffer
	atf_check -o ignore -e ignore dd if=/dev/random of=input bs=4096 count=1
	atf_check -o ignore -e ignore dd if=input iseek=2 bs=512 count=1 of=expected
	atf_check -o ignore sg_write_buffer --mode data --in=input /dev/$dev

	# Read it back
	atf_check -o save:output sg_read_buffer --mode data -l 512 -o 1024 --raw /dev/$dev

	# And verify
	if ! diff -q expected output; then
		atf_fail "Miscompare!"
	fi
}
offset_cleanup()
{
	cleanup
}

atf_test_case uninitialized cleanup
uninitialized_head()
{
	atf_set "descr" "READ BUFFER buffers are zero-initialized"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_read_buffer
}
uninitialized_body()
{
	create_ramdisk

	# Read an uninitialized buffer
	atf_check -o save:output sg_read_buffer --mode data -l 262144 --raw /dev/$dev

	# And verify
	atf_check -o ignore -e ignore dd if=/dev/zero bs=262144 count=1 of=expected
	if ! diff -q expected output; then
		atf_fail "Miscompare!"
	fi
}
uninitialized_cleanup()
{
	cleanup
}

atf_init_test_cases()
{
	atf_add_test_case basic
	atf_add_test_case desc
	atf_add_test_case length
	atf_add_test_case offset
	atf_add_test_case uninitialized
}
