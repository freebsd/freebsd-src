#
#  Copyright (c) 2013 Spectra Logic Corporation
#  All rights reserved.
# 
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions, and the following disclaimer,
#     without modification.
#  2. Redistributions in binary form must reproduce at minimum a disclaimer
#     substantially similar to the "NO WARRANTY" disclaimer below
#     ("Disclaimer") and any redistribution must be conditioned upon
#     including a substantially similar Disclaimer requirement for further
#     binary redistribution.
# 
#  NO WARRANTY
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
#  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
#  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
#  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGES.
# 
#  Authors: Will Andrews        (Spectra Logic Corporation)
#
# $FreeBSD$

#
# Test Case: GEOM partitions can be created
#
atf_test_case geom_part_create_test cleanup
geom_part_create_test_head()
{
	atf_set "descr" "Test that gpart create works"
	atf_set "has.cleanup" true
	atf_set "require.progs" gpart
	atf_set "require.user" "root"
}

geom_part_create_test_body()
{
	disk=$(pop_disk)
	[ -z "$disk" ] && atf_skip "Could not select a disk!"
	gpart destroy -F $disk 2>/dev/null || true
	atf_check -s exit:0 test -c ${disk}

	run_cmd "gpart create -s gpt $disk"
	on_cmd_failure "GPT creation failed on $disk"
	atf_check -s exit:1 test -c ${disk}p1

	run_cmd "gpart add -a 1m -s 64k -t freebsd-boot $disk"
	on_cmd_failure "Boot partition add failure"
	atf_check -s exit:0 test -c ${disk}p1
	atf_check -s exit:1 test -c ${disk}p2

	run_cmd "gpart add -t efi -s 10g $disk"
	on_cmd_failure "EFI partition add failure"
	atf_check -s exit:0 test -c ${disk}p1
	atf_check -s exit:0 test -c ${disk}p2
	atf_check -s exit:1 test -c ${disk}p3

	run_cmd "gpart add -a 1m -s 4G -t freebsd-swap -l swap0 $disk"
	on_cmd_failure "Swap partition add failure"
	atf_check -s exit:0 test -c ${disk}p2
	atf_check -s exit:0 test -c ${disk}p3
	atf_check -s exit:1 test -c ${disk}p4

	run_cmd "gpart add -a 1m -t freebsd-zfs -l system_pool0 $disk"
	on_cmd_failure "ZFS partition add failure"
	atf_check -s exit:0 test -c ${disk}p3
	atf_check -s exit:0 test -c ${disk}p4
	atf_check -s exit:1 test -c ${disk}p5

	sleep 1 # XXX destroy returns "busy" without this...
	run_cmd "gpart destroy -F $disk"
	on_cmd_failure "Unable to cleanup $disk: $cmdout"
	atf_pass
}

geom_part_create_test_cleanup()
{
	# XXX Can't pass state from body to cleanup.
	#gpart destroy -F $disk >/dev/null 2>&1 || true
	true
}

#
# ATF Test Program Initialization
#
atf_init_test_cases()
{
	atf_add_test_case geom_part_create_test
}

pop_disk()
{
	old_args=$*
	set -- $DISKS
	DISKS=""
	i=1
	for d in $*; do
		if [ $i -eq $# ]; then
			disk=$d
			break
		fi
		i=`expr $i + 1`
		DISKS="$DISKS $d"
	done
	set -- $old_args
	echo $disk
}

run_cmd()
{
	cmd=$1
	echo "Running command: ${cmd}"
	cmdout=`eval $cmd 2>&1`
	cmdret=$?
}

on_cmd_failure()
{
	fail_reason=$1
	[ $cmdret -ne 0 ] && atf_fail $fail_reason
}
