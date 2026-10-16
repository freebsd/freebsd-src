#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause

atf_test_case cd9660_rrip_pr296853 cleanup

cd9660_rrip_pr296853_head() {
	atf_set "descr" "Regression test for PR 296853"
	atf_set "require.user" "root"
}

cd9660_rrip_pr296853_body() {
	local iso mnt md

	iso="$(atf_get_srcdir)/malformed_pr296853.iso"
	mnt="${PWD}/mnt"

	atf_check mkdir "${mnt}"
	atf_check -o save:mdconfig_pr296853 \
	    mdconfig -a -t vnode -f "${iso}"

	# mdconfig_pr296853 contains the device name, e.g. md0.
	md="$(cat mdconfig_pr296853)"

	# The invalid ISO should fail but not cause a kernel panic.
	atf_check -s not-exit:0 \
	    mount -t cd9660 "/dev/${md}" "${mnt}"
}

cd9660_rrip_pr296853_cleanup() {
	local md mnt

	mnt="${PWD}/mnt"
	umount -f "${mnt}" 2>/dev/null || true

	if [ -f mdconfig_pr296853 ]; then
		md="$(cat mdconfig_pr296853)"
		mdconfig -d -u "${md}" 2>/dev/null || true
	fi

	rmdir "${mnt}" 2>/dev/null || true
}

atf_init_test_cases() {
	atf_add_test_case cd9660_rrip_pr296853
}
