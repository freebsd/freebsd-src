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

atf_test_case max_bytes_cap cleanup
max_bytes_cap_head()
{
	atf_set "descr" "BSDCONF_MAX_BYTES bounds parse input"
}
max_bytes_cap_body()
{
	setup_root
	printf 'foo=ok\n' > "$ROOT/etc/make.conf"
	atf_check -o inline:'foo: ok\n' "$SYSCONF" make -R "$ROOT" foo
	# A tight cap rejects an otherwise valid file
	atf_check -s not-exit:0 -e match:'File too large' \
	    env BSDCONF_MAX_BYTES=4 "$SYSCONF" make -R "$ROOT" foo
	got=$( printf 'bar=piped\n' | "$SYSCONF" generic -f - bar ) ||
		atf_fail "pipe parse failed"
	atf_check_equal "bar: piped" "$got"
}
max_bytes_cap_cleanup()
{
	cleanup_root
}

atf_test_case target_prefix cleanup
target_prefix_head()
{
	atf_set "descr" \
		"Unique target prefixes resolve; ambiguous and unknown fail"
}
target_prefix_body()
{
	find_sysconf
	setup_root
	:> "$ROOT/etc/make.conf"
	:> "$ROOT/etc/src-env.conf"
	:> "$ROOT/etc/src.conf"
	printf 'foo=1\n' > "$ROOT/etc/make.conf"

	# Unique prefixes
	atf_check -o inline:'foo: 1\n' "$SYSCONF" m -R "$ROOT" foo
	atf_check -o inline:'foo: 1\n' "$SYSCONF" ma -R "$ROOT" foo
	atf_check -o inline:'foo: 1\n' "$SYSCONF" make -R "$ROOT" foo
	atf_check -o inline:'foo: 1\n' "$SYSCONF" sr -R "$ROOT" foo
	printf 'kern.foo=1\n' > "$ROOT/etc/sysctl.conf"
	atf_check -o inline:'kern.foo: 1\n' "$SYSCONF" sy -R "$ROOT" kern.foo
	# generic requires -f
	atf_check -s not-exit:0 -e match:'no default files' \
	    "$SYSCONF" g -R "$ROOT" foo
	atf_check -o inline:'foo: 1\n' \
	    "$SYSCONF" g -R "$ROOT" -f /etc/make.conf foo

	# Ambiguous: src and sysctl
	atf_check -s not-exit:0 \
	    -e match:'ambiguous target' \
	    -e match:'src' \
	    -e match:'sysctl' \
	    "$SYSCONF" s foo
	# Ambiguous: pkg and poudriere (`po' is the poudriere shorthand)
	atf_check -s not-exit:0 \
	    -e match:'ambiguous target' \
	    -e match:'pkg' \
	    -e match:'poudriere' \
	    "$SYSCONF" p foo
	# Exact still wins
	atf_check -o inline:'foo: 1\n' "$SYSCONF" src -R "$ROOT" foo
	# Infix is not a prefix
	atf_check -s not-exit:0 -e match:'unknown target' \
	    "$SYSCONF" ctl foo
	# src-env is not a prefix of src
	atf_check -s not-exit:0 -e match:'src-env' -e match:'src' \
	    "$SYSCONF" src-env foo
}
target_prefix_cleanup()
{
	cleanup_root
}

atf_test_case poudriere_sysrc cleanup
poudriere_sysrc_head()
{
	atf_set "descr" \
		"poudriere target feeds sysrc -f over the conf collection"
}
poudriere_sysrc_body()
{
	find_sysconf
	command -v sysrc > /dev/null 2>&1 ||
		atf_skip "sysrc not installed"
	ETC="$( pwd )/petc"
	mkdir -p "$ETC/poudriere.d"
	printf 'ZPOOL=old\nFOO=global\n' > "$ETC/poudriere.conf"
	printf 'FOO=jail\nJOBS=2\n' \
	    > "$ETC/poudriere.d/131amd64-poudriere.conf"
	export POUDRIERE_ETC="$ETC"

	# -l / -L are printed here, one path per line (not sysrc -L)
	atf_check -o inline:"$ETC/poudriere.conf\n" \
	    "$SYSCONF" po -l
	atf_check -o inline:"$ETC/poudriere.conf\n$ETC/poudriere.d/poudriere.conf\n" \
	    "$SYSCONF" po -L
	atf_check -o inline:"$ETC/poudriere.conf\n$ETC/poudriere.d/131amd64-poudriere.conf\n" \
	    "$SYSCONF" po --jail 131amd64 -l
	atf_check -o inline:"$ETC/poudriere.conf\n$ETC/poudriere.d/poudriere.conf\n$ETC/poudriere.d/131amd64-poudriere.conf\n" \
	    "$SYSCONF" po --jail 131amd64 -L
	atf_check -o inline:"$ETC/poudriere.conf\n" \
	    "$SYSCONF" po --jail 99amd64 -l
	atf_check -o match:'99amd64-poudriere.conf' \
	    -o not-match:'rc\.conf' \
	    "$SYSCONF" po --jail 99amd64 -L
	atf_check -o inline:"$ETC/poudriere.conf\n" \
	    "$SYSCONF" po -E -L
	atf_check -o inline:"$ETC/poudriere.conf\n$ETC/poudriere.d/131amd64-poudriere.conf\n" \
	    "$SYSCONF" po --jail 131amd64 -E -L
	atf_check -s not-exit:0 -e match:'take no names' \
	    "$SYSCONF" po -l ZPOOL

	# Shorthand po; `p' is ambiguous with pkg
	atf_check -o match:'ZPOOL: old' \
	    "$SYSCONF" po ZPOOL
	# Write a new name into the first file
	atf_check -o match:'BASEFS' \
	    "$SYSCONF" poudriere BASEFS=/poudriere
	atf_check -o match:'BASEFS=' cat "$ETC/poudriere.conf"
	atf_check -o not-match:'BASEFS' \
	    cat "$ETC/poudriere.d/131amd64-poudriere.conf"

	# Overlay collection: --jail appends the jail overlay last
	atf_check -o match:'FOO: jail' \
	    "$SYSCONF" poudriere --jail 131amd64 FOO
	# Without --jail the overlay is not in the collection
	atf_check -o match:'FOO: global' \
	    "$SYSCONF" poudriere FOO
	# --tree is an alias of --ports
	printf 'FOO=tree\n' > "$ETC/poudriere.d/default-poudriere.conf"
	atf_check -o match:'FOO: tree' \
	    "$SYSCONF" poudriere --tree default FOO
	atf_check -o match:'FOO: tree' \
	    "$SYSCONF" poudriere --ports default FOO
	# Rewrite the overlay's existing var
	atf_check -o match:'JOBS' \
	    "$SYSCONF" poudriere --jail 131amd64 JOBS=8
	atf_check -o match:'JOBS: 8' \
	    "$SYSCONF" poudriere --jail 131amd64 JOBS

	# Attached --jail=
	atf_check -o match:'FOO: jail' \
	    "$SYSCONF" poudriere --jail=131amd64 FOO

	# `--' ends the nested keyword; colliding names are variables
	printf 'src=from-conf\nm=short\n' >> "$ETC/poudriere.conf"
	printf 'src=from-overlay\n' > "$ETC/poudriere.d/src.conf"
	atf_check -o match:'src: from-conf' \
	    "$SYSCONF" po -- src
	atf_check -o match:'m: short' \
	    "$SYSCONF" po -- m
	atf_check -s not-exit:0 -e match:"unknown variable 'make'" \
	    "$SYSCONF" po -- make
	atf_check -o match:'src: from-overlay' \
	    "$SYSCONF" po src src

	# -f replaces discovery
	printf 'BAR=only\n' > "$ETC/other.conf"
	atf_check -o match:'BAR: only' \
	    "$SYSCONF" poudriere -f "$ETC/other.conf" BAR

	# Missing globals
	unset POUDRIERE_ETC
	export POUDRIERE_ETC="$( pwd )/missing-etc"
	mkdir -p "$POUDRIERE_ETC"
	atf_check -s not-exit:0 -e match:'poudriere.conf' \
	    "$SYSCONF" po ZPOOL
}
poudriere_sysrc_cleanup()
{
	rm -rf "$( pwd )/petc" "$( pwd )/missing-etc"
}

atf_test_case poudriere_overlays cleanup
poudriere_overlays_head()
{
	atf_set "descr" \
		"poudriere make/src/src-env overlays with --jail/--tree/--set"
}
poudriere_overlays_body()
{
	find_sysconf
	ETC="$( pwd )/petc"
	D="$ETC/poudriere.d"
	mkdir -p "$D"
	export POUDRIERE_ETC="$ETC"

	printf 'CFLAGS=global\n' > "$D/make.conf"
	printf 'CFLAGS=jail\n' > "$D/131amd64-make.conf"
	printf 'CFLAGS=tree\n' > "$D/default-make.conf"
	printf 'CFLAGS=set\n' > "$D/dev-make.conf"
	printf 'CFLAGS=combo\n' > "$D/131amd64-default-make.conf"

	# Shorthand p m
	atf_check -o inline:'CFLAGS: global\n' \
	    "$SYSCONF" po m CFLAGS
	atf_check -o inline:'CFLAGS: jail\n' \
	    "$SYSCONF" poudriere make --jail 131amd64 CFLAGS
	atf_check -o inline:'CFLAGS: tree\n' \
	    "$SYSCONF" po m --tree default CFLAGS
	atf_check -o inline:'CFLAGS: tree\n' \
	    "$SYSCONF" poudriere make --ports default CFLAGS
	atf_check -o inline:'CFLAGS: set\n' \
	    "$SYSCONF" poudriere make --set dev CFLAGS
	# jail-tree combo is last of those two
	atf_check -o inline:'CFLAGS: combo\n' \
	    "$SYSCONF" poudriere make --jail 131amd64 --tree default CFLAGS

	# Rewrite overlay's existing name; new name goes to global
	atf_check -o match:'CFLAGS' \
	    "$SYSCONF" poudriere make --jail 131amd64 CFLAGS=-O2
	atf_check -o match:'CFLAGS=-O2' cat "$D/131amd64-make.conf"
	atf_check -o match:'CFLAGS=global' cat "$D/make.conf"
	atf_check -o match:'CXXFLAGS' \
	    "$SYSCONF" poudriere make --jail 131amd64 CXXFLAGS=-pipe
	atf_check -o match:'CXXFLAGS=-pipe' cat "$D/make.conf"
	atf_check -o not-match:'CXXFLAGS' cat "$D/131amd64-make.conf"

	# Create missing global
	rm -f "$D/make.conf"
	atf_check -o match:'LDFLAGS' \
	    "$SYSCONF" po m LDFLAGS=-L/usr/local/lib
	atf_check -o match:'LDFLAGS=-L/usr/local/lib' cat "$D/make.conf"

	# src vs src-env; --tree ignored (no tree-src.conf in poudriere(8))
	printf 'WITHOUT_FOO=\n' > "$D/src.conf"
	printf 'WITHOUT_TREE=\n' > "$D/default-src.conf"
	printf 'SRC_ENV_VAR=1\n' > "$D/src-env.conf"
	printf 'SRC_ENV_VAR=tree\n' > "$D/default-src-env.conf"
	atf_check -o inline:'WITHOUT_FOO (present)\n' \
	    "$SYSCONF" poudriere src WITHOUT_FOO
	atf_check -s not-exit:0 -e match:'unknown directive' \
	    "$SYSCONF" poudriere src --tree default WITHOUT_TREE
	atf_check -o inline:'SRC_ENV_VAR: 1\n' \
	    "$SYSCONF" poudriere src-env SRC_ENV_VAR
	atf_check -o inline:'SRC_ENV_VAR: 1\n' \
	    "$SYSCONF" poudriere src-env --tree default SRC_ENV_VAR
	atf_check -o inline:'SRC_ENV_VAR: 1\n' \
	    "$SYSCONF" po src-e SRC_ENV_VAR

	# Ambiguous nested prefix
	atf_check -s not-exit:0 -e match:'ambiguous' -e match:'src' \
	    -e match:'src-env' \
	    "$SYSCONF" po s FOO

	# -l lists the collection
	atf_check -o match:'make.conf' -o match:'131amd64-make.conf' \
	    "$SYSCONF" poudriere make --jail 131amd64 -l

	# -L lists every poudriere(8) stem, including files not on disk
	atf_check -o inline:"$D/make.conf\n$D/abc-make.conf\n$D/xyz-make.conf\n$D/160-make.conf\n$D/xyz-abc-make.conf\n$D/160-xyz-make.conf\n$D/160-abc-make.conf\n$D/160-xyz-abc-make.conf\n" \
	    "$SYSCONF" po m --jail 160 --set abc --tree xyz -L
	atf_check -o inline:"$D/make.conf\n$D/xyz-make.conf\n$D/160-make.conf\n$D/160-xyz-make.conf\n" \
	    "$SYSCONF" po m --jail 160 --tree xyz -L
	atf_check -o inline:"$D/make.conf\n" \
	    "$SYSCONF" po m --jail 160 --set abc --tree xyz -l
	atf_check -o inline:"$D/make.conf\n" \
	    "$SYSCONF" po m --jail 160 --set abc --tree xyz -E -L
	atf_check -o inline:"$D/src.conf\n$D/abc-src.conf\n$D/160-src.conf\n" \
	    "$SYSCONF" po src --jail 160 --set abc --tree xyz -L
	atf_check -o inline:"$D/src.conf\n$D/160-src.conf\n" \
	    "$SYSCONF" po src --jail 160 --tree xyz -L
	atf_check -o inline:"$D/src-env.conf\n$D/abc-src-env.conf\n$D/160-src-env.conf\n" \
	    "$SYSCONF" po src-env --jail 160 --set abc --tree xyz -L
	atf_check -o inline:"$D/src-env.conf\n" \
	    "$SYSCONF" po src-env --jail 160 --set abc --tree xyz -E -L
}
poudriere_overlays_cleanup()
{
	rm -rf "$( pwd )/petc"
}

atf_test_case ucl_groups cleanup
ucl_groups_head()
{
	atf_set "descr" \
		"UCL via libucl: dotted paths, typed values, surgical edits"
}
ucl_groups_body()
{
	setup_root
	cat > "$ROOT/app.conf" <<'EOF'
# sample
FreeBSD-ports: {
  url: "pkg+https://pkg.FreeBSD.org/${ABI}/latest",
  enabled: yes
}
FreeBSD-base: { enabled: no }
ALIAS {
	all-depends = "query %dn-%dv";
}
PLUGINS [
	provides
]
EOF
	cp "$ROOT/app.conf" "$ROOT/app.orig"
	# -f is the caller's path; do not also pass -R (that prefixes it)
	# Booleans read back as the library renders them
	atf_check -o inline:'true\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" -n FreeBSD-ports.enabled
	# No variables are defined: ${ABI} is literal
	atf_check -o inline:'pkg+https://pkg.FreeBSD.org/${ABI}/latest\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" -n FreeBSD-ports.url
	atf_check -o inline:'false\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" -n FreeBSD-base.enabled
	atf_check -o inline:'query %dn-%dv\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" -n ALIAS.all-depends
	# Arrays are leaves, shown as compact JSON
	atf_check -o inline:'["provides"]\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" -n PLUGINS
	# -c compares in the same rendering
	atf_check "$SYSCONF" ucl -f "$ROOT/app.conf" -c FreeBSD-ports.enabled=yes
	atf_check -s exit:1 -e match:'differs' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" -c FreeBSD-ports.enabled=no
	# Rewrite one value in place: only that line changes, and the
	# boolean keeps the file's spelling
	atf_check -o inline:'FreeBSD-ports.enabled: true -> false\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" FreeBSD-ports.enabled=no
	atf_check -o inline:'4c4\n<   enabled: yes\n---\n>   enabled: no\n' \
	    -s exit:1 diff "$ROOT/app.orig" "$ROOT/app.conf"
	# Unchanged write leaves the file alone
	cp "$ROOT/app.conf" "$ROOT/app.before"
	atf_check -o inline:'FreeBSD-ports.enabled: false (unchanged)\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" FreeBSD-ports.enabled=no
	atf_check cmp -s "$ROOT/app.conf" "$ROOT/app.before"
	# New key joins a one-line object in its style
	atf_check -o inline:'FreeBSD-base.priority:  -> 10\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" FreeBSD-base.priority=10
	atf_check -o match:'^FreeBSD-base: \{ enabled: no, priority: 10 \}$' \
	    cat "$ROOT/app.conf"
	# New nested path creates the intermediate objects, file style
	atf_check -o inline:'Other.inner.enabled:  -> true\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" Other.inner.enabled=yes
	atf_check -o inline:'true\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" -n Other.inner.enabled
	atf_check -o inline:'Other: {\n  inner: {\n    enabled: yes\n  }\n}\n' \
	    tail -5 "$ROOT/app.conf"
	# A value the grammar would cut short is kept whole, quoted
	atf_check -o inline:'Other.note:  -> a#b\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" Other.note=a#b
	atf_check -o inline:'a#b\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" -n Other.note
	atf_check -o match:'note: "a#b"' cat "$ROOT/app.conf"
	# Writing through a scalar is refused
	atf_check -s exit:1 -e match:'not an object' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" Other.note.x=1
	# += and -= on an array add and strike elements, one line each in
	# the array's own style; the file is never handed a word list
	atf_check -o inline:'PLUGINS: ["provides"] -> ["provides","bar"]\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS+=bar
	atf_check -o inline:'PLUGINS [\n\tprovides\n\tbar\n]\n' \
	    sed -n '/^PLUGINS/,/^]/p' "$ROOT/app.conf"
	atf_check -o inline:'PLUGINS: ["provides","bar"] (unchanged)\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS+=bar
	atf_check "$SYSCONF" ucl -f "$ROOT/app.conf" -c PLUGINS+=bar
	# A comment trailing the struck element goes with its line, as a
	# sysrc(8) removal would take it; one on a kept element stays put
	sed -i '' -e 's/^	bar$/&	# gone/' -e 's/^	provides$/&	# kept/' \
	    "$ROOT/app.conf"
	atf_check -o inline:'PLUGINS: ["provides","bar"] -> ["provides"]\n' \
		"$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS-=bar
	atf_check -o inline:'PLUGINS [\n\tprovides\t# kept\n]\n' \
	    sed -n '/^PLUGINS/,/^]/p' "$ROOT/app.conf"
	sed -i '' -e 's/^	provides	# kept$/	provides/' "$ROOT/app.conf"
	# Under an array a path component is an index (from zero) and `#'
	# the count, as uclcmd(1) spells it; name.i shows the request as
	# typed, name.# reads under the array's name; past the end is unknown
	atf_check -o inline:'PLUGINS: 1\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" 'PLUGINS.#'
	atf_check -o inline:'PLUGINS.0: provides\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS.0
	atf_check -s exit:1 -e inline:"sysconf: unknown directive 'PLUGINS.1'\n" \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS.1
	atf_check -s exit:1 -e inline:"sysconf: unknown directive 'PLUGINS.x'\n" \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS.x
	# name.i= changes that element where it stands; one past the end
	# appends; -x name.i strikes it and echoes the array, like -=
	atf_check -o inline:'PLUGINS.0: provides -> quux\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS.0=quux
	atf_check -o inline:'PLUGINS.1:  -> zed\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS.1=zed
	atf_check -o inline:'PLUGINS [\n\tquux\n\tzed\n]\n' \
	    sed -n '/^PLUGINS/,/^]/p' "$ROOT/app.conf"
	atf_check -s exit:1 -e match:'PLUGINS has 2 elements' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS.5=far
	atf_check "$SYSCONF" ucl -f "$ROOT/app.conf" -c PLUGINS.1=zed
	atf_check -s exit:1 -e match:'differs' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" -c PLUGINS.1=bar
	atf_check -o inline:'PLUGINS: ["quux","zed"] -> ["quux"]\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" -x PLUGINS.1
	atf_check -o inline:'PLUGINS.0: quux -> provides\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS.0=provides
	# name.#=value appends (the echo names the index taken); a negative
	# index counts from the end; neither removes or edits the count
	atf_check -o inline:'PLUGINS.1:  -> foo\nPLUGINS.2:  -> bar\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" 'PLUGINS.#=foo' 'PLUGINS.#=bar'
	atf_check -o inline:'PLUGINS.2: bar\nPLUGINS.0: provides\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS.-1 PLUGINS.-3
	atf_check -s exit:1 -e inline:"sysconf: unknown directive 'PLUGINS.-4'\n" \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS.-4
	atf_check -o inline:'PLUGINS.2: bar -> baz\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS.-1=baz
	atf_check -o inline:'PLUGINS: ["provides","foo","baz"] -> ["provides","foo"]\nPLUGINS: ["provides","foo"] -> ["provides"]\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" -x PLUGINS.-1 PLUGINS.-1
	atf_check -s exit:1 -e match:'count cannot be removed' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" -x 'PLUGINS.#'
	atf_check -s exit:1 -e match:'count cannot be edited' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" 'PLUGINS.#+=x'
	# Striking the last word leaves an empty string, not null
	atf_check -o inline:'PLUGINS.0: provides -> \n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS.-1-=provides
	atf_check -o inline:'PLUGINS: [""]\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS
	atf_check -o inline:'PLUGINS.0:  -> provides\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS.-1+=provides
	# Under an object the same components are keys
	printf 'errors { "404" = "Not Found", "#" = 7 }\n' > "$ROOT/k.conf"
	atf_check -o inline:'errors.404: Not Found\nerrors.#: 7\n' \
	    "$SYSCONF" ucl -f "$ROOT/k.conf" errors.404 'errors.#'
	# Keys the library cannot take bare (`*', `[1m') are written
	# quoted; a file that leaves `*' bare (as uclcmd(1) may) is
	# still read, and does not hide its neighbours
	atf_check -o inline:'foo.*:  -> all\nfoo.**:  -> allall\nfoo.[1m:  -> bold\nfoo.[7m:  -> inverse\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" 'foo.*=all' 'foo.**=allall' \
	    'foo.[1m=bold' 'foo.[7m=inverse'
	atf_check -o match:'"\*"' -o match:'"\*\*"' \
	    -o match:'"\[1m"' -o match:'"\[7m"' \
	    cat "$ROOT/app.conf"
	atf_check -o inline:'foo.*: all\nfoo.[1m: bold\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" 'foo.*' 'foo.[1m'
	# uclcmd(1) may emit `*' and `**' bare; the quoted neighbours stay
	sed -i '' -e 's/"\*"/ */' -e 's/"\*\*"/ **/ ' "$ROOT/app.conf"
	atf_check -o inline:'foo.*: all\nfoo.**: allall\nfoo.[1m: bold\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" 'foo.*' 'foo.**' 'foo.[1m'
	atf_check -o inline:'foo.*: all -> all-pattern\nfoo.[1m: bold -> bolden\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" 'foo.*=all-pattern' \
	    'foo.[1m=bolden'
	"$SYSCONF" ucl -f "$ROOT/app.conf" -x 'foo.*' 'foo.**' 'foo.[1m' \
	    'foo.[7m'
	# name.i+= and name.i-= edit the element: words on a string,
	# elements on a nested array; indices chain through nested arrays
	atf_check -o inline:'PLUGINS.0: provides -> provides x\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS.0+=x
	atf_check -o inline:'PLUGINS [\n\t"provides x"\n]\n' \
	    sed -n '/^PLUGINS/,/^]/p' "$ROOT/app.conf"
	atf_check -o inline:'PLUGINS.0: provides x -> provides\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS.0-=x
	printf 'M [\n  [ 1, 2 ],\n  [\n    3,\n    4\n  ]\n]\n' > "$ROOT/m.conf"
	atf_check -o inline:'M: 2\nM.1: 2\nM.1.0: 3\nM.1.1: 4\n' \
	    "$SYSCONF" ucl -f "$ROOT/m.conf" 'M.#' 'M.1.#' M.1.0 M.-1.-1
	atf_check -s exit:1 -e match:'M.1.0: not an array' \
	    "$SYSCONF" ucl -f "$ROOT/m.conf" M.1.0.0
	atf_check -s exit:1 -e inline:"sysconf: unknown directive 'M.2.0'\n" \
	    "$SYSCONF" ucl -f "$ROOT/m.conf" M.2.0
	# Requests in one run see one another's edits; each nested array
	# keeps its own layout
	atf_check -o inline:'M.1.0: 3 -> 30\nM.1: [30,4] -> [30,4,5]\nM.0: [1,2] -> [2]\n' \
	    "$SYSCONF" ucl -f "$ROOT/m.conf" M.1.0=30 M.1+=5 M.0-=1
	atf_check -o inline:'M [\n  [ 2 ],\n  [\n    30,\n    4,\n    5\n  ]\n]\n' \
	    cat "$ROOT/m.conf"
	atf_check -o inline:'M.1: [30,4,5] -> [30,4]\n' \
	    "$SYSCONF" ucl -f "$ROOT/m.conf" -x M.1.2
	atf_check -o inline:'M.1.0: 30 -> 3\nM.0: [2] -> [2,1]\n' \
	    "$SYSCONF" ucl -f "$ROOT/m.conf" M.1.0=3 M.0+=1
	atf_check -o inline:'M [\n  [ 2, 1 ],\n  [\n    3,\n    4\n  ]\n]\n' \
	    cat "$ROOT/m.conf"
	atf_check -o inline:'PLUGINS: ["provides"] -> ["provides","a"]\nPLUGINS: ["provides","a"] -> ["provides","a","b"]\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS+=a PLUGINS+=b
	atf_check -o inline:'PLUGINS: ["provides","a","b"] -> ["provides","b"]\nPLUGINS: ["provides","b"] -> ["provides"]\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" PLUGINS-=a PLUGINS-=b
	# A string keeps the sysrc(8) word edit
	atf_check -o inline:'ALIAS.all-depends: query %dn-%dv -> query %dn-%dv -x\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" ALIAS.all-depends+=-x
	atf_check -o match:'all-depends = "query %dn-%dv -x";' \
	    cat "$ROOT/app.conf"
	atf_check -o inline:'ALIAS.all-depends: query %dn-%dv -x -> query %dn-%dv\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" ALIAS.all-depends-=-x
	# Removals undo the additions exactly; an emptied object goes too,
	# and so does a comment trailing a removed statement
	sed -i '' -e 's/^  note: "a#b",\{0,1\}$/&  # scratch/' "$ROOT/app.conf"
	atf_check -o match:'# scratch' cat "$ROOT/app.conf"
	atf_check "$SYSCONF" ucl -f "$ROOT/app.conf" -x FreeBSD-base.priority \
	    Other.inner.enabled Other.note
	atf_check -o not-match:'scratch' cat "$ROOT/app.conf"
	atf_check -o inline:'FreeBSD-ports.enabled: false -> true\n' \
	    "$SYSCONF" ucl -f "$ROOT/app.conf" FreeBSD-ports.enabled=yes
	atf_check cmp -s "$ROOT/app.orig" "$ROOT/app.conf"
	# No default files
	atf_check -s exit:1 -e match:'no default files' "$SYSCONF" ucl foo
}
ucl_groups_cleanup()
{
	cleanup_root
}

atf_test_case ucl_hostile_input cleanup
ucl_hostile_input_head()
{
	atf_set "descr" \
		"UCL: null characters, stray statements and wild indices"
}
ucl_hostile_input_body()
{
	setup_root
	# A null character is refused on read and write; the file is left alone
	printf '\000' > "$ROOT/nul.conf"
	printf 'a = 1;\n\000b = 2;\n' > "$ROOT/mid.conf"
	cp "$ROOT/mid.conf" "$ROOT/mid.orig"
	atf_check -s exit:1 -e match:'contains a null character' \
	    "$SYSCONF" ucl -f "$ROOT/nul.conf" a
	atf_check -s exit:1 -e match:'contains a null character' \
	    "$SYSCONF" ucl -f "$ROOT/mid.conf" a
	atf_check -s exit:1 -e match:'contains a null character' \
	    "$SYSCONF" ucl -f "$ROOT/mid.conf" a=3
	atf_check cmp -s "$ROOT/mid.orig" "$ROOT/mid.conf"
	# Typed text that could open a macro lands as one quoted string
	printf 'a = 1;\n' > "$ROOT/m.conf"
	atf_check -o ignore "$SYSCONF" ucl -f "$ROOT/m.conf" \
	    "a=2
.include \"$ROOT/evil.conf\""
	atf_check -o not-match:'^\.include' cat "$ROOT/m.conf"
	atf_check -o ignore "$SYSCONF" ucl -f "$ROOT/m.conf" \
	    "a=2; .include \"$ROOT/evil.conf\""
	atf_check -o match:'^a = "2; \.include ' cat "$ROOT/m.conf"
	atf_check -o ignore "$SYSCONF" ucl -f "$ROOT/m.conf" \
	    "a={ b = 1; .include \"$ROOT/evil.conf\" }"
	atf_check -o match:'^a = "\{ b = 1; \.include ' cat "$ROOT/m.conf"
	# A plain number is still written as typed
	atf_check -o ignore "$SYSCONF" ucl -f "$ROOT/m.conf" a=1.5
	atf_check -o inline:'a = 1.5;\n' cat "$ROOT/m.conf"
	# A second statement after a newline stays inside the string
	atf_check -o ignore "$SYSCONF" ucl -f "$ROOT/m.conf" \
	    "a=1
a = 5"
	atf_check -o inline:'1\n' grep -c '^a' "$ROOT/m.conf"
	atf_check -o match:'^a = "1\\na = 5";$' cat "$ROOT/m.conf"
	# A comment typed after a number cannot hide a neighbour
	printf 'a = 1; b = 2;\n' > "$ROOT/m.conf"
	atf_check -o ignore "$SYSCONF" ucl -f "$ROOT/m.conf" 'a=3 # x'
	atf_check -o inline:'3 # x\n2\n' \
	    "$SYSCONF" ucl -f "$ROOT/m.conf" -n a b
	# An index libucl cannot count to, or one that would pass for the
	# count, is a key and so unknown, not an element
	printf 'L [ x, y ]\n' > "$ROOT/l.conf"
	for i in 4294967296 4294967297 -4294967296 -9223372036854775808 \
	    -9223372036854775807 9223372036854775808; do
		atf_check -s exit:1 \
		    -e inline:"sysconf: unknown directive 'L.$i'\n" \
		    "$SYSCONF" ucl -f "$ROOT/l.conf" "L.$i"
	done
	# The largest index libucl can count to is merely past the end
	atf_check -s exit:1 \
	    -e inline:"sysconf: unknown directive 'L.4294967295'\n" \
	    "$SYSCONF" ucl -f "$ROOT/l.conf" L.4294967295
}
ucl_hostile_input_cleanup()
{
	cleanup_root
}

atf_test_case ucl_file_shapes cleanup
ucl_file_shapes_head()
{
	atf_set "descr" \
		"UCL: repeated keys, nested comments, outer braces, bad files"
}
ucl_file_shapes_body()
{
	setup_root
	# A repeated key reads as its first; a removal takes every one,
	# lest a later one surface
	printf 'foo = 1;\nbar = 2;\nfoo = 3;\n' > "$ROOT/dup.conf"
	atf_check -o inline:'1\n' "$SYSCONF" ucl -f "$ROOT/dup.conf" -n foo
	atf_check "$SYSCONF" ucl -f "$ROOT/dup.conf" -x foo
	atf_check -o inline:'bar = 2;\n' cat "$ROOT/dup.conf"
	# Block comments nest: the key inside one is not the key
	printf '/* old /* note */ foo = 1 */\nfoo = 2;\n' > "$ROOT/c.conf"
	atf_check -o inline:'foo: 2 -> 5\n' \
	    "$SYSCONF" ucl -f "$ROOT/c.conf" foo=5
	atf_check -o inline:'/* old /* note */ foo = 1 */\nfoo = 5;\n' \
	    cat "$ROOT/c.conf"
	# A file that is one empty outer object takes the key inside it
	for body in '{}' '{\n}'; do
		printf '%b\n' "$body" > "$ROOT/o.conf"
		atf_check -o inline:'key:  -> 1\n' \
		    "$SYSCONF" ucl -f "$ROOT/o.conf" key=1
		atf_check -o inline:'1\n' \
		    "$SYSCONF" ucl -f "$ROOT/o.conf" -n key
		atf_check -o match:'^\{' -o not-match:'^"?key' \
		    cat "$ROOT/o.conf"
	done
	# A file libucl rejects is fatal, on a read and on a write
	printf 'a = 1;\nb = {\n' > "$ROOT/bad.conf"
	cp "$ROOT/bad.conf" "$ROOT/bad.orig"
	atf_check -s exit:1 -e match:'bad\.conf: ' \
	    "$SYSCONF" ucl -f "$ROOT/bad.conf" a
	atf_check -s exit:1 -e match:'bad\.conf: ' \
	    "$SYSCONF" ucl -f "$ROOT/bad.conf" a=2
	atf_check cmp -s "$ROOT/bad.orig" "$ROOT/bad.conf"
}
ucl_file_shapes_cleanup()
{
	cleanup_root
}

atf_test_case ucl_replace cleanup
ucl_replace_head()
{
	atf_set "descr" \
		"UCL: writes follow links, keep the mode, and refuse a FIFO"
}
ucl_replace_body()
{
	setup_root
	printf 'a = 1;\n' > "$ROOT/real.conf"
	chmod 640 "$ROOT/real.conf"
	ln -s real.conf "$ROOT/link.conf"
	atf_check -o inline:'a: 1 -> 2\n' \
	    "$SYSCONF" ucl -f "$ROOT/link.conf" a=2
	atf_check test -L "$ROOT/link.conf"
	atf_check -o inline:'a = 2;\n' cat "$ROOT/real.conf"
	atf_check -o inline:'640\n' stat -f %Lp "$ROOT/real.conf"
	# Nothing is left behind beside it
	atf_check -o inline:'boot\netc\nlink.conf\nreal.conf\n' ls "$ROOT"
	# A dangling link is refused, not replaced by a regular file
	ln -s missing.conf "$ROOT/dangling.conf"
	atf_check -s exit:1 -o ignore \
	    -e inline:"sysconf: $ROOT/dangling.conf: No such file or directory\n" \
	    "$SYSCONF" ucl -f "$ROOT/dangling.conf" a=1
	atf_check test -L "$ROOT/dangling.conf"
	atf_check test ! -e "$ROOT/missing.conf"
	# Only a regular file is replaced. A writer feeds the reads, which
	# would otherwise block opening the FIFO, and is gone before any
	# check can fail and leave it behind
	mkfifo "$ROOT/fifo.conf"
	while :; do : > "$ROOT/fifo.conf"; done >/dev/null 2>&1 &
	writer=$!
	rv=0
	"$SYSCONF" ucl -f "$ROOT/fifo.conf" a=1 2> "$ROOT/err" || rv=$?
	kill "$writer"
	atf_check_equal 1 "$rv"
	atf_check -o inline:"sysconf: $ROOT/fifo.conf: Invalid argument\n" \
	    cat "$ROOT/err"
	atf_check test -p "$ROOT/fifo.conf"
}
ucl_replace_cleanup()
{
	cleanup_root
}

atf_test_case ucl_replace_owner cleanup
ucl_replace_owner_head()
{
	atf_set "descr" "UCL: a write keeps the file's owner and group"
	atf_set "require.user" "root"
}
ucl_replace_owner_body()
{
	setup_root
	printf 'a = 1;\n' > "$ROOT/o.conf"
	chown nobody:nobody "$ROOT/o.conf"
	atf_check -o inline:'a: 1 -> 2\n' "$SYSCONF" ucl -f "$ROOT/o.conf" a=2
	atf_check -o inline:'nobody:nobody\n' \
	    stat -f %Su:%Sg "$ROOT/o.conf"
}
ucl_replace_owner_cleanup()
{
	cleanup_root
}

atf_test_case pkg_repos_override cleanup
pkg_repos_override_head()
{
	atf_set "descr" \
		"pkg reads /etc/pkg and writes overrides under repos/"
}
pkg_repos_override_body()
{
	setup_root
	mkdir -p "$ROOT/etc/pkg" "$ROOT/usr/local/etc/pkg/repos"
	cat > "$ROOT/etc/pkg/FreeBSD.conf" <<'EOF'
# base definitions; override under /usr/local/etc/pkg/repos
FreeBSD-ports: {
  url: "pkg+https://pkg.FreeBSD.org/${ABI}/latest",
  enabled: yes
}
FreeBSD-base: {
  enabled: no
}
EOF
	cp "$ROOT/etc/pkg/FreeBSD.conf" "$ROOT/base.before"
	printf '# local\nPKG_ENABLE_PLUGINS: true\n' \
	    > "$ROOT/usr/local/etc/pkg.conf"
	atf_check -o inline:'true\n' \
	    "$SYSCONF" pkg -R "$ROOT" -n FreeBSD-ports.enabled
	atf_check -o inline:'true\n' \
	    "$SYSCONF" pkg -R "$ROOT" -n PKG_ENABLE_PLUGINS
	# -F names the base file; -l lists the repos file before it exists
	atf_check -o match:'/etc/pkg/FreeBSD.conf$' \
	    "$SYSCONF" pkg -R "$ROOT" -n -F FreeBSD-ports.enabled
	atf_check -o match:'/usr/local/etc/pkg/repos/FreeBSD.conf$' \
	    "$SYSCONF" pkg -R "$ROOT" -l
	# Base file is not the write target: the repos file is created
	# in the base file's style
	atf_check -o inline:'FreeBSD-ports.enabled: true -> false\n' \
	    "$SYSCONF" pkg -R "$ROOT" FreeBSD-ports.enabled=no
	atf_check cmp -s "$ROOT/etc/pkg/FreeBSD.conf" "$ROOT/base.before"
	atf_check -o inline:'FreeBSD-ports: {\n  enabled: no\n}\n' \
	    cat "$ROOT/usr/local/etc/pkg/repos/FreeBSD.conf"
	atf_check -o inline:'false\n' \
	    "$SYSCONF" pkg -R "$ROOT" -n FreeBSD-ports.enabled
	atf_check -o match:'/usr/local/etc/pkg/repos/FreeBSD.conf$' \
	    "$SYSCONF" pkg -R "$ROOT" -n -F FreeBSD-ports.enabled
	# url still comes from the base file, ${ABI} unexpanded
	atf_check -o inline:'pkg+https://pkg.FreeBSD.org/${ABI}/latest\n' \
	    "$SYSCONF" pkg -R "$ROOT" -n FreeBSD-ports.url
	# Second write updates the local key in place (no second object)
	atf_check -o inline:'FreeBSD-ports.enabled: false -> true\n' \
	    "$SYSCONF" pkg -R "$ROOT" FreeBSD-ports.enabled=yes
	atf_check -o inline:'FreeBSD-ports: {\n  enabled: yes\n}\n' \
	    cat "$ROOT/usr/local/etc/pkg/repos/FreeBSD.conf"
	# A second repository key joins the object, base-file punctuation
	atf_check -o inline:'FreeBSD-ports.priority:  -> 5\n' \
	    "$SYSCONF" pkg -R "$ROOT" FreeBSD-ports.priority=5
	atf_check -o inline:'FreeBSD-ports: {\n  enabled: yes,\n  priority: 5\n}\n' \
	    cat "$ROOT/usr/local/etc/pkg/repos/FreeBSD.conf"
	atf_check "$SYSCONF" pkg -R "$ROOT" -x FreeBSD-ports.priority
	atf_check -o inline:'FreeBSD-ports: {\n  enabled: yes\n}\n' \
	    cat "$ROOT/usr/local/etc/pkg/repos/FreeBSD.conf"
	# Global key lands in pkg.conf; the rest of the file is untouched
	atf_check -o inline:'ASSUME_ALWAYS_YES:  -> true\n' \
	    "$SYSCONF" pkg -R "$ROOT" ASSUME_ALWAYS_YES=yes
	atf_check -o inline:'# local\nPKG_ENABLE_PLUGINS: true\nASSUME_ALWAYS_YES: yes\n' \
	    cat "$ROOT/usr/local/etc/pkg.conf"
	atf_check -o not-match:'ASSUME_ALWAYS_YES' \
	    cat "$ROOT/usr/local/etc/pkg/repos/FreeBSD.conf"
	# A key defined only in the base file cannot be removed
	atf_check -s exit:1 -e match:'unknown directive' \
	    "$SYSCONF" pkg -R "$ROOT" -x FreeBSD-base.enabled
	atf_check cmp -s "$ROOT/etc/pkg/FreeBSD.conf" "$ROOT/base.before"
	# Removing the local override exposes the base value again and
	# takes the emptied object with it
	atf_check "$SYSCONF" pkg -R "$ROOT" -x FreeBSD-ports.enabled
	atf_check -o inline:'true\n' \
	    "$SYSCONF" pkg -R "$ROOT" -n FreeBSD-ports.enabled
	atf_check -o match:'/etc/pkg/FreeBSD.conf$' \
	    "$SYSCONF" pkg -R "$ROOT" -n -F FreeBSD-ports.enabled
	atf_check -o not-match:'FreeBSD-ports' \
	    cat "$ROOT/usr/local/etc/pkg/repos/FreeBSD.conf"
}
pkg_repos_override_cleanup()
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
	atf_add_test_case max_bytes_cap
	atf_add_test_case target_prefix
	atf_add_test_case poudriere_sysrc
	atf_add_test_case poudriere_overlays
	atf_add_test_case ucl_groups
	atf_add_test_case ucl_hostile_input
	atf_add_test_case ucl_file_shapes
	atf_add_test_case ucl_replace
	atf_add_test_case ucl_replace_owner
	atf_add_test_case pkg_repos_override
}
