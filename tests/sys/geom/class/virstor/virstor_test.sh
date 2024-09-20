#
# Copyright (c) 2024 Dell Inc. or its subsidiaries.  All Rights Reserved.
#
# SPDX-License-Identifier: BSD-2-Clause
#

. $(atf_get_srcdir)/conf.sh

atf_test_case basic cleanup
basic_head()
{
	atf_set "descr" "geom virstor basic functional test"
	atf_set "require.user" "root"
}
basic_body()
{
	geom_atf_test_setup
	# Choose a virstor device name
	gvirstor_dev_setup name

	# Create an md backing device and initialize it with junk
	psecsize=512
	attach_md md -t swap -S $psecsize -s 5M || atf_fail "attach_md"
	jot -b uninitialized 0 | dd status=none of=/dev/$md 2> /dev/null

	# Create a virstor device
	vsizemb=64
	vsize=$((vsizemb * 1024 * 1024))
	atf_check -o ignore -e ignore \
	    gvirstor label -v -s ${vsizemb}M -m 512 $name /dev/$md
	devwait
	vdev="/dev/$class/$name"

	ssize=$(diskinfo $vdev | awk '{print $2}')
	atf_check_equal $psecsize $ssize

	size=$(diskinfo $vdev | awk '{print $3}')
	atf_check_equal $vsize $size

	# Write the first and last sectors of the virtual address space
	hasha=$(jot -b a 0 | head -c $ssize | sha1)
	hashz=$(jot -b z 0 | head -c $ssize | sha1)
	zsector=$((vsize / ssize - 1))
	jot -b a 0 | dd status=none of=$vdev bs=$ssize count=1 conv=notrunc
	jot -b z 0 | dd status=none of=$vdev bs=$ssize count=1 conv=notrunc \
	    seek=$zsector

	# Read back and compare
	hashx=$(dd status=none if=$vdev bs=$ssize count=1 | sha1)
	atf_check_equal $hasha $hashx
	hashx=$(dd status=none if=$vdev bs=$ssize count=1 skip=$zsector | sha1)
	atf_check_equal $hashz $hashx

	# Destroy, then retaste and reload
	atf_check -o ignore gvirstor destroy $name
	true > /dev/$md
	devwait

	# Read back and compare
	hashx=$(dd status=none if=$vdev bs=$ssize count=1 | sha1)
	atf_check_equal $hasha $hashx
	hashx=$(dd status=none if=$vdev bs=$ssize count=1 skip=$zsector | sha1)
	atf_check_equal $hashz $hashx
}
basic_cleanup()
{
	gvirstor_test_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case basic
}
