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

# TODO
# * multiple initiators may block removal

# Not Tested
# * persistent removal (not implemented in CTL)

atf_test_case allow cleanup
allow_head()
{
	atf_set "descr" "SCSI PREVENT ALLOW MEDIUM REMOVAL will prevent a CD from being ejected"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_prevent sg_start
}
allow_body()
{
	# -t 5 for CD/DVD device type
	create_ramdisk -t 5

	atf_check sg_prevent --prevent 1 /dev/$dev

	# Now sg_start --eject should fail
	atf_check -s exit:5 -e match:"Illegal request" sg_start --eject /dev/$dev

	atf_check sg_prevent --allow /dev/$dev

	# Now sg_start --eject should work again
	atf_check -s exit:0 sg_start --eject /dev/$dev
}
allow_cleanup()
{
	cleanup
}

atf_test_case allow_idempotent cleanup
allow_idempotent_head()
{
	atf_set "descr" "SCSI PREVENT ALLOW MEDIUM REMOVAL is idempotent when run from the same initiator"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_prevent sg_start
}
allow_idempotent_body()
{
	# -t 5 for CD/DVD device type
	create_ramdisk -t 5

	atf_check sg_prevent --allow /dev/$dev
	atf_check sg_prevent --allow /dev/$dev
	atf_check sg_prevent --prevent 1 /dev/$dev

	# Even though we ran --allow twice, a single --prevent command should
	# suffice to prevent ejecting.  Multiple ALLOW/PREVENT commands from
	# the same initiator don't have any additional effect.
	atf_check -s exit:5 -e match:"Illegal request" sg_start --eject /dev/$dev
}
allow_idempotent_cleanup()
{
	cleanup
}

atf_test_case nonremovable cleanup
nonremovable_head()
{
	atf_set "descr" "SCSI PREVENT ALLOW MEDIUM REMOVAL may not be used on non-removable media"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_prevent
}
nonremovable_body()
{
	# Create a HDD, not a CD, device
	create_ramdisk -t 0

	atf_check -s exit:9 -e match:"Invalid opcode" sg_prevent /dev/$dev
}
nonremovable_cleanup()
{
	cleanup
}

atf_test_case prevent cleanup
prevent_head()
{
	atf_set "descr" "SCSI PREVENT ALLOW MEDIUM REMOVAL will prevent a CD from being ejected"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_prevent sg_start
}
prevent_body()
{
	# -t 5 for CD/DVD device type
	create_ramdisk -t 5

	atf_check sg_prevent --prevent 1 /dev/$dev

	# Now sg_start --eject should fail
	atf_check -s exit:5 -e match:"Illegal request" sg_start --eject /dev/$dev
}
prevent_cleanup()
{
	cleanup
}

atf_test_case prevent_idempotent cleanup
prevent_idempotent_head()
{
	atf_set "descr" "SCSI PREVENT ALLOW MEDIUM REMOVAL is idempotent when run from the same initiator"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_prevent sg_start
}
prevent_idempotent_body()
{
	# -t 5 for CD/DVD device type
	create_ramdisk -t 5

	atf_check sg_prevent --prevent 1 /dev/$dev
	atf_check sg_prevent --prevent 1 /dev/$dev
	atf_check sg_prevent --allow /dev/$dev

	# Even though we ran prevent idempotent and allow only once, eject
	# should be allowed.  Multiple PREVENT commands from the same initiator
	# don't have any additional effect.
	atf_check sg_start --eject /dev/$dev
}
prevent_idempotent_cleanup()
{
	cleanup
}

atf_init_test_cases()
{
	atf_add_test_case allow
	atf_add_test_case allow_idempotent
	atf_add_test_case nonremovable
	atf_add_test_case prevent
	atf_add_test_case prevent_idempotent
}
