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

# Not Tested
# * Allocation length, because sg3_utils 1.48 does not provide a way to set it.
# * RCTD bit, because CTL does not support it.

. $(atf_get_srcdir)/ctl.subr

require_sg_opcodes_version()
{
	WANT=$1
	HAVE=$(sg_opcodes -V 2>&1 | cut -w -f 3)
	if [ `echo "$HAVE >= $WANT" | bc -l` = 0 ]; then
		atf_skip "This test requires sg_opcodes $WANT or greater"
	fi
}

# Query all supported opcodes.
# NB: the fixture here may need to change frequently, any time CTL gains
# support for new opcodes or service actions.
atf_test_case all_opcodes cleanup
all_opcodes_head()
{
	atf_set "descr" "REPORT SUPPORTED OPCODES can report all supported opcodes"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_opcodes ctladm
}
all_opcodes_body()
{
	create_ramdisk

	atf_check -o file:$(atf_get_srcdir)/all-supported-opcodes.txt sg_opcodes -p disk -nH /dev/$dev
}
all_opcodes_cleanup()
{
	cleanup
}

# Query support for a single opcode.  The REPORTING OPTIONS field will be 1 and
# REQUESTED SERVICE ACTION will be zero.
atf_test_case basic cleanup
basic_head()
{
	atf_set "descr" "REPORT SUPPORTED OPCODES can report a single supported opcode"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_opcodes ctladm
}
basic_body()
{
	create_ramdisk

	atf_check -o inline:" 00     00 03 00 0a 28 1a ff ff  ff ff 00 ff ff 07\n" sg_opcodes -o 0x28 -p disk -nH /dev/$dev
}
basic_cleanup()
{
	cleanup
}

atf_test_case invalid_rep_opts cleanup
invalid_rep_opts_head()
{
	atf_set "descr" "REPORT SUPPORTED OPCODES will fail gracefully if the REPORTING OPTIONS field is set to an invalid value"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_opcodes ctladm
}
invalid_rep_opts_body()
{
	require_sg_opcodes_version 1.03
	create_ramdisk

	atf_check -o ignore -e ignore -s exit:5 sg_opcodes -o 0x28 -p disk -n --rep-opts=4 /dev/$dev
}
invalid_rep_opts_cleanup()
{
	cleanup
}

# Try to query support for an opcode that needs a service action, but without
# specifying a service action.
atf_test_case missing_service_action cleanup
missing_service_action_head()
{
	atf_set "descr" "REPORT SUPPORTED OPCODES fails gracefully if the service action is omitted"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_opcodes ctladm
}
missing_service_action_body()
{
	create_ramdisk

	atf_check -e ignore -s exit:5 sg_opcodes -o 0x3c -p disk -n /dev/$dev
}
missing_service_action_cleanup()
{
	cleanup
}

# Regression test for CVE-2024-42416
atf_test_case out_of_bounds_service_action cleanup
out_of_bounds_service_action_head()
{
	atf_set "descr" "REPORT SUPPORTED OPCODES fails gracefully if the requested service action is out of bounds"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_opcodes ctladm
}
out_of_bounds_service_action_body()
{
	require_sg_opcodes_version 1.03
	create_ramdisk

	# opcode 0 (Test Unit Ready) does not take service actions
	# opcode 0x3c (Read Buffer(10)) does take service actions
	for opcode in 0 0x3c; do
		for ro in 2 3; do
			for sa in 32 100 255 256 10000 65535; do
				atf_check -s exit:5 -o ignore -e ignore sg_opcodes --rep-opts=$ro -o $opcode -s $sa -p disk -nH /dev/$dev
			done
		done
	done
}
out_of_bounds_service_action_cleanup()
{
	cleanup
}

# Query support for an opcode that needs a service action
atf_test_case service_action cleanup
service_action_head()
{
	atf_set "descr" "REPORT SUPPORTED OPCODES can query an opcode that needs a service action"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_opcodes ctladm
}
service_action_body()
{
	create_ramdisk

	atf_check -o inline:" 00     00 03 00 0a 3c 02 00 ff  ff ff ff ff ff 07\n" sg_opcodes -o 0x3c -s 2 -p disk -nH /dev/$dev
}
service_action_cleanup()
{
	cleanup
}

# Try to query support for an opcode that does not need a service action, but
# provide one anyway.
atf_test_case unexpected_service_action cleanup
unexpected_service_action_head()
{
	atf_set "descr" "REPORT SUPPORTED OPCODES fails gracefully if an extraneous service action is provided"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_opcodes ctladm
}
unexpected_service_action_body()
{
	create_ramdisk

	atf_check -e ignore -s exit:5 sg_opcodes -o 0x28 -s 1 -p disk -n /dev/$dev
}
unexpected_service_action_cleanup()
{
	cleanup
}

# Try to query support for an opcode that does not need a service action, but
# provide one anyway.  Set REPORTING OPTIONS to 3.  This requests that the
# command be reported as unsupported, but REQUEST SUPPORTED OPCODES will return
# successfully.
atf_test_case unexpected_service_action_ro3 cleanup
unexpected_service_action_ro3_head()
{
	atf_set "descr" "REPORT SUPPORTED OPCODES fails gracefully if an extraneous service action is provided, using REPORTING OPTIONS 3"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_opcodes ctladm
}
unexpected_service_action_ro3_body()
{
	require_sg_opcodes_version 1.03
	create_ramdisk

	atf_check -e ignore -o inline:" 00     00 01 00 00\n" sg_opcodes --rep-opts=3 -o 0xb7 -s 1 -p disk -nH /dev/$dev
}
unexpected_service_action_ro3_cleanup()
{
	cleanup
}

atf_test_case unsupported_opcode cleanup
unsupported_opcode_head()
{
	atf_set "descr" "REPORT SUPPORTED OPCODES can report a single unsupported opcode"
	atf_set "require.user" "root"
	atf_set "require.progs" sg_opcodes ctladm
}
unsupported_opcode_body()
{
	create_ramdisk

	atf_check -o inline:" 00     00 01 00 00\n" sg_opcodes -o 1 -p disk -nH /dev/$dev
}
unsupported_opcode_cleanup()
{
	cleanup
}


atf_init_test_cases()
{
	atf_add_test_case all_opcodes
	atf_add_test_case basic
	atf_add_test_case invalid_rep_opts
	atf_add_test_case missing_service_action
	atf_add_test_case out_of_bounds_service_action
	atf_add_test_case service_action
	atf_add_test_case unsupported_opcode
	atf_add_test_case unexpected_service_action
	atf_add_test_case unexpected_service_action_ro3
}
