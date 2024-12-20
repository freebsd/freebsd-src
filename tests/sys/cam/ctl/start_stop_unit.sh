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

. $(atf_get_srcdir)/ctl.subr

# TODO:
# * format layer
# * IMM bit
# * LOEJ
# * noflush
# * power conditions

# Not Tested
# * Power Condition Modifier (not implemented in CTL)

atf_test_case eject cleanup
eject_head()
{
	atf_set "descr" "START STOP UNIT can eject a CDROM device"
	atf_set "require.user" "root"
	atf_set "require.progs" "sg_start sg_readcap ctladm"
}
eject_body()
{
	# -t 5 for CD/DVD device type
	create_ramdisk -t 5

	# Verify that the device is online
	# Too bad I don't know of any other way to check that it's stopped but
	# by using sg_readcap.
	atf_check -o ignore -e not-match:"Device not ready" sg_readcap /dev/$dev

	# eject the device
	atf_check sg_start --eject /dev/$dev

	# Ejected, it should now return ENXIO
	atf_check -s exit:1 -o ignore -e match:"Device not configured" dd if=/dev/$dev bs=4096 count=1 of=/dev/null
}
eject_cleanup()
{
	cleanup
}

atf_test_case load cleanup
load_head()
{
	atf_set "descr" "START STOP UNIT can load a CDROM device"
	atf_set "require.user" "root"
	atf_set "require.progs" "sg_start sg_readcap ctladm"
}
load_body()
{
	# -t 5 for CD/DVD device type
	create_ramdisk -t 5

	# eject the device
	atf_check sg_start --eject /dev/$dev

	# Verify that it's offline it should now return ENXIO
	atf_check -s exit:1 -o ignore -e match:"Device not configured" dd if=/dev/$dev bs=4096 count=1 of=/dev/null

	# Load it again
	atf_check sg_start --load /dev/$dev

	atf_check -o ignore -e ignore dd if=/dev/$dev bs=4096 count=1 of=/dev/null
	atf_check -o ignore -e not-match:"Device not ready" sg_readcap /dev/$dev
}
load_cleanup()
{
	cleanup
}

atf_test_case start cleanup
start_head()
{
	atf_set "descr" "START STOP UNIT can start a device"
	atf_set "require.user" "root"
	atf_set "require.progs" "sg_start sg_readcap ctladm"
}
start_body()
{
	create_ramdisk

	# stop the device
	atf_check sg_start --stop /dev/$dev

	# And start it again
	atf_check sg_start /dev/$dev

	# Now sg_readcap should succeed.  Too bad I don't know of any other way
	# to check that it's stopped.
	atf_check -o ignore -e not-match:"Device not ready" sg_readcap /dev/$dev
}
start_cleanup()
{
	cleanup
}

atf_test_case stop cleanup
stop_head()
{
	atf_set "descr" "START STOP UNIT can stop a device"
	atf_set "require.user" "root"
	atf_set "require.progs" "sg_start sg_readcap ctladm"
}
stop_body()
{
	create_ramdisk

	# Stop the device
	atf_check sg_start --stop /dev/$dev

	# Now sg_readcap should fail.  Too bad I don't know of any other way to
	# check that it's stopped.
	atf_check -s exit:2 -e match:"Device not ready" sg_readcap /dev/$dev
}
stop_cleanup()
{
	cleanup
}

atf_init_test_cases()
{
	atf_add_test_case eject
	atf_add_test_case load
	atf_add_test_case start
	atf_add_test_case stop
}
