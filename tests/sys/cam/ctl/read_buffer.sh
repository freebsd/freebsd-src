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

load_modules() {
	if ! kldstat -q -m ctl; then
		kldload ctl || atf_skip "could not load ctl kernel mod"
	fi
	if ! ctladm port -o on -p 0; then
		atf_skip "could not enable the camsim frontend"
	fi
}

find_da_device() {
	SERIAL=$1

	# Rescan camsim
	# XXX  camsim doesn't update when creating a new device.  Worse, a
	# rescan won't look for new devices.  So we must disable/re-enable it.
	# Worse still, enabling it isn't synchronous, so we need a retry loop
	# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=281000
	retries=5
	ctladm port -o off -p 0 >/dev/null
	ctladm port -o on -p 0 >/dev/null
	while true; do

		# Find the corresponding da device
		da=`geom disk list | awk -v serial=$SERIAL ' /Geom name:/ { devname=$NF } /ident:/ && $NF ~ serial { print devname; exit } '`
		if [ -z "$da" ]; then
			retries=$(( $retries - 1 ))
			if [ $retries -eq 0 ]; then
				cat lun-create.txt
				geom disk list
				atf_fail "Could not find da device"
			fi
			sleep 0.1
			continue
		fi
		break
	done
}

# Create a CTL LUN
create_ramdisk() {
	atf_check -o save:lun-create.txt ctladm create -b ramdisk -s 1048576
	atf_check egrep -q "LUN created successfully" lun-create.txt
	SERIAL=`awk '/Serial Number:/ {print $NF}' lun-create.txt`
	if [ -z "$SERIAL" ]; then
		atf_fail "Could not find serial number"
	fi
	find_da_device $SERIAL
}

cleanup() {
	if [ -e "lun-create.txt" ]; then
		lun_id=`awk '/LUN ID:/ {print $NF}' lun-create.txt`
		ctladm remove -b ramdisk -l $lun_id > /dev/null
	fi
}

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
	atf_check -o ignore sg_write_buffer --mode data --in=input /dev/$da

	# Read it back
	atf_check -o save:output sg_read_buffer --mode data -l $len --raw /dev/$da

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

	atf_check -o inline:" 00     00 04 00 00\n" sg_read_buffer --hex --mode desc /dev/$da
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
	atf_check -o ignore sg_write_buffer --mode data --in=input /dev/$da

	# Read it back
	atf_check -o save:output sg_read_buffer --mode data -l 2048 --raw /dev/$da

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
	atf_check -o ignore sg_write_buffer --mode data --in=input /dev/$da

	# Read it back
	atf_check -o save:output sg_read_buffer --mode data -l 512 -o 1024 --raw /dev/$da

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
	atf_check -o save:output sg_read_buffer --mode data -l 262144 --raw /dev/$da

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
