#! /bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2021 Gavin D. Howard and contributors.
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

script="$0"
scriptdir=$(dirname "$script")
script=$(basename "$script")

. "$scriptdir/functions.sh"

cd "$scriptdir"

usage() {

	if [ $# -gt 0 ]; then

		_usage_val=1

		printf "%s\n\n" "$1"

	else
		_usage_val=0
	fi

	printf 'usage:\n'
	printf '    %s -h\n' "$script"
	printf '    %s --help\n' "$script"
	printf '    %s [-a|-bD|-dB|-c] [-CEfgGHlmMNPtTvz] [-O OPT_LEVEL] [-k KARATSUBA_LEN]\n' "$script"
	printf '    %s \\\n' "$script"
	printf '       [--library|--bc-only --disable-dc|--dc-only --disable-bc|--coverage]\\\n'
	printf '       [--force --debug --disable-extra-math --disable-generated-tests]    \\\n'
	printf '       [--disable-history --disable-man-pages --disable-nls]               \\\n'
	printf '       [--disable-prompt --disable-strip] [--install-all-locales]          \\\n'
	printf '       [--opt=OPT_LEVEL] [--karatsuba-len=KARATSUBA_LEN]                   \\\n'
	printf '       [--prefix=PREFIX] [--bindir=BINDIR] [--datarootdir=DATAROOTDIR]     \\\n'
	printf '       [--datadir=DATADIR] [--mandir=MANDIR] [--man1dir=MAN1DIR]           \\\n'
	printf '\n'
	printf '    -a, --library\n'
	printf '        Build the libbc instead of the programs. This is meant to be used with\n'
	printf '        Other software like programming languages that want to make use of the\n'
	printf '        parsing and math capabilities. This option will install headers using\n'
	printf '        `make install`.\n'
	printf '    -b, --bc-only\n'
	printf '        Build bc only. It is an error if "-d", "--dc-only", "-B", or\n'
	printf '        "--disable-bc" are specified too.\n'
	printf '    -B, --disable-bc\n'
	printf '        Disable bc. It is an error if "-b", "--bc-only", "-D", or "--disable-dc"\n'
	printf '        are specified too.\n'
	printf '    -c, --coverage\n'
	printf '        Generate test coverage code. Requires gcov and regcovr.\n'
	printf '        It is an error if either "-b" ("-D") or "-d" ("-B") is specified.\n'
	printf '        Requires a compiler that use gcc-compatible coverage options\n'
	printf '    -C, --disable-clean\n'
	printf '        Disable the clean that configure.sh does before configure.\n'
	printf '    -d, --dc-only\n'
	printf '        Build dc only. It is an error if "-b", "--bc-only", "-D", or\n'
	printf '        "--disable-dc" are specified too.\n'
	printf '    -D, --disable-dc\n'
	printf '        Disable dc. It is an error if "-d", "--dc-only", "-B", or "--disable-bc"\n'
	printf '        are specified too.\n'
	printf '    -E, --disable-extra-math\n'
	printf '        Disable extra math. This includes: "$" operator (truncate to integer),\n'
	printf '        "@" operator (set number of decimal places), and r(x, p) (rounding\n'
	printf '        function). Additionally, this option disables the extra printing\n'
	printf '        functions in the math library.\n'
	printf '    -f, --force\n'
	printf '        Force use of all enabled options, even if they do not work. This\n'
	printf '        option is to allow the maintainer a way to test that certain options\n'
	printf '        are not failing invisibly. (Development only.)'
	printf '    -g, --debug\n'
	printf '        Build in debug mode. Adds the "-g" flag, and if there are no\n'
	printf '        other CFLAGS, and "-O" was not given, this also adds the "-O0"\n'
	printf '        flag. If this flag is *not* given, "-DNDEBUG" is added to CPPFLAGS\n'
	printf '        and a strip flag is added to the link stage.\n'
	printf '    -G, --disable-generated-tests\n'
	printf '        Disable generating tests. This is for platforms that do not have a\n'
	printf '        GNU bc-compatible bc to generate tests.\n'
	printf '    -h, --help\n'
	printf '        Print this help message and exit.\n'
	printf '    -H, --disable-history\n'
	printf '        Disable history.\n'
	printf '    -k KARATSUBA_LEN, --karatsuba-len KARATSUBA_LEN\n'
	printf '        Set the karatsuba length to KARATSUBA_LEN (default is 64).\n'
	printf '        It is an error if KARATSUBA_LEN is not a number or is less than 16.\n'
	printf '    -l, --install-all-locales\n'
	printf '        Installs all locales, regardless of how many are on the system. This\n'
	printf '        option is useful for package maintainers who want to make sure that\n'
	printf '        a package contains all of the locales that end users might need.\n'
	printf '    -m, --enable-memcheck\n'
	printf '        Enable memcheck mode, to ensure no memory leaks. For development only.\n'
	printf '    -M, --disable-man-pages\n'
	printf '        Disable installing manpages.\n'
	printf '    -N, --disable-nls\n'
	printf '        Disable POSIX locale (NLS) support.\n'
	printf '    -O OPT_LEVEL, --opt OPT_LEVEL\n'
	printf '        Set the optimization level. This can also be included in the CFLAGS,\n'
	printf '        but it is provided, so maintainers can build optimized debug builds.\n'
	printf '        This is passed through to the compiler, so it must be supported.\n'
	printf '    -P, --disable-prompt\n'
	printf '        Disables the prompt in the built bc. The prompt will never show up,\n'
	printf '        or in other words, it will be permanently disabled and cannot be\n'
	printf '        enabled.\n'
	printf '    -t, --enable-test-timing\n'
	printf '        Enable the timing of tests. This is for development only.\n'
	printf '    -T, --disable-strip\n'
	printf '        Disable stripping symbols from the compiled binary or binaries.\n'
	printf '        Stripping symbols only happens when debug mode is off.\n'
	printf '    -v, --enable-valgrind\n'
	printf '        Enable a build appropriate for valgrind. For development only.\n'
	printf '    -z, --enable-fuzz-mode\n'
	printf '        Enable fuzzing mode. THIS IS FOR DEVELOPMENT ONLY.\n'
	printf '    --prefix PREFIX\n'
	printf '        The prefix to install to. Overrides "$PREFIX" if it exists.\n'
	printf '        If PREFIX is "/usr", install path will be "/usr/bin".\n'
	printf '        Default is "/usr/local".\n'
	printf '    --bindir BINDIR\n'
	printf '        The directory to install binaries in. Overrides "$BINDIR" if it exists.\n'
	printf '        Default is "$PREFIX/bin".\n'
	printf '    --includedir INCLUDEDIR\n'
	printf '        The directory to install headers in. Overrides "$INCLUDEDIR" if it\n'
	printf '        exists. Default is "$PREFIX/include".\n'
	printf '    --libdir LIBDIR\n'
	printf '        The directory to install libraries in. Overrides "$LIBDIR" if it exists.\n'
	printf '        Default is "$PREFIX/lib".\n'
	printf '    --datarootdir DATAROOTDIR\n'
	printf '        The root location for data files. Overrides "$DATAROOTDIR" if it exists.\n'
	printf '        Default is "$PREFIX/share".\n'
	printf '    --datadir DATADIR\n'
	printf '        The location for data files. Overrides "$DATADIR" if it exists.\n'
	printf '        Default is "$DATAROOTDIR".\n'
	printf '    --mandir MANDIR\n'
	printf '        The location to install manpages to. Overrides "$MANDIR" if it exists.\n'
	printf '        Default is "$DATADIR/man".\n'
	printf '    --man1dir MAN1DIR\n'
	printf '        The location to install Section 1 manpages to. Overrides "$MAN1DIR" if\n'
	printf '        it exists. Default is "$MANDIR/man1".\n'
	printf '    --man3dir MAN3DIR\n'
	printf '        The location to install Section 3 manpages to. Overrides "$MAN3DIR" if\n'
	printf '        it exists. Default is "$MANDIR/man3".\n'
	printf '\n'
	printf 'In addition, the following environment variables are used:\n'
	printf '\n'
	printf '    CC           C compiler. Must be compatible with POSIX c99. If there is a\n'
	printf '                 space in the basename of the compiler, the items after the\n'
	printf '                 first space are assumed to be compiler flags, and in that case,\n'
	printf '                 the flags are automatically moved into CFLAGS. Default is\n'
	printf '                 "c99".\n'
	printf '    HOSTCC       Host C compiler. Must be compatible with POSIX c99. If there is\n'
	printf '                 a space in the basename of the compiler, the items after the\n'
	printf '                 first space are assumed to be compiler flags, and in the case,\n'
	printf '                 the flags are automatically moved into HOSTCFLAGS. Default is\n'
	printf '                 "$CC".\n'
	printf '    HOST_CC      Same as HOSTCC. If HOSTCC also exists, it is used.\n'
	printf '    CFLAGS       C compiler flags.\n'
	printf '    HOSTCFLAGS   CFLAGS for HOSTCC. Default is "$CFLAGS".\n'
	printf '    HOST_CFLAGS  Same as HOST_CFLAGS. If HOST_CFLAGS also exists, it is used.\n'
	printf '    CPPFLAGS     C preprocessor flags. Default is "".\n'
	printf '    LDFLAGS      Linker flags. Default is "".\n'
	printf '    PREFIX       The prefix to install to. Default is "/usr/local".\n'
	printf '                 If PREFIX is "/usr", install path will be "/usr/bin".\n'
	printf '    BINDIR       The directory to install binaries in. Default is "$PREFIX/bin".\n'
	printf '    INCLUDEDIR   The directory to install header files in. Default is\n'
	printf '                 "$PREFIX/include".\n'
	printf '    LIBDIR       The directory to install libraries in. Default is\n'
	printf '                 "$PREFIX/lib".\n'
	printf '    DATAROOTDIR  The root location for data files. Default is "$PREFIX/share".\n'
	printf '    DATADIR      The location for data files. Default is "$DATAROOTDIR".\n'
	printf '    MANDIR       The location to install manpages to. Default is "$DATADIR/man".\n'
	printf '    MAN1DIR      The location to install Section 1 manpages to. Default is\n'
	printf '                 "$MANDIR/man1".\n'
	printf '    MAN3DIR      The location to install Section 3 manpages to. Default is\n'
	printf '                 "$MANDIR/man3".\n'
	printf '    NLSPATH      The location to install locale catalogs to. Must be an absolute\n'
	printf '                 path (or contain one). This is treated the same as the POSIX\n'
	printf '                 definition of $NLSPATH (see POSIX environment variables for\n'
	printf '                 more information). Default is "/usr/share/locale/%%L/%%N".\n'
	printf '    EXECSUFFIX   The suffix to append to the executable names, used to not\n'
	printf '                 interfere with other installed bc executables. Default is "".\n'
	printf '    EXECPREFIX   The prefix to append to the executable names, used to not\n'
	printf '                 interfere with other installed bc executables. Default is "".\n'
	printf '    DESTDIR      For package creation. Default is "". If it is empty when\n'
	printf '                 `%s` is run, it can also be passed to `make install`\n' "$script"
	printf '                 later as an environment variable. If both are specified,\n'
	printf '                 the one given to `%s` takes precedence.\n' "$script"
	printf '    LONG_BIT     The number of bits in a C `long` type. This is mostly for the\n'
	printf '                 embedded space since this `bc` uses `long`s internally for\n'
	printf '                 overflow checking. In C99, a `long` is required to be 32 bits.\n'
	printf '                 For most normal desktop systems, setting this is unnecessary,\n'
	printf '                 except that 32-bit platforms with 64-bit longs may want to set\n'
	printf '                 it to `32`. Default is the default of `LONG_BIT` for the target\n'
	printf '                 platform. Minimum allowed is `32`. It is a build time error if\n'
	printf '                 the specified value of `LONG_BIT` is greater than the default\n'
	printf '                 value of `LONG_BIT` for the target platform.\n'
	printf '    GEN_HOST     Whether to use `gen/strgen.c`, instead of `gen/strgen.sh`, to\n'
	printf '                 produce the C files that contain the help texts as well as the\n'
	printf '                 math libraries. By default, `gen/strgen.c` is used, compiled by\n'
	printf '                 "$HOSTCC" and run on the host machine. Using `gen/strgen.sh`\n'
	printf '                 removes the need to compile and run an executable on the host\n'
	printf '                 machine since `gen/strgen.sh` is a POSIX shell script. However,\n'
	printf '                 `gen/lib2.bc` is perilously close to 4095 characters, the max\n'
	printf '                 supported length of a string literal in C99 (and it could be\n'
	printf '                 added to in the future), and `gen/strgen.sh` generates a string\n'
	printf '                 literal instead of an array, as `gen/strgen.c` does. For most\n'
	printf '                 production-ready compilers, this limit probably is not\n'
	printf '                 enforced, but it could be. Both options are still available for\n'
	printf '                 this reason. If you are sure your compiler does not have the\n'
	printf '                 limit and do not want to compile and run a binary on the host\n'
	printf '                 machine, set this variable to "0". Any other value, or a\n'
	printf '                 non-existent value, will cause the build system to compile and\n'
	printf '                 run `gen/strgen.c`. Default is "".\n'
	printf '    GEN_EMU      Emulator to run string generator code under (leave empty if not\n'
	printf '                 necessary). This is not necessary when using `gen/strgen.sh`.\n'
	printf '                 Default is "".\n'
	printf '\n'
	printf 'WARNING: even though `configure.sh` supports both option types, short and\n'
	printf 'long, it does not support handling both at the same time. Use only one type.\n'

	exit "$_usage_val"
}

replace_ext() {

	if [ "$#" -ne 3 ]; then
		err_exit "Invalid number of args to $0"
	fi

	_replace_ext_file="$1"
	_replace_ext_ext1="$2"
	_replace_ext_ext2="$3"

	_replace_ext_result=${_replace_ext_file%.$_replace_ext_ext1}.$_replace_ext_ext2

	printf '%s\n' "$_replace_ext_result"
}

replace_exts() {

	if [ "$#" -ne 3 ]; then
		err_exit "Invalid number of args to $0"
	fi

	_replace_exts_files="$1"
	_replace_exts_ext1="$2"
	_replace_exts_ext2="$3"

	for _replace_exts_file in $_replace_exts_files; do
		_replace_exts_new_name=$(replace_ext "$_replace_exts_file" "$_replace_exts_ext1" "$_replace_exts_ext2")
		_replace_exts_result="$_replace_exts_result $_replace_exts_new_name"
	done

	printf '%s\n' "$_replace_exts_result"
}

replace() {

	if [ "$#" -ne 3 ]; then
		err_exit "Invalid number of args to $0"
	fi

	_replace_str="$1"
	_replace_needle="$2"
	_replace_replacement="$3"

	substring_replace "$_replace_str" "%%$_replace_needle%%" "$_replace_replacement"
}

find_src_files() {

	if [ "$#" -ge 1 ] && [ "$1" != "" ]; then

		while [ "$#" -ge 1 ]; do
			_find_src_files_a="${1## }"
			shift
			_find_src_files_args="$_find_src_files_args ! -path src/${_find_src_files_a}"
		done

	else
		_find_src_files_args="-print"
	fi

	printf '%s\n' $(find src/ -depth -name "*.c" $_find_src_files_args)
}

gen_file_list() {

	if [ "$#" -lt 1 ]; then
		err_exit "Invalid number of args to $0"
	fi

	_gen_file_list_contents="$1"
	shift

	p=$(pwd)

	cd "$scriptdir"

	if [ "$#" -ge 1 ]; then
		_gen_file_list_unneeded="$@"
	else
		_gen_file_list_unneeded=""
	fi

	_gen_file_list_needle_src="SRC"
	_gen_file_list_needle_obj="OBJ"
	_gen_file_list_needle_gcda="GCDA"
	_gen_file_list_needle_gcno="GCNO"

	_gen_file_list_replacement=$(find_src_files $_gen_file_list_unneeded | tr '\n' ' ')
	_gen_file_list_contents=$(replace "$_gen_file_list_contents" \
		"$_gen_file_list_needle_src" "$_gen_file_list_replacement")

	_gen_file_list_replacement=$(replace_exts "$_gen_file_list_replacement" "c" "o")
	_gen_file_list_contents=$(replace "$_gen_file_list_contents" \
		"$_gen_file_list_needle_obj" "$_gen_file_list_replacement")

	_gen_file_list_replacement=$(replace_exts "$_gen_file_list_replacement" "o" "gcda")
	_gen_file_list_contents=$(replace "$_gen_file_list_contents" \
		"$_gen_file_list_needle_gcda" "$_gen_file_list_replacement")

	_gen_file_list_replacement=$(replace_exts "$_gen_file_list_replacement" "gcda" "gcno")
	_gen_file_list_contents=$(replace "$_gen_file_list_contents" \
		"$_gen_file_list_needle_gcno" "$_gen_file_list_replacement")

	cd "$p"

	printf '%s\n' "$_gen_file_list_contents"
}

gen_tests() {

	_gen_tests_name="$1"
	shift

	_gen_tests_uname="$1"
	shift

	_gen_tests_extra_math="$1"
	shift

	_gen_tests_time_tests="$1"
	shift

	_gen_tests_extra_required=$(cat tests/extra_required.txt)

	for _gen_tests_t in $(cat "$scriptdir/tests/$_gen_tests_name/all.txt"); do

		if [ "$_gen_tests_extra_math" -eq 0 ]; then

			if [ -z "${_gen_tests_extra_required##*$_gen_tests_t*}" ]; then
				printf 'test_%s_%s:\n\t@printf "Skipping %s %s\\n"\n\n' \
					"$_gen_tests_name" "$_gen_tests_t" "$_gen_tests_name" \
					"$_gen_tests_t" >> "$scriptdir/Makefile"
				continue
			fi

		fi

		printf 'test_%s_%s:\n\t@sh tests/test.sh %s %s %s %s %s\n\n' \
			"$_gen_tests_name" "$_gen_tests_t" "$_gen_tests_name" \
			"$_gen_tests_t" "$generate_tests" "$time_tests" \
			"$*" >> "$scriptdir/Makefile"

	done
}

gen_test_targets() {

	_gen_test_targets_name="$1"
	shift

	_gen_test_targets_tests=$(cat "$scriptdir/tests/${_gen_test_targets_name}/all.txt")

	for _gen_test_targets_t in $_gen_test_targets_tests; do
		printf ' test_%s_%s' "$_gen_test_targets_name" "$_gen_test_targets_t"
	done

	printf '\n'
}

gen_script_tests() {

	_gen_script_tests_name="$1"
	shift

	_gen_script_tests_extra_math="$1"
	shift

	_gen_script_tests_generate="$1"
	shift

	_gen_script_tests_time="$1"
	shift

	_gen_script_tests_tests=$(cat "$scriptdir/tests/$_gen_script_tests_name/scripts/all.txt")

	for _gen_script_tests_f in $_gen_script_tests_tests; do

		_gen_script_tests_b=$(basename "$_gen_script_tests_f" ".${_gen_script_tests_name}")

		printf 'test_%s_script_%s:\n\t@sh tests/script.sh %s %s %s 1 %s %s %s\n\n' \
			"$_gen_script_tests_name" "$_gen_script_tests_b" "$_gen_script_tests_name" \
			"$_gen_script_tests_f" "$_gen_script_tests_extra_math" "$_gen_script_tests_generate" \
			"$_gen_script_tests_time" "$*" >> "$scriptdir/Makefile"
	done
}

gen_script_test_targets() {

	_gen_script_test_targets_name="$1"
	shift

	_gen_script_test_targets_tests=$(cat "$scriptdir/tests/$_gen_script_test_targets_name/scripts/all.txt")

	for _gen_script_test_targets_f in $_gen_script_test_targets_tests; do
		_gen_script_test_targets_b=$(basename "$_gen_script_test_targets_f" \
			".$_gen_script_test_targets_name")
		printf ' test_%s_script_%s' "$_gen_script_test_targets_name" \
			"$_gen_script_test_targets_b"
	done

	printf '\n'
}

bc_only=0
dc_only=0
coverage=0
karatsuba_len=32
debug=0
hist=1
extra_math=1
optimization=""
generate_tests=1
install_manpages=1
nls=1
prompt=1
force=0
strip_bin=1
all_locales=0
library=0
fuzz=0
time_tests=0
vg=0
memcheck=0
clean=1

while getopts "abBcdDEfgGhHk:lMmNO:PStTvz-" opt; do

	case "$opt" in
		a) library=1 ;;
		b) bc_only=1 ;;
		B) dc_only=1 ;;
		c) coverage=1 ;;
		C) clean=0 ;;
		d) dc_only=1 ;;
		D) bc_only=1 ;;
		E) extra_math=0 ;;
		f) force=1 ;;
		g) debug=1 ;;
		G) generate_tests=0 ;;
		h) usage ;;
		H) hist=0 ;;
		k) karatsuba_len="$OPTARG" ;;
		l) all_locales=1 ;;
		m) memcheck=1 ;;
		M) install_manpages=0 ;;
		N) nls=0 ;;
		O) optimization="$OPTARG" ;;
		P) prompt=0 ;;
		t) time_tests=1 ;;
		T) strip_bin=0 ;;
		v) vg=1 ;;
		z) fuzz=1 ;;
		-)
			arg="$1"
			arg="${arg#--}"
			LONG_OPTARG="${arg#*=}"
			case $arg in
				help) usage ;;
				library) library=1 ;;
				bc-only) bc_only=1 ;;
				dc-only) dc_only=1 ;;
				coverage) coverage=1 ;;
				debug) debug=1 ;;
				force) force=1 ;;
				prefix=?*) PREFIX="$LONG_OPTARG" ;;
				prefix)
					if [ "$#" -lt 2 ]; then
						usage "No argument given for '--$arg' option"
					fi
					PREFIX="$2"
					shift ;;
				bindir=?*) BINDIR="$LONG_OPTARG" ;;
				bindir)
					if [ "$#" -lt 2 ]; then
						usage "No argument given for '--$arg' option"
					fi
					BINDIR="$2"
					shift ;;
				includedir=?*) INCLUDEDIR="$LONG_OPTARG" ;;
				includedir)
					if [ "$#" -lt 2 ]; then
						usage "No argument given for '--$arg' option"
					fi
					INCLUDEDIR="$2"
					shift ;;
				libdir=?*) LIBDIR="$LONG_OPTARG" ;;
				libdir)
					if [ "$#" -lt 2 ]; then
						usage "No argument given for '--$arg' option"
					fi
					LIBDIR="$2"
					shift ;;
				datarootdir=?*) DATAROOTDIR="$LONG_OPTARG" ;;
				datarootdir)
					if [ "$#" -lt 2 ]; then
						usage "No argument given for '--$arg' option"
					fi
					DATAROOTDIR="$2"
					shift ;;
				datadir=?*) DATADIR="$LONG_OPTARG" ;;
				datadir)
					if [ "$#" -lt 2 ]; then
						usage "No argument given for '--$arg' option"
					fi
					DATADIR="$2"
					shift ;;
				mandir=?*) MANDIR="$LONG_OPTARG" ;;
				mandir)
					if [ "$#" -lt 2 ]; then
						usage "No argument given for '--$arg' option"
					fi
					MANDIR="$2"
					shift ;;
				man1dir=?*) MAN1DIR="$LONG_OPTARG" ;;
				man1dir)
					if [ "$#" -lt 2 ]; then
						usage "No argument given for '--$arg' option"
					fi
					MAN1DIR="$2"
					shift ;;
				man3dir=?*) MAN3DIR="$LONG_OPTARG" ;;
				man3dir)
					if [ "$#" -lt 2 ]; then
						usage "No argument given for '--$arg' option"
					fi
					MAN3DIR="$2"
					shift ;;
				localedir=?*) LOCALEDIR="$LONG_OPTARG" ;;
				localedir)
					if [ "$#" -lt 2 ]; then
						usage "No argument given for '--$arg' option"
					fi
					LOCALEDIR="$2"
					shift ;;
				karatsuba-len=?*) karatsuba_len="$LONG_OPTARG" ;;
				karatsuba-len)
					if [ "$#" -lt 2 ]; then
						usage "No argument given for '--$arg' option"
					fi
					karatsuba_len="$1"
					shift ;;
				opt=?*) optimization="$LONG_OPTARG" ;;
				opt)
					if [ "$#" -lt 2 ]; then
						usage "No argument given for '--$arg' option"
					fi
					optimization="$1"
					shift ;;
				disable-bc) dc_only=1 ;;
				disable-dc) bc_only=1 ;;
				disable-clean) clean=0 ;;
				disable-extra-math) extra_math=0 ;;
				disable-generated-tests) generate_tests=0 ;;
				disable-history) hist=0 ;;
				disable-man-pages) install_manpages=0 ;;
				disable-nls) nls=0 ;;
				disable-prompt) prompt=0 ;;
				disable-strip) strip_bin=0 ;;
				enable-test-timing) time_tests=1 ;;
				enable-valgrind) vg=1 ;;
				enable-fuzz-mode) fuzz=1 ;;
				enable-memcheck) memcheck=1 ;;
				install-all-locales) all_locales=1 ;;
				help* | bc-only* | dc-only* | coverage* | debug*)
					usage "No arg allowed for --$arg option" ;;
				disable-bc* | disable-dc* | disable-clean*)
					usage "No arg allowed for --$arg option" ;;
				disable-extra-math*)
					usage "No arg allowed for --$arg option" ;;
				disable-generated-tests* | disable-history*)
					usage "No arg allowed for --$arg option" ;;
				disable-man-pages* | disable-nls* | disable-strip*)
					usage "No arg allowed for --$arg option" ;;
				enable-fuzz-mode* | enable-test-timing* | enable-valgrind*)
					usage "No arg allowed for --$arg option" ;;
				enable-memcheck* | install-all-locales*)
					usage "No arg allowed for --$arg option" ;;
				'') break ;; # "--" terminates argument processing
				* ) usage "Invalid option $LONG_OPTARG" ;;
			esac
			shift
			OPTIND=1 ;;
		?) usage "Invalid option $opt" ;;
	esac

done

if [ "$clean" -ne 0 ]; then
	if [ -f ./Makefile ]; then
		make clean_config > /dev/null
	fi
fi

if [ "$bc_only" -eq 1 ] && [ "$dc_only" -eq 1 ]; then
	usage "Can only specify one of -b(-D) or -d(-B)"
fi

if [ "$library" -ne 0 ]; then
	if [ "$bc_only" -eq 1 ] || [ "$dc_only" -eq 1 ]; then
		usage "Must not specify -b(-D) or -d(-B) when building the library"
	fi
fi

case $karatsuba_len in
	(*[!0-9]*|'') usage "KARATSUBA_LEN is not a number" ;;
	(*) ;;
esac

if [ "$karatsuba_len" -lt 16 ]; then
	usage "KARATSUBA_LEN is less than 16"
fi

set -e

if [ -z "${LONG_BIT+set}" ]; then
	LONG_BIT_DEFINE=""
elif [ "$LONG_BIT" -lt 32 ]; then
	usage "LONG_BIT is less than 32"
else
	LONG_BIT_DEFINE="-DBC_LONG_BIT=\$(BC_LONG_BIT)"
fi

if [ -z "$CC" ]; then
	CC="c99"
else
	ccbase=$(basename "$CC")
	suffix=" *"
	prefix="* "

	if [ "${ccbase%%$suffix}" != "$ccbase" ]; then
		ccflags="${ccbase#$prefix}"
		cc="${ccbase%%$suffix}"
		ccdir=$(dirname "$CC")
		if [ "$ccdir" = "." ] && [ "${CC#.}" = "$CC" ]; then
			ccdir=""
		else
			ccdir="$ccdir/"
		fi
		CC="${ccdir}${cc}"
		CFLAGS="$CFLAGS $ccflags"
	fi
fi

if [ -z "$HOSTCC" ] && [ -z "$HOST_CC" ]; then
	HOSTCC="$CC"
elif [ -z "$HOSTCC" ]; then
	HOSTCC="$HOST_CC"
fi

if [ "$HOSTCC" != "$CC" ]; then
	ccbase=$(basename "$HOSTCC")
	suffix=" *"
	prefix="* "

	if [ "${ccbase%%$suffix}" != "$ccbase" ]; then
		ccflags="${ccbase#$prefix}"
		cc="${ccbase%%$suffix}"
		ccdir=$(dirname "$HOSTCC")
		if [ "$ccdir" = "." ] && [ "${HOSTCC#.}" = "$HOSTCC" ]; then
			ccdir=""
		else
			ccdir="$ccdir/"
		fi
		HOSTCC="${ccdir}${cc}"
		HOSTCFLAGS="$HOSTCFLAGS $ccflags"
	fi
fi

if [ -z "${HOSTCFLAGS+set}" ] && [ -z "${HOST_CFLAGS+set}" ]; then
	HOSTCFLAGS="$CFLAGS"
elif [ -z "${HOSTCFLAGS+set}" ]; then
	HOSTCFLAGS="$HOST_CFLAGS"
fi

link="@printf 'No link necessary\\\\n'"
main_exec="BC"
executable="BC_EXEC"

tests="test_bc timeconst test_dc"

bc_test="@tests/all.sh bc $extra_math 1 $generate_tests 0 \$(BC_EXEC)"
dc_test="@tests/all.sh dc $extra_math 1 $generate_tests 0 \$(DC_EXEC)"

timeconst="@tests/bc/timeconst.sh tests/bc/scripts/timeconst.bc \$(BC_EXEC)"

# In order to have cleanup at exit, we need to be in
# debug mode, so don't run valgrind without that.
if [ "$vg" -ne 0 ]; then
	debug=1
	bc_test_exec='valgrind $(VALGRIND_ARGS) $(BC_EXEC)'
	dc_test_exec='valgrind $(VALGRIND_ARGS) $(DC_EXEC)'
else
	bc_test_exec='$(BC_EXEC)'
	dc_test_exec='$(DC_EXEC)'
fi

karatsuba="@printf 'karatsuba cannot be run because one of bc or dc is not built\\\\n'"
karatsuba_test="@printf 'karatsuba cannot be run because one of bc or dc is not built\\\\n'"

bc_lib="\$(GEN_DIR)/lib.o"
bc_help="\$(GEN_DIR)/bc_help.o"
dc_help="\$(GEN_DIR)/dc_help.o"

default_target_prereqs="\$(BIN) \$(OBJS)"
default_target_cmd="\$(CC) \$(CFLAGS) \$(OBJS) \$(LDFLAGS) -o \$(EXEC)"
default_target="\$(DC_EXEC)"

second_target_prereqs=""
second_target_cmd="$default_target_cmd"
second_target="\$(BC_EXEC)"

if [ "$library" -ne 0 ]; then

	extra_math=1
	nls=0
	hist=0
	prompt=0
	bc=1
	dc=1

	default_target_prereqs="\$(BIN) \$(OBJ)"
	default_target_cmd="ar -r -cu \$(LIBBC) \$(OBJ)"
	default_target="\$(LIBBC)"
	tests="test_library"

elif [ "$bc_only" -eq 1 ]; then

	bc=1
	dc=0

	dc_help=""

	executables="bc"

	dc_test="@printf 'No dc tests to run\\\\n'"

	install_prereqs=" install_execs"
	install_man_prereqs=" install_bc_manpage"
	uninstall_prereqs=" uninstall_bc"
	uninstall_man_prereqs=" uninstall_bc_manpage"

	default_target="\$(BC_EXEC)"
	second_target="\$(DC_EXEC)"
	tests="test_bc timeconst"

elif [ "$dc_only" -eq 1 ]; then

	bc=0
	dc=1

	bc_lib=""
	bc_help=""

	executables="dc"

	main_exec="DC"
	executable="DC_EXEC"

	bc_test="@printf 'No bc tests to run\\\\n'"

	timeconst="@printf 'timeconst cannot be run because bc is not built\\\\n'"

	install_prereqs=" install_execs"
	install_man_prereqs=" install_dc_manpage"
	uninstall_prereqs=" uninstall_dc"
	uninstall_man_prereqs=" uninstall_dc_manpage"

	tests="test_dc"

else

	bc=1
	dc=1

	executables="bc and dc"

	karatsuba="@\$(KARATSUBA) 30 0 \$(BC_EXEC)"
	karatsuba_test="@\$(KARATSUBA) 1 100 \$(BC_EXEC)"

	if [ "$library" -eq 0 ]; then
		install_prereqs=" install_execs"
		install_man_prereqs=" install_bc_manpage install_dc_manpage"
		uninstall_prereqs=" uninstall_bc uninstall_dc"
		uninstall_man_prereqs=" uninstall_bc_manpage uninstall_dc_manpage"
	else
		install_prereqs=" install_library install_bcl_header"
		install_man_prereqs=" install_bcl_manpage"
		uninstall_prereqs=" uninstall_library uninstall_bcl_header"
		uninstall_man_prereqs=" uninstall_bcl_manpage"
		tests="test_library"
	fi

	second_target_prereqs="$default_target_prereqs"
	default_target_prereqs="$second_target"
	default_target_cmd="\$(LINK) \$(BIN) \$(EXEC_PREFIX)\$(DC)"

fi

if [ "$fuzz" -ne 0 ]; then
	debug=1
	hist=0
	prompt=0
	nls=0
	optimization="3"
fi

if [ "$debug" -eq 1 ]; then

	if [ -z "$CFLAGS" ] && [ -z "$optimization" ]; then
		CFLAGS="-O0"
	fi

	CFLAGS="-g $CFLAGS"

else
	CPPFLAGS="-DNDEBUG $CPPFLAGS"
	if [ "$strip_bin" -ne 0 ]; then
		LDFLAGS="-s $LDFLAGS"
	fi
fi

if [ -n "$optimization" ]; then
	CFLAGS="-O$optimization $CFLAGS"
fi

if [ "$coverage" -eq 1 ]; then

	if [ "$bc_only" -eq 1 ] || [ "$dc_only" -eq 1 ]; then
		usage "Can only specify -c without -b or -d"
	fi

	CFLAGS="-fprofile-arcs -ftest-coverage -g -O0 $CFLAGS"
	CPPFLAGS="-DNDEBUG $CPPFLAGS"

	COVERAGE_OUTPUT="@gcov -pabcdf \$(GCDA) \$(BC_GCDA) \$(DC_GCDA) \$(HISTORY_GCDA) \$(RAND_GCDA)"
	COVERAGE_OUTPUT="$COVERAGE_OUTPUT;\$(RM) -f \$(GEN)*.gc*"
	COVERAGE_OUTPUT="$COVERAGE_OUTPUT;gcovr --html-details --output index.html"
	COVERAGE_PREREQS=" test coverage_output"

else
	COVERAGE_OUTPUT="@printf 'Coverage not generated\\\\n'"
	COVERAGE_PREREQS=""
fi

if [ -z "${DESTDIR+set}" ]; then
	destdir=""
else
	destdir="DESTDIR = $DESTDIR"
fi

if [ -z "${PREFIX+set}" ]; then
	PREFIX="/usr/local"
fi

if [ -z "${BINDIR+set}" ]; then
	BINDIR="$PREFIX/bin"
fi

if [ -z "${INCLUDEDIR+set}" ]; then
	INCLUDEDIR="$PREFIX/include"
fi

if [ -z "${LIBDIR+set}" ]; then
	LIBDIR="$PREFIX/lib"
fi

if [ "$install_manpages" -ne 0 ] || [ "$nls" -ne 0 ]; then
	if [ -z "${DATAROOTDIR+set}" ]; then
		DATAROOTDIR="$PREFIX/share"
	fi
fi

if [ "$install_manpages" -ne 0 ]; then

	if [ -z "${DATADIR+set}" ]; then
		DATADIR="$DATAROOTDIR"
	fi

	if [ -z "${MANDIR+set}" ]; then
		MANDIR="$DATADIR/man"
	fi

	if [ -z "${MAN1DIR+set}" ]; then
		MAN1DIR="$MANDIR/man1"
	fi

	if [ -z "${MAN3DIR+set}" ]; then
		MAN3DIR="$MANDIR/man3"
	fi

else
	install_man_prereqs=""
	uninstall_man_prereqs=""
fi

if [ "$nls" -ne 0 ]; then

	set +e

	printf 'Testing NLS...\n'

	flags="-DBC_ENABLE_NLS=1 -DBC_ENABLED=$bc -DDC_ENABLED=$dc"
	flags="$flags -DBC_ENABLE_HISTORY=$hist"
	flags="$flags -DBC_ENABLE_EXTRA_MATH=$extra_math -I./include/"
	flags="$flags -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700"

	"$CC" $CPPFLAGS $CFLAGS $flags -c "src/vm.c" -o "$scriptdir/vm.o" > /dev/null 2>&1

	err="$?"

	rm -rf "$scriptdir/vm.o"

	# If this errors, it is probably because of building on Windows,
	# and NLS is not supported on Windows, so disable it.
	if [ "$err" -ne 0 ]; then
		printf 'NLS does not work.\n'
		if [ $force -eq 0 ]; then
			printf 'Disabling NLS...\n\n'
			nls=0
		else
			printf 'Forcing NLS...\n\n'
		fi
	else
		printf 'NLS works.\n\n'

		printf 'Testing gencat...\n'
		gencat "$scriptdir/en_US.cat" "$scriptdir/locales/en_US.msg" > /dev/null 2>&1

		err="$?"

		rm -rf "$scriptdir/en_US.cat"

		if [ "$err" -ne 0 ]; then
			printf 'gencat does not work.\n'
			if [ $force -eq 0 ]; then
				printf 'Disabling NLS...\n\n'
				nls=0
			else
				printf 'Forcing NLS...\n\n'
			fi
		else

			printf 'gencat works.\n\n'

			if [ "$HOSTCC" != "$CC" ]; then
				printf 'Cross-compile detected.\n\n'
				printf 'WARNING: Catalog files generated with gencat may not be portable\n'
				printf '         across different architectures.\n\n'
			fi

			if [ -z "$NLSPATH" ]; then
				NLSPATH="/usr/share/locale/%L/%N"
			fi

			install_locales_prereqs=" install_locales"
			uninstall_locales_prereqs=" uninstall_locales"

		fi

	fi

	set -e

else
	install_locales_prereqs=""
	uninstall_locales_prereqs=""
	all_locales=0
fi

if [ "$nls" -ne 0 ] && [ "$all_locales" -ne 0 ]; then
	install_locales="\$(LOCALE_INSTALL) -l \$(NLSPATH) \$(MAIN_EXEC) \$(DESTDIR)"
else
	install_locales="\$(LOCALE_INSTALL) \$(NLSPATH) \$(MAIN_EXEC) \$(DESTDIR)"
fi

if [ "$hist" -eq 1 ]; then

	set +e

	printf 'Testing history...\n'

	flags="-DBC_ENABLE_HISTORY=1 -DBC_ENABLED=$bc -DDC_ENABLED=$dc"
	flags="$flags -DBC_ENABLE_NLS=$nls -DBC_ENABLE_LIBRARY=0"
	flags="$flags -DBC_ENABLE_EXTRA_MATH=$extra_math -I./include/"
	flags="$flags -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700"

	"$CC" $CPPFLAGS $CFLAGS $flags -c "src/history.c" -o "$scriptdir/history.o" > /dev/null 2>&1

	err="$?"

	rm -rf "$scriptdir/history.o"

	# If this errors, it is probably because of building on Windows,
	# and history is not supported on Windows, so disable it.
	if [ "$err" -ne 0 ]; then
		printf 'History does not work.\n'
		if [ $force -eq 0 ]; then
			printf 'Disabling history...\n\n'
			hist=0
		else
			printf 'Forcing history...\n\n'
		fi
	else
		printf 'History works.\n\n'
	fi

	set -e

fi

if [ "$library" -eq 1 ]; then
	bc_lib=""
fi

if [ "$extra_math" -eq 1 ] && [ "$bc" -ne 0 ] && [ "$library" -eq 0 ]; then
	BC_LIB2_O="\$(GEN_DIR)/lib2.o"
else
	BC_LIB2_O=""
fi

GEN="strgen"
GEN_EXEC_TARGET="\$(HOSTCC) \$(HOSTCFLAGS) -o \$(GEN_EXEC) \$(GEN_C)"
CLEAN_PREREQS=" clean_gen"

if [ -z "${GEN_HOST+set}" ]; then
	GEN_HOST=1
else
	if [ "$GEN_HOST" -eq 0 ]; then
		GEN="strgen.sh"
		GEN_EXEC_TARGET="@printf 'Do not need to build gen/strgen.c\\\\n'"
		CLEAN_PREREQS=""
	fi
fi

manpage_args=""
unneeded=""
headers="\$(HEADERS)"

if [ "$extra_math" -eq 0 ]; then
	manpage_args="E"
	unneeded="$unneeded rand.c"
else
	headers="$headers \$(EXTRA_MATH_HEADERS)"
fi

if [ "$hist" -eq 0 ]; then
	manpage_args="${manpage_args}H"
	unneeded="$unneeded history.c"
else
	headers="$headers \$(HISTORY_HEADERS)"
fi

if [ "$nls" -eq 0 ]; then
	manpage_args="${manpage_args}N"
fi

if [ "$prompt" -eq 0 ]; then
	manpage_args="${manpage_args}P"
fi

if [ "$bc" -eq 0 ]; then
	unneeded="$unneeded bc.c bc_lex.c bc_parse.c"
else
	headers="$headers \$(BC_HEADERS)"
fi

if [ "$dc" -eq 0 ]; then
	unneeded="$unneeded dc.c dc_lex.c dc_parse.c"
else
	headers="$headers \$(DC_HEADERS)"
fi

if [ "$library" -ne 0 ]; then
	unneeded="$unneeded args.c opt.c read.c file.c main.c"
	unneeded="$unneeded lang.c lex.c parse.c program.c"
	unneeded="$unneeded bc.c bc_lex.c bc_parse.c"
	unneeded="$unneeded dc.c dc_lex.c dc_parse.c"
	headers="$headers \$(LIBRARY_HEADERS)"
else
	unneeded="$unneeded library.c"
fi

if [ "$manpage_args" = "" ]; then
	manpage_args="A"
fi

if [ "$vg" -ne 0 ]; then
	memcheck=1
fi

bc_tests=$(gen_test_targets bc)
bc_script_tests=$(gen_script_test_targets bc)
dc_tests=$(gen_test_targets dc)
dc_script_tests=$(gen_script_test_targets dc)

# Print out the values; this is for debugging.
if [ "$bc" -ne 0 ]; then
	printf 'Building bc\n'
else
	printf 'Not building bc\n'
fi
if [ "$dc" -ne 0 ]; then
	printf 'Building dc\n'
else
	printf 'Not building dc\n'
fi
printf '\n'
printf 'BC_ENABLE_LIBRARY=%s\n\n' "$library"
printf 'BC_ENABLE_HISTORY=%s\n' "$hist"
printf 'BC_ENABLE_EXTRA_MATH=%s\n' "$extra_math"
printf 'BC_ENABLE_NLS=%s\n' "$nls"
printf 'BC_ENABLE_PROMPT=%s\n' "$prompt"
printf 'BC_ENABLE_AFL=%s\n' "$fuzz"
printf '\n'
printf 'BC_NUM_KARATSUBA_LEN=%s\n' "$karatsuba_len"
printf '\n'
printf 'CC=%s\n' "$CC"
printf 'CFLAGS=%s\n' "$CFLAGS"
printf 'HOSTCC=%s\n' "$HOSTCC"
printf 'HOSTCFLAGS=%s\n' "$HOSTCFLAGS"
printf 'CPPFLAGS=%s\n' "$CPPFLAGS"
printf 'LDFLAGS=%s\n' "$LDFLAGS"
printf 'PREFIX=%s\n' "$PREFIX"
printf 'BINDIR=%s\n' "$BINDIR"
printf 'INCLUDEDIR=%s\n' "$INCLUDEDIR"
printf 'LIBDIR=%s\n' "$LIBDIR"
printf 'DATAROOTDIR=%s\n' "$DATAROOTDIR"
printf 'DATADIR=%s\n' "$DATADIR"
printf 'MANDIR=%s\n' "$MANDIR"
printf 'MAN1DIR=%s\n' "$MAN1DIR"
printf 'MAN3DIR=%s\n' "$MAN3DIR"
printf 'NLSPATH=%s\n' "$NLSPATH"
printf 'EXECSUFFIX=%s\n' "$EXECSUFFIX"
printf 'EXECPREFIX=%s\n' "$EXECPREFIX"
printf 'DESTDIR=%s\n' "$DESTDIR"
printf 'LONG_BIT=%s\n' "$LONG_BIT"
printf 'GEN_HOST=%s\n' "$GEN_HOST"
printf 'GEN_EMU=%s\n' "$GEN_EMU"

contents=$(cat "$scriptdir/Makefile.in")

needle="WARNING"
replacement='*** WARNING: Autogenerated from Makefile.in. DO NOT MODIFY ***'

contents=$(replace "$contents" "$needle" "$replacement")

if [ "$unneeded" = "" ]; then
	unneeded="library.c"
fi

contents=$(gen_file_list "$contents" $unneeded)

SRC_TARGETS=""

src_files=$(find_src_files $unneeded)

temp_ifs="$IFS"
IFS=$'\n'

for f in $src_files; do
	o=$(replace_ext "$f" "c" "o")
	SRC_TARGETS=$(printf '%s\n\n%s: %s %s\n\t$(CC) $(CFLAGS) -o %s -c %s\n' \
		"$SRC_TARGETS" "$o" "$headers" "$f" "$o" "$f")
done

IFS="$temp_ifs"

contents=$(replace "$contents" "HEADERS" "$headers")

contents=$(replace "$contents" "BC_ENABLED" "$bc")
contents=$(replace "$contents" "DC_ENABLED" "$dc")

contents=$(replace "$contents" "BC_ALL_TESTS" "$bc_test")
contents=$(replace "$contents" "BC_TESTS" "$bc_tests")
contents=$(replace "$contents" "BC_SCRIPT_TESTS" "$bc_script_tests")
contents=$(replace "$contents" "BC_TEST_EXEC" "$bc_test_exec")
contents=$(replace "$contents" "TIMECONST_ALL_TESTS" "$timeconst")

contents=$(replace "$contents" "DC_ALL_TESTS" "$dc_test")
contents=$(replace "$contents" "DC_TESTS" "$dc_tests")
contents=$(replace "$contents" "DC_SCRIPT_TESTS" "$dc_script_tests")
contents=$(replace "$contents" "DC_TEST_EXEC" "$dc_test_exec")

contents=$(replace "$contents" "LIBRARY" "$library")
contents=$(replace "$contents" "HISTORY" "$hist")
contents=$(replace "$contents" "EXTRA_MATH" "$extra_math")
contents=$(replace "$contents" "NLS" "$nls")
contents=$(replace "$contents" "PROMPT" "$prompt")
contents=$(replace "$contents" "FUZZ" "$fuzz")
contents=$(replace "$contents" "MEMCHECK" "$memcheck")

contents=$(replace "$contents" "BC_LIB_O" "$bc_lib")
contents=$(replace "$contents" "BC_HELP_O" "$bc_help")
contents=$(replace "$contents" "DC_HELP_O" "$dc_help")
contents=$(replace "$contents" "BC_LIB2_O" "$BC_LIB2_O")
contents=$(replace "$contents" "KARATSUBA_LEN" "$karatsuba_len")

contents=$(replace "$contents" "NLSPATH" "$NLSPATH")
contents=$(replace "$contents" "DESTDIR" "$destdir")
contents=$(replace "$contents" "EXECSUFFIX" "$EXECSUFFIX")
contents=$(replace "$contents" "EXECPREFIX" "$EXECPREFIX")
contents=$(replace "$contents" "BINDIR" "$BINDIR")
contents=$(replace "$contents" "INCLUDEDIR" "$INCLUDEDIR")
contents=$(replace "$contents" "LIBDIR" "$LIBDIR")
contents=$(replace "$contents" "MAN1DIR" "$MAN1DIR")
contents=$(replace "$contents" "MAN3DIR" "$MAN3DIR")
contents=$(replace "$contents" "CFLAGS" "$CFLAGS")
contents=$(replace "$contents" "HOSTCFLAGS" "$HOSTCFLAGS")
contents=$(replace "$contents" "CPPFLAGS" "$CPPFLAGS")
contents=$(replace "$contents" "LDFLAGS" "$LDFLAGS")
contents=$(replace "$contents" "CC" "$CC")
contents=$(replace "$contents" "HOSTCC" "$HOSTCC")
contents=$(replace "$contents" "COVERAGE_OUTPUT" "$COVERAGE_OUTPUT")
contents=$(replace "$contents" "COVERAGE_PREREQS" "$COVERAGE_PREREQS")
contents=$(replace "$contents" "INSTALL_PREREQS" "$install_prereqs")
contents=$(replace "$contents" "INSTALL_MAN_PREREQS" "$install_man_prereqs")
contents=$(replace "$contents" "INSTALL_LOCALES" "$install_locales")
contents=$(replace "$contents" "INSTALL_LOCALES_PREREQS" "$install_locales_prereqs")
contents=$(replace "$contents" "UNINSTALL_MAN_PREREQS" "$uninstall_man_prereqs")
contents=$(replace "$contents" "UNINSTALL_PREREQS" "$uninstall_prereqs")
contents=$(replace "$contents" "UNINSTALL_LOCALES_PREREQS" "$uninstall_locales_prereqs")

contents=$(replace "$contents" "DEFAULT_TARGET" "$default_target")
contents=$(replace "$contents" "DEFAULT_TARGET_PREREQS" "$default_target_prereqs")
contents=$(replace "$contents" "DEFAULT_TARGET_CMD" "$default_target_cmd")
contents=$(replace "$contents" "SECOND_TARGET" "$second_target")
contents=$(replace "$contents" "SECOND_TARGET_PREREQS" "$second_target_prereqs")
contents=$(replace "$contents" "SECOND_TARGET_CMD" "$second_target_cmd")

contents=$(replace "$contents" "ALL_PREREQ" "$ALL_PREREQ")
contents=$(replace "$contents" "BC_EXEC_PREREQ" "$bc_exec_prereq")
contents=$(replace "$contents" "BC_EXEC_CMD" "$bc_exec_cmd")
contents=$(replace "$contents" "DC_EXEC_PREREQ" "$dc_exec_prereq")
contents=$(replace "$contents" "DC_EXEC_CMD" "$dc_exec_cmd")

contents=$(replace "$contents" "EXECUTABLES" "$executables")
contents=$(replace "$contents" "MAIN_EXEC" "$main_exec")
contents=$(replace "$contents" "EXEC" "$executable")
contents=$(replace "$contents" "TESTS" "$tests")

contents=$(replace "$contents" "BC_TEST" "$bc_test")
contents=$(replace "$contents" "DC_TEST" "$dc_test")

contents=$(replace "$contents" "VG_BC_TEST" "$vg_bc_test")
contents=$(replace "$contents" "VG_DC_TEST" "$vg_dc_test")

contents=$(replace "$contents" "TIMECONST" "$timeconst")

contents=$(replace "$contents" "KARATSUBA" "$karatsuba")
contents=$(replace "$contents" "KARATSUBA_TEST" "$karatsuba_test")

contents=$(replace "$contents" "LONG_BIT" "$LONG_BIT")
contents=$(replace "$contents" "LONG_BIT_DEFINE" "$LONG_BIT_DEFINE")

contents=$(replace "$contents" "GEN" "$GEN")
contents=$(replace "$contents" "GEN_EXEC_TARGET" "$GEN_EXEC_TARGET")
contents=$(replace "$contents" "CLEAN_PREREQS" "$CLEAN_PREREQS")
contents=$(replace "$contents" "GEN_EMU" "$GEN_EMU")

printf '%s\n%s\n\n' "$contents" "$SRC_TARGETS" > "$scriptdir/Makefile"

if [ "$bc" -ne 0 ]; then
	gen_tests bc BC "$extra_math" "$time_tests" $bc_test_exec
	gen_script_tests bc "$extra_math" "$generate_tests" "$time_tests" $bc_test_exec
fi

if [ "$dc" -ne 0 ]; then
	gen_tests dc DC "$extra_math" "$time_tests" $dc_test_exec
	gen_script_tests dc "$extra_math" "$generate_tests" "$time_tests" $dc_test_exec
fi

cd "$scriptdir"

cp -f manuals/bc/$manpage_args.1.md manuals/bc.1.md
cp -f manuals/bc/$manpage_args.1 manuals/bc.1
cp -f manuals/dc/$manpage_args.1.md manuals/dc.1.md
cp -f manuals/dc/$manpage_args.1 manuals/dc.1

make clean > /dev/null
