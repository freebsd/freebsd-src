# SPDX-License-Identifier: BSD-2-Clause

# Create a Rock Ridge ISO that demonstrates its
# extensions, like longer dir and file names etc.
create_rock_ridge_iso() {
	local iso_fs short long iso

	iso_fs="${1}"
	short="${iso_fs}/short"
	long="${short}/longernameabcd"
	mkdir -p "${long}" || return 1

	printf "test file" > "${iso_fs}/regular" || return 1
	printf "nested" > "${short}/nested" || return 1
	ln -s "regular" "${iso_fs}/symlinkfile" || return 1
	ln -s "short/longernameabcd" "${iso_fs}/symlinkdir" || return 1

	iso="${2}"
	makefs -t cd9660 -o rockridge "${iso}" "${iso_fs}"
}

atf_test_case cd9660_rrip_pr296853 cleanup

cd9660_rrip_pr296853_head() {
	atf_set "descr" "Regression test for PR 296853"
	atf_set "require.progs" "makefs mdconfig mount umount"
	atf_set "require.user" "root"
}

cd9660_rrip_pr296853_body() {
	local iso_fs iso mnt md

	iso_fs="${PWD}/pr296853"
	iso="${PWD}/malformed_pr296853.iso"
	if ! create_rock_ridge_iso "${iso_fs}" "${iso}"; then
		atf_fail "failed to create Rock Ridge ISO"
	fi
	atf_check "$(atf_get_srcdir)/rrip_corrupt" "${iso}"

	mnt="${PWD}/mnt"
	atf_check mkdir "${mnt}"
	atf_check -o save:mdconfig_pr296853 \
	    mdconfig -a -t vnode -f "${iso}"

	# mdconfig_pr296853 contains the device name, e.g. md0.
	md="$(cat mdconfig_pr296853)"

	# The invalid ISO should fail but not cause a kernel panic.
	atf_check -s not-exit:0 -e match:"Invalid argument" \
	    mount -t cd9660 "/dev/${md}" "${mnt}"
}

cd9660_rrip_pr296853_cleanup() {
	local iso_fs iso md mnt

	iso_fs="${PWD}/pr296853"
	iso="${PWD}/malformed_pr296853.iso"
	mnt="${PWD}/mnt"
	umount -f "${mnt}" 2>/dev/null || true

	if md="$(cat mdconfig_pr296853 2>/dev/null)"; then
		mdconfig -d -u "${md}"
		rm -f mdconfig_pr296853
	fi

	rmdir "${mnt}" 2>/dev/null || true
	rm -rf "${iso_fs}" "${iso}"
}

atf_init_test_cases() {
	atf_add_test_case cd9660_rrip_pr296853
}
