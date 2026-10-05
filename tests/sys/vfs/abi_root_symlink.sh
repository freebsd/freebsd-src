# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
#
# Regression tests for absolute symlink resolution under an ABI root
# (compat.linux.emul_path).  namei(9) resolves paths for Linux ABI
# processes by trying the ABI root first and falling back to the
# native root.  Absolute symlink targets receive the same treatment:
# a target is first resolved under the ABI root (so self-contained
# Linux userlands keep working, PR 289739); when the target itself
# does not resolve there, the walk restarts once from the native
# root using the expansion (so symlinks pointing at host paths, and
# linprocfs' /proc/<pid>/exe, keep working, PR 297426).  An ENOENT
# past a target that did resolve under the ABI root falls back to
# the original path like any other lookup.
#
# These tests require the Linux ABI (linux_enable=YES) and a Linux
# userland (e.g. emulators/linux_base-rl9) providing ${emul_path}/bin/sh.
# Without that, they skip, so CI jobs that lack a Linux base do not
# fail.  The two native-only cases still fail on CURRENT namei(9) and
# use atf_expect_fail until the native-root retry lands (PR 297426).
# Each case comments the fixture so it can be rebuilt by hand.
# $wd is any empty directory (the ATF work directory when kyua
# runs).  $emul is compat.linux.emul_path, usually /compat/linux.
# $emul$wd means the same path names created under that ABI root.
#

require_linux()
{
	emul=$(sysctl -n compat.linux.emul_path 2>/dev/null)
	[ "$emul" ] || atf_skip "Linux ABI not present (compat.linux.emul_path)"
	kldstat -q -n linux64 || kldstat -q -n linux ||
		atf_skip "linux(4) not loaded"
	lsh="$emul/bin/sh"
	[ -x "$lsh" ] || atf_skip "Linux userland not installed ($lsh)"
	# Mirror of the ATF work directory under the ABI root.
	wd=$(pwd)
	abiwd="$emul$wd"
	atf_check mkdir -p "$abiwd"
}

cleanup_linux()
{
	emul=$(sysctl -n compat.linux.emul_path 2>/dev/null)
	[ "$emul" ] || return 0
	rm -rf "$emul$(pwd)" 2>/dev/null
	# Remove now-empty parents mirrored under the ABI root.
	rmdir -p "$emul$(dirname "$(pwd)")" 2>/dev/null
	return 0
}

atf_test_case symlink_target_in_abi_root cleanup
symlink_target_in_abi_root_head()
{
	atf_set "descr" "Absolute symlink target under the ABI root wins" \
	    "over an identically named native file (PR 289739)"
	atf_set "require.user" "root"
}
symlink_target_in_abi_root_body()
{
	#
	# Recreate by hand.  $wd is any empty directory.  $emul is
	# compat.linux.emul_path (usually /compat/linux).
	#
	# Native:
	#     $wd/f-both
	#         Regular file containing the word "native".
	#
	# ABI (the same path names created under $emul):
	#     $emul$wd/f-both
	#         Regular file containing the word "abi".
	#     $emul$wd/link-both
	#         Absolute symlink whose target is $wd/f-both.
	#
	# A Linux process opens $wd/link-both.  namei tries that path
	# under the ABI root first ($emul$wd/link-both), follows the
	# symlink to $wd/f-both, and stays on the ABI pass, so the
	# file that is read is $emul$wd/f-both.
	#
	# Expected result: the read returns "abi".
	#
	require_linux
	printf native > "$wd/f-both"
	printf abi > "$abiwd/f-both"
	atf_check ln -s "$wd/f-both" "$abiwd/link-both"
	atf_check -o inline:"abi" "$lsh" -c "cat $wd/link-both"
}
symlink_target_in_abi_root_cleanup()
{
	cleanup_linux
}

atf_test_case symlink_target_native_only cleanup
symlink_target_native_only_head()
{
	atf_set "descr" "Absolute symlink under the ABI root pointing at" \
	    "a native-only file falls back to the native root (PR 297426)"
	atf_set "require.user" "root"
}
symlink_target_native_only_body()
{
	#
	# Recreate by hand.  $wd is any empty directory.  $emul is
	# compat.linux.emul_path (usually /compat/linux).
	#
	# Native:
	#     $wd/f-native
	#         Regular file containing the word "native".
	#
	# ABI (the same path names created under $emul):
	#     $emul$wd/link-native
	#         Absolute symlink whose target is $wd/f-native.
	#     There is no $emul$wd/f-native.
	#
	# A Linux process opens $wd/link-native.  namei tries that
	# path under the ABI root first ($emul$wd/link-native) and
	# follows the symlink to $wd/f-native.  That target does not
	# exist under the ABI root, so the walk should restart once
	# from the native root using the expansion and read
	# $wd/f-native.
	#
	# Expected result: the read returns "native".
	# On CURRENT namei this is ENOENT (atf_expect_fail; PR 297426).
	#
	require_linux
	printf native > "$wd/f-native"
	atf_check ln -s "$wd/f-native" "$abiwd/link-native"
	atf_expect_fail \
	    "PR 297426: abs symlink to native-only target fails under ABI root"
	atf_check -o inline:"native" "$lsh" -c "cat $wd/link-native"
}
symlink_target_native_only_cleanup()
{
	cleanup_linux
}

atf_test_case symlink_target_native_only_midpath cleanup
symlink_target_native_only_midpath_head()
{
	atf_set "descr" "Absolute directory symlink under the ABI root" \
	    "pointing at a native-only directory falls back to the native" \
	    "root for lookups through it (PR 297426), as with" \
	    "linux-rl9-fontconfig's /etc/fonts -> /usr/local/etc/fonts"
	atf_set "require.user" "root"
}
symlink_target_native_only_midpath_body()
{
	#
	# Recreate by hand.  $wd is any empty directory.  $emul is
	# compat.linux.emul_path (usually /compat/linux).
	#
	# Native:
	#     $wd/ndir/
	#         Directory.
	#     $wd/ndir/f
	#         Regular file containing the word "native".
	#
	# ABI (the same path names created under $emul):
	#     $emul$wd/dirlink
	#         Absolute symlink whose target is $wd/ndir.
	#     There is no $emul$wd/ndir.
	#
	# A Linux process opens $wd/dirlink/f.  namei tries that path
	# under the ABI root first ($emul$wd/dirlink/f), follows
	# dirlink to $wd/ndir, and then wants $wd/ndir/f.  The
	# directory does not exist under the ABI root, so the walk
	# should restart once from the native root using the
	# expansion and read $wd/ndir/f.  This is the
	# linux-rl9-fontconfig case (/etc/fonts ->
	# /usr/local/etc/fonts).
	#
	# Expected result: the read returns "native".
	# On CURRENT namei this is ENOENT (atf_expect_fail; PR 297426).
	#
	require_linux
	atf_check mkdir "$wd/ndir"
	printf native > "$wd/ndir/f"
	atf_check ln -s "$wd/ndir" "$abiwd/dirlink"
	atf_expect_fail \
	    "PR 297426: abs dir symlink to native-only target fails under ABI root"
	atf_check -o inline:"native" "$lsh" -c "cat $wd/dirlink/f"
}
symlink_target_native_only_midpath_cleanup()
{
	cleanup_linux
}

atf_test_case symlink_target_resolved_suffix cleanup
symlink_target_resolved_suffix_head()
{
	atf_set "descr" "ENOENT past an absolute symlink target that" \
	    "resolved under the ABI root is not retried from the native" \
	    "root, even when the missing suffix exists natively"
	atf_set "require.user" "root"
}
symlink_target_resolved_suffix_body()
{
	#
	# Recreate by hand.  $wd is any empty directory.  $emul is
	# compat.linux.emul_path (usually /compat/linux).
	#
	# Native:
	#     $wd/tdir/
	#         Directory.
	#     $wd/tdir/f
	#         Regular file containing the word "native".
	#
	# ABI (the same path names created under $emul):
	#     $emul$wd/tdir/
	#         Empty directory (no file named f).
	#     $emul$wd/reslink
	#         Absolute symlink whose target is $wd/tdir.
	#
	# A Linux process opens $wd/reslink/f.  namei tries that path
	# under the ABI root first ($emul$wd/reslink/f), follows
	# reslink to $wd/tdir, and finds $emul$wd/tdir on the ABI
	# pass.  The leftover component f is missing there.  Because
	# the symlink target already resolved under the ABI root,
	# the native restart must not walk the expansion (that would
	# incorrectly find $wd/tdir/f).  It restarts the original
	# path and still gets ENOENT.
	#
	# Expected result: ENOENT.
	#
	require_linux
	atf_check mkdir "$abiwd/tdir"
	atf_check mkdir "$wd/tdir"
	printf native > "$wd/tdir/f"
	atf_check ln -s "$wd/tdir" "$abiwd/reslink"
	atf_check -s not-exit:0 -e ignore "$lsh" -c "cat $wd/reslink/f"
}
symlink_target_resolved_suffix_cleanup()
{
	cleanup_linux
}

atf_test_case plain_native_fallback cleanup
plain_native_fallback_head()
{
	atf_set "descr" "Plain path (no symlink) absent under the ABI root" \
	    "still falls back to the native root"
	atf_set "require.user" "root"
}
plain_native_fallback_body()
{
	#
	# Recreate by hand.  $wd is any empty directory.  $emul is
	# compat.linux.emul_path (usually /compat/linux).
	#
	# Native:
	#     $wd/f-plain
	#         Regular file containing the word "native".
	#
	# ABI (the same path names created under $emul):
	#     There is no $emul$wd/f-plain, and no symlink.
	#
	# A Linux process opens $wd/f-plain.  namei tries that path
	# under the ABI root first, gets ENOENT, and restarts the
	# original path from the native root.
	#
	# Expected result: the read returns "native".
	#
	require_linux
	printf native > "$wd/f-plain"
	atf_check -o inline:"native" "$lsh" -c "cat $wd/f-plain"
}
plain_native_fallback_cleanup()
{
	cleanup_linux
}

atf_init_test_cases()
{
	atf_add_test_case symlink_target_in_abi_root
	atf_add_test_case symlink_target_native_only
	atf_add_test_case symlink_target_native_only_midpath
	atf_add_test_case symlink_target_resolved_suffix
	atf_add_test_case plain_native_fallback
}
