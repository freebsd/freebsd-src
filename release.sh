#! /bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2020 Gavin D. Howard and contributors.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

usage() {
	printf 'usage: %s [run_tests] [generate_tests] [test_with_clang] [test_with_gcc] \n' "$script"
	printf '          [run_sanitizers] [run_valgrind] [run_64_bit] [run_gen_script]\n'
	exit 1
}

header() {

	_header_msg="$1"
	shift

	printf '\n'
	printf '*******************\n'
	printf "$_header_msg"
	printf '\n'
	printf '*******************\n'
	printf '\n'
}

do_make() {
	make -j4 "$@"
}

configure() {

	_configure_CFLAGS="$1"
	shift

	_configure_CC="$1"
	shift

	_configure_configure_flags="$1"
	shift

	_configure_GEN_HOST="$1"
	shift

	_configure_LONG_BIT="$1"
	shift

	if [ "$gen_tests" -eq 0 ]; then
		_configure_configure_flags="-G $_configure_configure_flags"
	fi

	if [ "$_configure_CC" = "clang" ]; then
		_configure_CFLAGS="$clang_flags $_configure_CFLAGS"
	elif [ "$_configure_CC" = "gcc" ]; then
		_configure_CFLAGS="$gcc_flags $_configure_CFLAGS"
	fi

	_configure_header=$(printf 'Running ./configure.sh %s ...' "$_configure_configure_flags")
	_configure_header=$(printf "$_configure_header\n    CC=\"%s\"\n" "$_configure_CC")
	_configure_header=$(printf "$_configure_header\n    CFLAGS=\"%s\"\n" "$_configure_CFLAGS")
	_configure_header=$(printf "$_configure_header\n    LONG_BIT=%s" "$_configure_LONG_BIT")
	_configure_header=$(printf "$_configure_header\n    GEN_HOST=%s" "$_configure_GEN_HOST")

	header "$_configure_header"
	CFLAGS="$_configure_CFLAGS" CC="$_configure_CC" GEN_HOST="$_configure_GEN_HOST" \
		LONG_BIT="$_configure_LONG_BIT" ./configure.sh $_configure_configure_flags > /dev/null
}

build() {

	_build_CFLAGS="$1"
	shift

	_build_CC="$1"
	shift

	_build_configure_flags="$1"
	shift

	_build_GEN_HOST="$1"
	shift

	_build_LONG_BIT="$1"
	shift

	configure "$_build_CFLAGS" "$_build_CC" "$_build_configure_flags" "$_build_GEN_HOST" "$_build_LONG_BIT"

	_build_header=$(printf 'Building...\n    CC=%s' "$_build_CC")
	_build_header=$(printf "$_build_header\n    CFLAGS=\"%s\"" "$_build_CFLAGS")
	_build_header=$(printf "$_build_header\n    LONG_BIT=%s" "$_build_LONG_BIT")
	_build_header=$(printf "$_build_header\n    GEN_HOST=%s" "$_build_GEN_HOST")

	header "$_build_header"

	do_make > /dev/null 2> "$scriptdir/.test.txt"

	if [ -s "$scriptdir/.test.txt" ]; then
		printf '%s generated warning(s):\n' "$_build_CC"
		printf '\n'
		cat "$scriptdir/.test.txt"
		exit 1
	fi
}

runtest() {

	header "Running tests"

	if [ "$#" -gt 0 ]; then
		do_make "$@"
	else
		do_make test
	fi
}

runconfigtests() {

	_runconfigtests_CFLAGS="$1"
	shift

	_runconfigtests_CC="$1"
	shift

	_runconfigtests_configure_flags="$1"
	shift

	_runconfigtests_GEN_HOST="$1"
	shift

	_runconfigtests_LONG_BIT="$1"
	shift

	_runconfigtests_run_tests="$1"
	shift

	if [ "$_runconfigtests_run_tests" -ne 0 ]; then
		_runconfigtests_header=$(printf 'Running tests with configure flags')
	else
		_runconfigtests_header=$(printf 'Building with configure flags')
	fi

	_runconfigtests_header=$(printf "$_runconfigtests_header \"%s\" ...\n" "$_runconfigtests_configure_flags")
	_runconfigtests_header=$(printf "$_runconfigtests_header\n    CC=%s\n" "$_runconfigseries_CC")
	_runconfigtests_header=$(printf "$_runconfigtests_header\n    CFLAGS=\"%s\"" "$_runconfigseries_CFLAGS")
	_runconfigtests_header=$(printf "$_runconfigtests_header\n    LONG_BIT=%s" "$_runconfigtests_LONG_BIT")
	_runconfigtests_header=$(printf "$_runconfigtests_header\n    GEN_HOST=%s" "$_runconfigtests_GEN_HOST")

	header "$_runconfigtests_header"

	build "$_runconfigtests_CFLAGS" "$_runconfigtests_CC" \
		"$_runconfigtests_configure_flags" "$_runconfigtests_GEN_HOST" \
		"$_runconfigtests_LONG_BIT"

	if [ "$_runconfigtests_run_tests" -ne 0 ]; then
		runtest
	fi

	do_make clean

	build "$_runconfigtests_CFLAGS" "$_runconfigtests_CC" \
		"$_runconfigtests_configure_flags -b" "$_runconfigtests_GEN_HOST" \
		"$_runconfigtests_LONG_BIT"

	if [ "$_runconfigtests_run_tests" -ne 0 ]; then
		runtest
	fi

	do_make clean

	build "$_runconfigtests_CFLAGS" "$_runconfigtests_CC" \
		"$_runconfigtests_configure_flags -d" "$_runconfigtests_GEN_HOST" \
		"$_runconfigtests_LONG_BIT"

	if [ "$_runconfigtests_run_tests" -ne 0 ]; then
		runtest
	fi

	do_make clean
}

runconfigseries() {

	_runconfigseries_CFLAGS="$1"
	shift

	_runconfigseries_CC="$1"
	shift

	_runconfigseries_configure_flags="$1"
	shift

	_runconfigseries_run_tests="$1"
	shift

	if [ "$run_64_bit" -ne 0 ]; then

		runconfigtests "$_runconfigseries_CFLAGS" "$_runconfigseries_CC" \
			"$_runconfigseries_configure_flags" 1 64 "$_runconfigseries_run_tests"

		if [ "$run_gen_script" -ne 0 ]; then
			runconfigtests "$_runconfigseries_CFLAGS" "$_runconfigseries_CC" \
				"$_runconfigseries_configure_flags" 0 64 "$_runconfigseries_run_tests"
		fi

		runconfigtests "$_runconfigseries_CFLAGS -DBC_RAND_BUILTIN=0" "$_runconfigseries_CC" \
			"$_runconfigseries_configure_flags" 1 64 "$_runconfigseries_run_tests"

	fi

	runconfigtests "$_runconfigseries_CFLAGS" "$_runconfigseries_CC" \
		"$_runconfigseries_configure_flags" 1 32 "$_runconfigseries_run_tests"

	if [ "$run_gen_script" -ne 0 ]; then
		runconfigtests "$_runconfigseries_CFLAGS" "$_runconfigseries_CC" \
			"$_runconfigseries_configure_flags" 0 32 "$_runconfigseries_run_tests"
	fi
}

runtestseries() {

	_runtestseries_CFLAGS="$1"
	shift

	_runtestseries_CC="$1"
	shift

	_runtestseries_configure_flags="$1"
	shift

	_runtestseries_run_tests="$1"
	shift

	_runtestseries_flags="E H N P EH EN EP HN HP NP EHN EHP ENP HNP EHNP"

	runconfigseries "$_runtestseries_CFLAGS" "$_runtestseries_CC" \
		"$_runtestseries_configure_flags" "$_runtestseries_run_tests"

	for f in $_runtestseries_flags; do
		runconfigseries "$_runtestseries_CFLAGS" "$_runtestseries_CC" \
			"$_runtestseries_configure_flags -$f" "$_runtestseries_run_tests"
	done
}

runtests() {

	_runtests_CFLAGS="$1"
	shift

	_runtests_CC="$1"
	shift

	_runtests_configure_flags="$1"
	shift

	_runtests_run_tests="$1"
	shift

	runtestseries "-std=c99 $_runtests_CFLAGS" "$_runtests_CC" "$_runtests_configure_flags" "$_runtests_run_tests"
	runtestseries "-std=c11 $_runtests_CFLAGS" "$_runtests_CC" "$_runtests_configure_flags" "$_runtests_run_tests"
}

karatsuba() {

	header "Running Karatsuba tests"
	do_make karatsuba_test
}

vg() {

	header "Running valgrind"

	if [ "$run_64_bit" -ne 0 ]; then
		_vg_bits=64
	else
		_vg_bits=32
	fi

	build "$debug" "gcc" "-O0 -g" "1" "$_vg_bits"
	runtest valgrind

	do_make clean_config

	build "$debug" "gcc" "-O0 -gb" "1" "$_vg_bits"
	runtest valgrind

	do_make clean_config

	build "$debug" "gcc" "-O0 -gd" "1" "$_vg_bits"
	runtest valgrind

	do_make clean_config
}

debug() {

	_debug_CC="$1"
	shift

	_debug_run_tests="$1"
	shift

	runtests "$debug" "$_debug_CC" "-g" "$_debug_run_tests"

	if [ "$_debug_CC" = "clang" -a "$run_sanitizers" -ne 0 ]; then
		runtests "$debug -fsanitize=undefined" "$_debug_CC" "-g" "$_debug_run_tests"
	fi
}

release() {

	_release_CC="$1"
	shift

	_release_run_tests="$1"
	shift

	runtests "$release" "$_release_CC" "-O3" "$_release_run_tests"
}

reldebug() {

	_reldebug_CC="$1"
	shift

	_reldebug_run_tests="$1"
	shift

	runtests "$debug" "$_reldebug_CC" "-gO3" "$_reldebug_run_tests"

	if [ "$_reldebug_CC" = "clang" -a "$run_sanitizers" -ne 0 ]; then
		runtests "$debug -fsanitize=address" "$_reldebug_CC" "-gO3" "$_reldebug_run_tests"
		runtests "$debug -fsanitize=memory" "$_reldebug_CC" "-gO3" "$_reldebug_run_tests"
	fi
}

minsize() {

	_minsize_CC="$1"
	shift

	_minsize_run_tests="$1"
	shift

	runtests "$release" "$_minsize_CC" "-Os" "$_minsize_run_tests"
}

build_set() {

	_build_set_CC="$1"
	shift

	_build_set_run_tests="$1"
	shift

	debug "$_build_set_CC" "$_build_set_run_tests"
	release "$_build_set_CC" "$_build_set_run_tests"
	reldebug "$_build_set_CC" "$_build_set_run_tests"
	minsize "$_build_set_CC" "$_build_set_run_tests"
}

clang_flags="-Weverything -Wno-padded -Wno-switch-enum -Wno-format-nonliteral"
clang_flags="$clang_flags -Wno-cast-align -Wno-missing-noreturn -Wno-disabled-macro-expansion"
clang_flags="$clang_flags -Wno-unreachable-code -Wno-unreachable-code-return"
clang_flags="$clang_flags -Wno-implicit-fallthrough"
gcc_flags="-Wno-maybe-uninitialized -Wno-clobbered"

cflags="-Wall -Wextra -Werror -pedantic -Wno-conditional-uninitialized"

debug="$cflags -fno-omit-frame-pointer"
release="$cflags -DNDEBUG"

set -e

script="$0"
scriptdir=$(dirname "$script")

if [ "$#" -gt 0 ]; then
	run_tests="$1"
	shift
else
	run_tests=1
fi

if [ "$#" -gt 0 ]; then
	gen_tests="$1"
	shift
else
	gen_tests=1
fi

if [ "$#" -gt 0 ]; then
	test_with_clang="$1"
	shift
else
	test_with_clang=1
fi

if [ "$#" -gt 0 ]; then
	test_with_gcc="$1"
	shift
else
	test_with_gcc=1
fi

if [ "$#" -gt 0 ]; then
	run_sanitizers="$1"
	shift
else
	run_sanitizers=1
fi

if [ "$#" -gt 0 ]; then
	run_valgrind="$1"
	shift
else
	run_valgrind=1
fi

if [ "$#" -gt 0 ]; then
	run_64_bit="$1"
	shift
else
	run_64_bit=1
fi

if [ "$#" -gt 0 ]; then
	run_gen_script="$1"
	shift
else
	run_gen_script=0
fi

if [ "$run_64_bit" -ne 0 ]; then
	bits=64
else
	bits=32
fi

cd "$scriptdir"

if [ "$test_with_clang" -ne 0 ]; then
	defcc="clang"
elif [ "$test_with_gcc" -ne 0 ]; then
	defcc="gcc"
else
	defcc="c99"
fi

export ASAN_OPTIONS="abort_on_error=1"
export UBSAN_OPTIONS="print_stack_trace=1,silence_unsigned_overflow=1"

build "$debug" "$defcc" "-g" "1" "$bits"

header "Running math library under --standard"

printf 'quit\n' | bin/bc -ls

version=$(make version)

do_make clean_tests

if [ "$test_with_clang" -ne 0 ]; then
	build_set "clang" "$run_tests"
fi

if [ "$test_with_gcc" -ne 0 ]; then
	build_set "gcc" "$run_tests"
fi

if [ "$run_tests" -ne 0 ]; then

	build "$release" "$defcc" "-O3" "1" "$bits"

	karatsuba

	if [ "$run_valgrind" -ne 0 -a "$test_with_gcc" -ne 0 ]; then
		vg
	fi

	printf '\n'
	printf 'Tests successful.\n'

	set +e

	command -v afl-gcc > /dev/null 2>&1
	err="$?"

	set -e

	if [ "$err" -eq 0 ]; then

		header "Configuring for afl-gcc..."

		configure "$debug $gcc_flags -DBC_ENABLE_RAND=0" "afl-gcc" "-HNP -gO3" "1" "$bits"

		printf '\n'
		printf 'Run make\n'
		printf '\n'
		printf 'Then run %s/tests/randmath.py and the fuzzer.\n' "$scriptdir"
		printf '\n'
		printf 'Then run ASan on the fuzzer test cases with the following build:\n'
		printf '\n'
		printf '    CFLAGS="-fsanitize=address -fno-omit-frame-pointer -DBC_ENABLE_RAND=0" ./configure.sh -gO3 -HNPS\n'
		printf '    make\n'
		printf '\n'
		printf 'Then run the GitHub release script as follows:\n'
		printf '\n'
		printf '    <github_release> %s .travis.yml codecov.yml release.sh \\\n' "$version"
		printf '    RELEASE.md tests/afl.py tests/radamsa.sh tests/radamsa.txt tests/randmath.py \\\n'
		printf '    tests/bc/scripts/timeconst.bc\n'

	fi

fi
