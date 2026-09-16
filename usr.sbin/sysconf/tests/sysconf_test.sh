#
# Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

#
# Resolve the sysconf binary: honor SYSCONF if set, else the sibling
# build product (objdir layout: .../sysconf/tests next to .../sysconf/sysconf),
# else PATH.  Require a regular file: when installed under
# /usr/tests/usr.sbin/sysconf, `../sysconf' is this directory itself and
# `[ -x dir ]' is true for searchable directories.
#
find_sysconf()
{
	if [ -n "$SYSCONF" ] && [ -f "$SYSCONF" ] && [ -x "$SYSCONF" ]; then
		return
	fi
	if [ -f "$( atf_get_srcdir )/../sysconf" ] &&
	   [ -x "$( atf_get_srcdir )/../sysconf" ]
	then
		SYSCONF="$( atf_get_srcdir )/../sysconf"
	elif [ -f "$( pwd )/../sysconf" ] && [ -x "$( pwd )/../sysconf" ]; then
		SYSCONF="$( pwd )/../sysconf"
	else
		SYSCONF=$( command -v sysconf ) ||
			atf_skip "sysconf binary not found"
	fi
}

setup_root()
{
	find_sysconf
	ROOT="$( pwd )/root"
	mkdir -p "$ROOT/etc" "$ROOT/boot/defaults"
}

cleanup_root()
{
	rm -rf "$( pwd )/root"
}

atf_test_case make_append_accumulate cleanup
make_append_accumulate_head()
{
	atf_set "descr" \
		"make += accumulates on read and appends a line on write"
}
make_append_accumulate_body()
{
	setup_root
	printf 'foo=1\n' > "$ROOT/etc/make.conf"
	atf_check -o inline:'foo: 1\n' "$SYSCONF" make -R "$ROOT" foo
	printf 'foo+=2\n' >> "$ROOT/etc/make.conf"
	atf_check -o inline:'foo: 1 2\n' "$SYSCONF" make -R "$ROOT" foo
	atf_check -o inline:'foo: 1 2 -> 1 2 -O2\n' \
	    "$SYSCONF" make -R "$ROOT" 'foo+=-O2'
	atf_check -o inline:'foo: 1 2 -O2\n' "$SYSCONF" make -R "$ROOT" foo
	atf_check -o match:'foo\+=-O2' cat "$ROOT/etc/make.conf"
	# -V on a new += statement reports (added), not `+= -> +=value'
	atf_check -o match:'make.conf:[0-9]*: foo\+=-O3 \(added\)' \
	    "$SYSCONF" make -R "$ROOT" -V 'foo+=-O3'
	atf_check -o inline:'foo: 1 2 -O2 -O3\n' "$SYSCONF" make -R "$ROOT" foo
	# Identical += is a no-op (append_present)
	before=$( stat -f '%m' "$ROOT/etc/make.conf" )
	atf_check "$SYSCONF" make -R "$ROOT" -c 'foo+=-O2'
	after=$( stat -f '%m' "$ROOT/etc/make.conf" )
	atf_check_equal "$before" "$after"
}
make_append_accumulate_cleanup()
{
	cleanup_root
}

atf_test_case src_triad_and_trail cleanup
src_triad_and_trail_head()
{
	atf_set "descr" "src triad order, -V trail, writes prefer src.conf"
}
src_triad_and_trail_body()
{
	setup_root
	printf 'CFLAGS=-O0\n' > "$ROOT/etc/src-env.conf"
	printf 'CFLAGS+=-g\n' > "$ROOT/etc/make.conf"
	printf 'CFLAGS+=-pipe\n' > "$ROOT/etc/src.conf"
	atf_check -o inline:'CFLAGS: -O0 -g -pipe\n' \
	    "$SYSCONF" src -R "$ROOT" CFLAGS
	atf_check -o match:'src-env.conf:1: CFLAGS=-O0' \
	    -o match:'make.conf:1: CFLAGS\+=-g -> -O0 -g' \
	    -o match:'src.conf:1: CFLAGS\+=-pipe -> -O0 -g -pipe' \
	    "$SYSCONF" src -R "$ROOT" -V CFLAGS
	atf_check -o match:'src.conf' \
	    "$SYSCONF" src -R "$ROOT" -v CFLAGS
	# New directive lands in src.conf
	atf_check "$SYSCONF" src -R "$ROOT" -q WITHOUT_FOO=
	atf_check -o match:'WITHOUT_FOO=' cat "$ROOT/etc/src.conf"
	atf_check -o not-match:'WITHOUT_FOO' cat "$ROOT/etc/make.conf"
}
src_triad_and_trail_cleanup()
{
	cleanup_root
}

atf_test_case src_env_rejected
src_env_rejected_head()
{
	atf_set "descr" "src-env keyword is rejected with a hint toward src"
}
src_env_rejected_body()
{
	find_sysconf
	atf_check -s not-exit:0 -e match:'src-env' -e match:'src' \
	    "$SYSCONF" src-env foo
}

atf_test_case env_overrides cleanup
env_overrides_head()
{
	atf_set "descr" "SRC_ENV_CONF / __MAKE_CONF / SRCCONF remap the triad"
}
env_overrides_body()
{
	setup_root
	mkdir -p "$ROOT/alt"
	printf 'X=from-env\n' > "$ROOT/alt/se.conf"
	printf 'X+=from-make\n' > "$ROOT/alt/mk.conf"
	printf 'X+=from-src\n' > "$ROOT/alt/sc.conf"
	# Empty stock files so -R still resolves the triad slots
	:> "$ROOT/etc/src-env.conf"
	:> "$ROOT/etc/make.conf"
	:> "$ROOT/etc/src.conf"
	export SRC_ENV_CONF="$ROOT/alt/se.conf"
	export __MAKE_CONF="$ROOT/alt/mk.conf"
	export SRCCONF="$ROOT/alt/sc.conf"
	atf_check -o inline:'X: from-env from-make from-src\n' \
	    "$SYSCONF" src -R "$ROOT" X
	unset SRC_ENV_CONF __MAKE_CONF SRCCONF
}
env_overrides_cleanup()
{
	cleanup_root
}

atf_test_case attached_f_passthrough cleanup
attached_f_passthrough_head()
{
	atf_set "descr" \
		"Attached -fPATH does not steal the next argv (rc as name)"
}
attached_f_passthrough_body()
{
	setup_root
	mkdir -p "$ROOT/boot"
	printf 'zfs_load="YES"\n' > "$ROOT/boot/loader.conf"
	#
	# If find_target consumed 'loader' as -f's argument, 'rc' would become
	# the target and exec sysrc(8).  Correctly, target is loader and 'rc'
	# is an unknown directive name.
	#
	atf_check -s not-exit:0 -e match:'unknown directive' \
	    "$SYSCONF" -R "$ROOT" -f/boot/loader.conf loader rc
}
attached_f_passthrough_cleanup()
{
	cleanup_root
}

atf_test_case verbatim_backslash cleanup
verbatim_backslash_head()
{
	atf_set "descr" \
		"verbatim_ok re-examines the byte after an even \\\\ run"
}
verbatim_backslash_body()
{
	setup_root
	printf 'foo=1\n' > "$ROOT/etc/make.conf"
	# Even-length \\ before # must not skip #: put must reject (Faraz)
	atf_check -s not-exit:0 -e match:'Invalid argument' \
	    "$SYSCONF" make -R "$ROOT" 'foo=a\\#b'
	# A value without a comment marker still writes.
	atf_check -o match:'foo:' \
	    "$SYSCONF" make -R "$ROOT" 'foo=a\\b'
	atf_check -o match:'foo=a\\\\b' cat "$ROOT/etc/make.conf"
}
verbatim_backslash_cleanup()
{
	cleanup_root
}

atf_test_case make_strike_last_assign cleanup
make_strike_last_assign_head()
{
	atf_set "descr" \
		"make -= strikes the last assignment containing the word"
}
make_strike_last_assign_body()
{
	setup_root
	printf 'bar+=1\nbar+=1 2\nbar+=1 2 3\n' > "$ROOT/etc/make.conf"
	atf_check -o match:'->' "$SYSCONF" make -R "$ROOT" 'bar-=3'
	atf_check -o inline:'bar+=1\nbar+=1 2\nbar+=1 2\n' \
	    cat "$ROOT/etc/make.conf"
	atf_check -o match:'->' "$SYSCONF" make -R "$ROOT" 'bar-=2'
	atf_check -o inline:'bar+=1\nbar+=1 2\nbar+=1\n' \
	    cat "$ROOT/etc/make.conf"
	atf_check -o match:'->' "$SYSCONF" make -R "$ROOT" 'bar-=1'
	atf_check -o inline:'bar+=1\nbar+=1 2\n' cat "$ROOT/etc/make.conf"
	atf_check -o match:'->' "$SYSCONF" make -R "$ROOT" 'bar-=1'
	atf_check -o inline:'bar+=1\nbar+=2\n' cat "$ROOT/etc/make.conf"
	atf_check -o match:'->' "$SYSCONF" make -R "$ROOT" 'bar-=2'
	atf_check -o inline:'bar+=1\n' cat "$ROOT/etc/make.conf"
	atf_check -o inline:'bar: 1 (unchanged)\n' \
	    "$SYSCONF" make -R "$ROOT" 'bar-=9'
	# Emptying the last word deletes the statement: (removed), like (added)
	printf 'z+=only\n' >>"$ROOT/etc/make.conf"
	atf_check -o match:'make.conf:[0-9]*: z\+=only \(removed\)' \
	    "$SYSCONF" make -R "$ROOT" -V 'z-=only'
	atf_check -o not-match:'^z\+=' cat "$ROOT/etc/make.conf"
}
make_strike_last_assign_cleanup()
{
	cleanup_root
}

atf_test_case equal_value_nocheck_mtime cleanup
equal_value_nocheck_mtime_head()
{
	atf_set "descr" "Equal-value write and -c leave mtime alone"
}
equal_value_nocheck_mtime_body()
{
	setup_root
	printf 'bar=1\n' > "$ROOT/etc/make.conf"
	sleep 1
	before=$( stat -f '%m' "$ROOT/etc/make.conf" )
	# Equal-value write: informational echo, file untouched
	atf_check -o inline:'bar: 1 (unchanged)\n' \
	    "$SYSCONF" make -R "$ROOT" bar=1
	# -e uses `=' for write echoes the same as for reads
	atf_check -o inline:'bar=1 (unchanged)\n' \
	    "$SYSCONF" make -R "$ROOT" -e bar=1
	after=$( stat -f '%m' "$ROOT/etc/make.conf" )
	atf_check_equal "$before" "$after"
	# -q suppresses the echo; -c is silent and does not write
	atf_check -o empty "$SYSCONF" make -R "$ROOT" -q bar=1
	atf_check -o empty "$SYSCONF" make -R "$ROOT" -c bar=1
	after2=$( stat -f '%m' "$ROOT/etc/make.conf" )
	atf_check_equal "$before" "$after2"
	atf_check -o inline:'bar=1 # -> 2\n' \
	    "$SYSCONF" make -R "$ROOT" -e bar=2
	atf_check -o inline:'bar=2\n' \
	    "$SYSCONF" make -R "$ROOT" -e bar
}
equal_value_nocheck_mtime_cleanup()
{
	cleanup_root
}

atf_test_case sysctl_oid_range cleanup
sysctl_oid_range_head()
{
	atf_set "descr" \
		"sysctl target rejects values that overflow the OID CTLTYPE"
}
sysctl_oid_range_body()
{
	find_sysconf
	case "$( uname -s )" in
	FreeBSD) ;;
	*) atf_skip "requires FreeBSD sysctl OIDFMT" ;;
	esac
	# -f alone (no -R) still validates against the running kernel.
	conf="$( pwd )/sysctl.conf"
	:> "$conf"
	atf_check -s exit:1 -e match:'out of range for integer' \
	    "$SYSCONF" sysctl -f "$conf" \
	    security.bsd.see_other_uids=99999999999
	atf_check -o empty cat "$conf"
	atf_check -s exit:1 -e match:'out of range for unsigned integer' \
	    "$SYSCONF" sysctl -f "$conf" kern.ipc.somaxconn=-1
	atf_check -s exit:1 -e match:'out of range for unsigned integer' \
	    "$SYSCONF" sysctl -f "$conf" kern.ipc.somaxconn=abc
	atf_check -o match:'see_other_uids' \
	    "$SYSCONF" sysctl -f "$conf" security.bsd.see_other_uids=0
	atf_check -o match:'see_other_uids=0' cat "$conf"
}
sysctl_oid_range_cleanup()
{
	rm -f "$( pwd )/sysctl.conf"
}

atf_test_case make_src_knobs cleanup
make_src_knobs_head()
{
	atf_set "descr" \
		"-s WITH_/WITHOUT_ presence, query display, unchanged, -x"
}
make_src_knobs_body()
{
	setup_root
	:> "$ROOT/etc/make.conf"
	:> "$ROOT/etc/sysctl.conf"
	:> "$ROOT/etc/src-env.conf"
	:> "$ROOT/etc/src.conf"

	# -s only on make/src
	atf_check -s exit:1 -e match:'only valid for make and src' \
	    "$SYSCONF" sysctl -R "$ROOT" -s WITHOUT_FOO
	# name must be WITH_/WITHOUT_
	atf_check -s exit:1 -e match:'WITH_ or WITHOUT_' \
	    "$SYSCONF" make -R "$ROOT" -s CFLAGS
	# no value with -s
	atf_check -s exit:1 -e match:'does not take a value' \
	    "$SYSCONF" make -R "$ROOT" -s 'WITHOUT_FOO='

	atf_check -o inline:'WITHOUT_FOO: (not present) -> (present)\n' \
	    "$SYSCONF" make -R "$ROOT" -s WITHOUT_FOO
	atf_check -o match:'^WITHOUT_FOO=$' cat "$ROOT/etc/make.conf"
	atf_check -o inline:'WITHOUT_FOO (present)\n' \
	    "$SYSCONF" make -R "$ROOT" WITHOUT_FOO
	atf_check -o inline:'WITHOUT_FOO=\n' \
	    "$SYSCONF" make -R "$ROOT" -e WITHOUT_FOO
	atf_check -o inline:'\n' \
	    "$SYSCONF" make -R "$ROOT" -n WITHOUT_FOO

	before=$( stat -f '%m' "$ROOT/etc/make.conf" )
	atf_check -o inline:'WITHOUT_FOO: (present) (unchanged)\n' \
	    "$SYSCONF" make -R "$ROOT" -s WITHOUT_FOO
	after=$( stat -f '%m' "$ROOT/etc/make.conf" )
	atf_check_equal "$before" "$after"

	# Non-empty placeholder (=t) still means present (defined())
	atf_check -o inline:'WITHOUT_BAR: (not present) -> (present)\n' \
	    "$SYSCONF" make -R "$ROOT" 'WITHOUT_BAR=t'
	atf_check -o match:'^WITHOUT_BAR=t$' cat "$ROOT/etc/make.conf"
	atf_check -o inline:'WITHOUT_BAR (present)\n' \
	    "$SYSCONF" make -R "$ROOT" WITHOUT_BAR
	atf_check -o inline:'WITHOUT_BAR=t\n' \
	    "$SYSCONF" make -R "$ROOT" -e WITHOUT_BAR
	atf_check -o inline:'t\n' \
	    "$SYSCONF" make -R "$ROOT" -n WITHOUT_BAR
	before=$( stat -f '%m' "$ROOT/etc/make.conf" )
	atf_check -o inline:'WITHOUT_BAR: (present) (unchanged)\n' \
	    "$SYSCONF" make -R "$ROOT" -s WITHOUT_BAR
	after=$( stat -f '%m' "$ROOT/etc/make.conf" )
	atf_check_equal "$before" "$after"
	atf_check -o match:'^WITHOUT_BAR=t$' cat "$ROOT/etc/make.conf"

	# Explicit name= always writes (incl. empty); echo value changes
	atf_check -o inline:'WITHOUT_BAR: t -> \n' \
	    "$SYSCONF" make -R "$ROOT" 'WITHOUT_BAR='
	atf_check -o match:'^WITHOUT_BAR=$' cat "$ROOT/etc/make.conf"
	atf_check -o inline:'WITHOUT_BAR:  -> 123\n' \
	    "$SYSCONF" make -R "$ROOT" 'WITHOUT_BAR=123'
	atf_check -o match:'^WITHOUT_BAR=123$' cat "$ROOT/etc/make.conf"
	atf_check -o inline:'WITHOUT_BAR: 123 -> 456\n' \
	    "$SYSCONF" make -R "$ROOT" 'WITHOUT_BAR=456'
	atf_check -o match:'^WITHOUT_BAR=456$' cat "$ROOT/etc/make.conf"
	# -s still no-op while present with a value
	before=$( stat -f '%m' "$ROOT/etc/make.conf" )
	atf_check -o inline:'WITHOUT_BAR: (present) (unchanged)\n' \
	    "$SYSCONF" make -R "$ROOT" -s WITHOUT_BAR
	after=$( stat -f '%m' "$ROOT/etc/make.conf" )
	atf_check_equal "$before" "$after"
	atf_check -o match:'^WITHOUT_BAR=456$' cat "$ROOT/etc/make.conf"

	# src prefers src.conf
	atf_check -o inline:'WITHOUT_LLDB: (not present) -> (present)\n' \
	    "$SYSCONF" src -R "$ROOT" -s WITHOUT_LLDB
	atf_check -o match:'^WITHOUT_LLDB=$' cat "$ROOT/etc/src.conf"
	# -x is quiet unless -v/-V (same as non-knob removals)
	atf_check -o empty \
	    "$SYSCONF" src -R "$ROOT" -x WITHOUT_LLDB
	atf_check -o empty cat "$ROOT/etc/src.conf"
	atf_check -o inline:'WITHOUT_LLDB: (not present) -> (present)\n' \
	    "$SYSCONF" src -R "$ROOT" -s WITHOUT_LLDB
	atf_check -o match:'WITHOUT_LLDB \(removed\)' \
	    "$SYSCONF" src -R "$ROOT" -vx WITHOUT_LLDB
	atf_check -o empty cat "$ROOT/etc/src.conf"

	# WITH_*=no (exact lowercase) warns like bsd.mkopt.mk; WITHOUT_*=no
	# and WITH_*=NO do not
	atf_check -e match:'Use WITHOUT_FOO=1 instead of WITH_FOO=no' \
	    -o match:'WITH_FOO' \
	    "$SYSCONF" make -R "$ROOT" 'WITH_FOO=no'
	atf_check -o match:'^WITH_FOO=no$' cat "$ROOT/etc/make.conf"
	atf_check -e empty -o match:'WITH_BAR' \
	    "$SYSCONF" make -R "$ROOT" 'WITH_BAR=NO'
	atf_check -e empty -o match:'WITHOUT_BAZ' \
	    "$SYSCONF" make -R "$ROOT" 'WITHOUT_BAZ=no'

	# Reads remind with file:line (presence display hides =no)
	atf_check -e match:'make.conf:[0-9]+: Use WITHOUT_FOO=1 instead of WITH_FOO=no' \
	    -o inline:'WITH_FOO (present)\n' \
	    "$SYSCONF" make -R "$ROOT" WITH_FOO
	atf_check -e match:'make.conf:[0-9]+: Use WITHOUT_FOO=1 instead of WITH_FOO=no' \
	    -o match:'WITH_FOO \(present\)' \
	    "$SYSCONF" make -R "$ROOT" -a
	atf_check -e match:'make.conf:[0-9]+: Use WITHOUT_FOO=1 instead of WITH_FOO=no' \
	    -o inline:'WITH_FOO=no\n' \
	    "$SYSCONF" make -R "$ROOT" -e WITH_FOO
	atf_check -e empty -o inline:'WITH_FOO (present)\n' \
	    "$SYSCONF" make -R "$ROOT" -q WITH_FOO

	# WITHOUT_MODULES is not a presence knob
	atf_check -s exit:1 -e match:'WITHOUT_MODULES takes a module list' \
	    "$SYSCONF" make -R "$ROOT" -s WITHOUT_MODULES
	atf_check -o match:'WITHOUT_MODULES:  -> plip' \
	    "$SYSCONF" make -R "$ROOT" 'WITHOUT_MODULES=plip'
	atf_check -o inline:'WITHOUT_MODULES: plip\n' \
	    "$SYSCONF" make -R "$ROOT" WITHOUT_MODULES
}
make_src_knobs_cleanup()
{
	cleanup_root
}

atf_init_test_cases()
{
	atf_add_test_case make_src_knobs
	atf_add_test_case make_append_accumulate
	atf_add_test_case make_strike_last_assign
	atf_add_test_case src_triad_and_trail
	atf_add_test_case src_env_rejected
	atf_add_test_case env_overrides
	atf_add_test_case attached_f_passthrough
	atf_add_test_case verbatim_backslash
	atf_add_test_case equal_value_nocheck_mtime
	atf_add_test_case sysctl_oid_range
}
