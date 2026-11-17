#
# Copyright (c) 2026 Klara, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#

prepare_cmd() {
	cmd="${PWD}/${1:-pax}"
	atf_check ln -sf "$(command -v pax)" "${cmd}"
}

prepare_tree() {
	local root=${1:-.}
	atf_check mkdir -p "${root}"/dir/subdir
	atf_check -o save:"${root}"/dir/subdir/file echo 'Hello, world!'
	atf_check ln "${root}"/dir/subdir/file "${root}"/dir/subdir/link
	atf_check ln -s subdir/file "${root}"/dir/link
}

check_tree() {
	local root=${1:-.}
	atf_check test -d "${root}"
	atf_check test -d "${root}"/dir
	atf_check test -d "${root}"/dir/subdir
	atf_check test -f "${root}"/dir/subdir/file
	atf_check test ! -L "${root}"/dir/subdir/link
	atf_check test "${root}"/dir/subdir/link -ef "${root}"/dir/subdir/file
	atf_check test -L "${root}"/dir/link
	atf_check test "${root}"/dir/link -ef "${root}"/dir/subdir/file
}

atf_test_case pax_copy
pax_copy_head() {
	atf_set descr "Exercise pax copy mode"
	atf_set require.progs "pax"
}
pax_copy_body() {
	prepare_cmd pax
	prepare_tree
	atf_check \
	    -s exit:1 \
	    -e match:'Cannot access destination directory' \
	    pax -rw dir dst
	atf_check mkdir dst
	atf_check "${cmd}" -rw dir dst
	check_tree dst
}

atf_test_case cpio_copy
cpio_copy_head() {
	atf_set descr "Exercise cpio pass-through mode"
	atf_set require.progs "pax"
}
cpio_copy_body() {
	prepare_cmd cpio
	prepare_tree
	atf_check -o save:list find dir
	atf_check mkdir dst
	atf_check "${cmd}" -p dst < list
	check_tree dst
}

atf_test_case pax_extract
pax_extract_head() {
	atf_set descr "Extract a pax archive and exercise pax personality"
	atf_set require.progs "bsdtar pax"
}
pax_extract_body() {
	prepare_cmd pax
	prepare_tree src
	atf_check bsdtar -cf test --format=pax -C src dir
	atf_check "${cmd}" -rf test
	check_tree
}

atf_test_case cpio_extract
cpio_extract_head() {
	atf_set descr "Extract a cpio archive and exercise cpio personality"
	atf_set require.progs "bsdtar pax"
}
cpio_extract_body() {
	prepare_cmd cpio
	prepare_tree src
	atf_check bsdtar -cf test --format=cpio -C src dir
	atf_check "${cmd}" -i < test
	check_tree
}

atf_test_case tar_extract
tar_extract_head() {
	atf_set descr "Extract a tar archive and exercise tar personality"
	atf_set require.progs "bsdtar pax"
}
tar_extract_body() {
	prepare_cmd tar
	prepare_tree src
	atf_check bsdtar -cf test --format=ustar -C src dir
	atf_check "${cmd}" -xf test
	check_tree
}

atf_init_test_cases()
{
	atf_add_test_case pax_copy
	atf_add_test_case cpio_copy
	atf_add_test_case pax_extract
	atf_add_test_case cpio_extract
	atf_add_test_case tar_extract
}
