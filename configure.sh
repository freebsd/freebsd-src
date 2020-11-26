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

script="$0"
scriptdir=$(dirname "$script")
script=$(basename "$script")

. "$scriptdir/functions.sh"

usage() {

	if [ $# -gt 0 ]; then

		_usage_val=1

		printf "%s\n\n" "$1"

	else
		_usage_val=0
	fi

	printf 'usage: %s -h\n' "$script"
	printf '       %s --help\n' "$script"
	printf '       %s [-bD|-dB|-c] [-EfgGHlMNPT] [-O OPT_LEVEL] [-k KARATSUBA_LEN]\n' "$script"
	printf '       %s \\\n' "$script"
	printf '           [--bc-only --disable-dc|--dc-only --disable-bc|--coverage]      \\\n'
	printf '           [--debug --disable-extra-math --disable-generated-tests]        \\\n'
	printf '           [--disable-history --disable-man-pages --disable-nls]           \\\n'
	printf '           [--disable-prompt --disable-strip] [--install-all-locales]      \\\n'
	printf '           [--opt=OPT_LEVEL] [--karatsuba-len=KARATSUBA_LEN]               \\\n'
	printf '           [--prefix=PREFIX] [--bindir=BINDIR] [--datarootdir=DATAROOTDIR] \\\n'
	printf '           [--datadir=DATADIR] [--mandir=MANDIR] [--man1dir=MAN1DIR]       \\\n'
	printf '           [--force]                                                       \\\n'
	printf '\n'
	printf '    -b, --bc-only\n'
	printf '        Build bc only. It is an error if "-d", "--dc-only", "-B", or "--disable-bc"\n'
	printf '        are specified too.\n'
	printf '    -B, --disable-bc\n'
	printf '        Disable bc. It is an error if "-b", "--bc-only", "-D", or "--disable-dc"\n'
	printf '        are specified too.\n'
	printf '    -c, --coverage\n'
	printf '        Generate test coverage code. Requires gcov and regcovr.\n'
	printf '        It is an error if either "-b" ("-D") or "-d" ("-B") is specified.\n'
	printf '        Requires a compiler that use gcc-compatible coverage options\n'
	printf '    -d, --dc-only\n'
	printf '        Build dc only. It is an error if "-b", "--bc-only", "-D", or "--disable-dc"\n'
	printf '        are specified too.\n'
	printf '    -D, --disable-dc\n'
	printf '        Disable dc. It is an error if "-d", "--dc-only" "-B", or "--disable-bc"\n'
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
	printf '    -T, --disable-strip\n'
	printf '        Disable stripping symbols from the compiled binary or binaries.\n'
	printf '        Stripping symbols only happens when debug mode is off.\n'
	printf '    --prefix PREFIX\n'
	printf '        The prefix to install to. Overrides "$PREFIX" if it exists.\n'
	printf '        If PREFIX is "/usr", install path will be "/usr/bin".\n'
	printf '        Default is "/usr/local".\n'
	printf '    --bindir BINDIR\n'
	printf '        The directory to install binaries. Overrides "$BINDIR" if it exists.\n'
	printf '        Default is "$PREFIX/bin".\n'
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
	printf '    BINDIR       The directory to install binaries. Default is "$PREFIX/bin".\n'
	printf '    DATAROOTDIR  The root location for data files. Default is "$PREFIX/share".\n'
	printf '    DATADIR      The location for data files. Default is "$DATAROOTDIR".\n'
	printf '    MANDIR       The location to install manpages to. Default is "$DATADIR/man".\n'
	printf '    MAN1DIR      The location to install Section 1 manpages to. Default is\n'
	printf '                 "$MANDIR/man1".\n'
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

gen_file_lists() {

	if [ "$#" -lt 3 ]; then
		err_exit "Invalid number of args to $0"
	fi

	_gen_file_lists_contents="$1"
	shift

	_gen_file_lists_filedir="$1"
	shift

	_gen_file_lists_typ="$1"
	shift

	# If there is an extra argument, and it
	# is zero, we keep the file lists empty.
	if [ "$#" -gt 0 ]; then
		_gen_file_lists_use="$1"
	else
		_gen_file_lists_use="1"
	fi

	_gen_file_lists_needle_src="${_gen_file_lists_typ}SRC"
	_gen_file_lists_needle_obj="${_gen_file_lists_typ}OBJ"
	_gen_file_lists_needle_gcda="${_gen_file_lists_typ}GCDA"
	_gen_file_lists_needle_gcno="${_gen_file_lists_typ}GCNO"

	if [ "$_gen_file_lists_use" -ne 0 ]; then

		_gen_file_lists_replacement=$(cd "$_gen_file_lists_filedir" && find . ! -name . -prune -name "*.c" | cut -d/ -f2 | sed "s@^@$_gen_file_lists_filedir/@g" | tr '\n' ' ')
		_gen_file_lists_contents=$(replace "$_gen_file_lists_contents" "$_gen_file_lists_needle_src" "$_gen_file_lists_replacement")

		_gen_file_lists_replacement=$(replace_exts "$_gen_file_lists_replacement" "c" "o")
		_gen_file_lists_contents=$(replace "$_gen_file_lists_contents" "$_gen_file_lists_needle_obj" "$_gen_file_lists_replacement")

		_gen_file_lists_replacement=$(replace_exts "$_gen_file_lists_replacement" "o" "gcda")
		_gen_file_lists_contents=$(replace "$_gen_file_lists_contents" "$_gen_file_lists_needle_gcda" "$_gen_file_lists_replacement")

		_gen_file_lists_replacement=$(replace_exts "$_gen_file_lists_replacement" "gcda" "gcno")
		_gen_file_lists_contents=$(replace "$_gen_file_lists_contents" "$_gen_file_lists_needle_gcno" "$_gen_file_lists_replacement")

	else
		_gen_file_lists_contents=$(replace "$_gen_file_lists_contents" "$_gen_file_lists_needle_src" "")
		_gen_file_lists_contents=$(replace "$_gen_file_lists_contents" "$_gen_file_lists_needle_obj" "")
		_gen_file_lists_contents=$(replace "$_gen_file_lists_contents" "$_gen_file_lists_needle_gcda" "")
		_gen_file_lists_contents=$(replace "$_gen_file_lists_contents" "$_gen_file_lists_needle_gcno" "")
	fi

	printf '%s\n' "$_gen_file_lists_contents"
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

while getopts "bBcdDEfgGhHk:lMNO:PST-" opt; do

	case "$opt" in
		b) bc_only=1 ;;
		B) dc_only=1 ;;
		c) coverage=1 ;;
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
		M) install_manpages=0 ;;
		N) nls=0 ;;
		O) optimization="$OPTARG" ;;
		P) prompt=0 ;;
		T) strip_bin=0 ;;
		-)
			arg="$1"
			arg="${arg#--}"
			LONG_OPTARG="${arg#*=}"
			case $arg in
				help) usage ;;
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
				disable-extra-math) extra_math=0 ;;
				disable-generated-tests) generate_tests=0 ;;
				disable-history) hist=0 ;;
				disable-man-pages) install_manpages=0 ;;
				disable-nls) nls=0 ;;
				disable-prompt) prompt=0 ;;
				disable-strip) strip_bin=0 ;;
				install-all-locales) all_locales=1 ;;
				help* | bc-only* | dc-only* | coverage* | debug*)
					usage "No arg allowed for --$arg option" ;;
				disable-bc* | disable-dc* | disable-extra-math*)
					usage "No arg allowed for --$arg option" ;;
				disable-generated-tests* | disable-history*)
					usage "No arg allowed for --$arg option" ;;
				disable-man-pages* | disable-nls* | disable-strip*)
					usage "No arg allowed for --$arg option" ;;
				install-all-locales*)
					usage "No arg allowed for --$arg option" ;;
				'') break ;; # "--" terminates argument processing
				* ) usage "Invalid option $LONG_OPTARG" ;;
			esac
			shift
			OPTIND=1 ;;
		?) usage "Invalid option $opt" ;;
	esac

done

if [ "$bc_only" -eq 1 ] && [ "$dc_only" -eq 1 ]; then
	usage "Can only specify one of -b(-D) or -d(-B)"
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

bc_test="@tests/all.sh bc $extra_math 1 $generate_tests 0 \$(BC_EXEC)"
bc_time_test="@tests/all.sh bc $extra_math 1 $generate_tests 1 \$(BC_EXEC)"

dc_test="@tests/all.sh dc $extra_math 1 $generate_tests 0 \$(DC_EXEC)"
dc_time_test="@tests/all.sh dc $extra_math 1 $generate_tests 1 \$(DC_EXEC)"

timeconst="@tests/bc/timeconst.sh tests/bc/scripts/timeconst.bc \$(BC_EXEC)"

# In order to have cleanup at exit, we need to be in
# debug mode, so don't run valgrind without that.
if [ "$debug" -ne 0 ]; then
	vg_bc_test="@tests/all.sh bc $extra_math 1 $generate_tests 0 valgrind \$(VALGRIND_ARGS) \$(BC_EXEC)"
	vg_dc_test="@tests/all.sh dc $extra_math 1 $generate_tests 0 valgrind \$(VALGRIND_ARGS) \$(DC_EXEC)"
else
	vg_bc_test="@printf 'Cannot run valgrind without debug flags\\\\n'"
	vg_dc_test="@printf 'Cannot run valgrind without debug flags\\\\n'"
fi

karatsuba="@printf 'karatsuba cannot be run because one of bc or dc is not built\\\\n'"
karatsuba_test="@printf 'karatsuba cannot be run because one of bc or dc is not built\\\\n'"

bc_lib="\$(GEN_DIR)/lib.o"
bc_help="\$(GEN_DIR)/bc_help.o"
dc_help="\$(GEN_DIR)/dc_help.o"

if [ "$bc_only" -eq 1 ]; then

	bc=1
	dc=0

	dc_help=""

	executables="bc"

	dc_test="@printf 'No dc tests to run\\\\n'"
	dc_time_test="@printf 'No dc tests to run\\\\n'"
	vg_dc_test="@printf 'No dc tests to run\\\\n'"

	install_prereqs=" install_bc_manpage"
	uninstall_prereqs=" uninstall_bc"
	uninstall_man_prereqs=" uninstall_bc_manpage"

elif [ "$dc_only" -eq 1 ]; then

	bc=0
	dc=1

	bc_lib=""
	bc_help=""

	executables="dc"

	main_exec="DC"
	executable="DC_EXEC"

	bc_test="@printf 'No bc tests to run\\\\n'"
	bc_time_test="@printf 'No bc tests to run\\\\n'"
	vg_bc_test="@printf 'No bc tests to run\\\\n'"

	timeconst="@printf 'timeconst cannot be run because bc is not built\\\\n'"

	install_prereqs=" install_dc_manpage"
	uninstall_prereqs=" uninstall_dc"
	uninstall_man_prereqs=" uninstall_dc_manpage"

else

	bc=1
	dc=1

	executables="bc and dc"

	link="\$(LINK) \$(BIN) \$(EXEC_PREFIX)\$(DC)"

	karatsuba="@\$(KARATSUBA) 30 0 \$(BC_EXEC)"
	karatsuba_test="@\$(KARATSUBA) 1 100 \$(BC_EXEC)"

	install_prereqs=" install_bc_manpage install_dc_manpage"
	uninstall_prereqs=" uninstall_bc uninstall_dc"
	uninstall_man_prereqs=" uninstall_bc_manpage uninstall_dc_manpage"

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

else
	install_prereqs=""
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
	flags="$flags -DBC_ENABLE_NLS=$nls"
	flags="$flags -DBC_ENABLE_EXTRA_MATH=$extra_math -I./include/"
	flags="$flags -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700"

	"$CC" $CPPFLAGS $CFLAGS $flags -c "src/history/history.c" -o "$scriptdir/history.o" > /dev/null 2>&1

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

if [ "$extra_math" -eq 1 ] && [ "$bc" -ne 0 ]; then
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

if [ "$extra_math" -eq 0 ]; then
	manpage_args="E"
fi

if [ "$hist" -eq 0 ]; then
	manpage_args="${manpage_args}H"
fi

if [ "$nls" -eq 0 ]; then
	manpage_args="${manpage_args}N"
fi

if [ "$prompt" -eq 0 ]; then
	manpage_args="${manpage_args}P"
fi

if [ "$manpage_args" = "" ]; then
	manpage_args="A"
fi

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
printf 'BC_ENABLE_HISTORY=%s\n' "$hist"
printf 'BC_ENABLE_EXTRA_MATH=%s\n' "$extra_math"
printf 'BC_ENABLE_NLS=%s\n' "$nls"
printf 'BC_ENABLE_PROMPT=%s\n' "$prompt"
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
printf 'DATAROOTDIR=%s\n' "$DATAROOTDIR"
printf 'DATADIR=%s\n' "$DATADIR"
printf 'MANDIR=%s\n' "$MANDIR"
printf 'MAN1DIR=%s\n' "$MAN1DIR"
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

contents=$(gen_file_lists "$contents" "$scriptdir/src" "")
contents=$(gen_file_lists "$contents" "$scriptdir/src/bc" "BC_" "$bc")
contents=$(gen_file_lists "$contents" "$scriptdir/src/dc" "DC_" "$dc")
contents=$(gen_file_lists "$contents" "$scriptdir/src/history" "HISTORY_" "$hist")
contents=$(gen_file_lists "$contents" "$scriptdir/src/rand" "RAND_" "$extra_math")

contents=$(replace "$contents" "BC_ENABLED" "$bc")
contents=$(replace "$contents" "DC_ENABLED" "$dc")
contents=$(replace "$contents" "LINK" "$link")

contents=$(replace "$contents" "HISTORY" "$hist")
contents=$(replace "$contents" "EXTRA_MATH" "$extra_math")
contents=$(replace "$contents" "NLS" "$nls")
contents=$(replace "$contents" "PROMPT" "$prompt")
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
contents=$(replace "$contents" "MAN1DIR" "$MAN1DIR")
contents=$(replace "$contents" "CFLAGS" "$CFLAGS")
contents=$(replace "$contents" "HOSTCFLAGS" "$HOSTCFLAGS")
contents=$(replace "$contents" "CPPFLAGS" "$CPPFLAGS")
contents=$(replace "$contents" "LDFLAGS" "$LDFLAGS")
contents=$(replace "$contents" "CC" "$CC")
contents=$(replace "$contents" "HOSTCC" "$HOSTCC")
contents=$(replace "$contents" "COVERAGE_OUTPUT" "$COVERAGE_OUTPUT")
contents=$(replace "$contents" "COVERAGE_PREREQS" "$COVERAGE_PREREQS")
contents=$(replace "$contents" "INSTALL_PREREQS" "$install_prereqs")
contents=$(replace "$contents" "INSTALL_LOCALES" "$install_locales")
contents=$(replace "$contents" "INSTALL_LOCALES_PREREQS" "$install_locales_prereqs")
contents=$(replace "$contents" "UNINSTALL_MAN_PREREQS" "$uninstall_man_prereqs")
contents=$(replace "$contents" "UNINSTALL_PREREQS" "$uninstall_prereqs")
contents=$(replace "$contents" "UNINSTALL_LOCALES_PREREQS" "$uninstall_locales_prereqs")

contents=$(replace "$contents" "EXECUTABLES" "$executables")
contents=$(replace "$contents" "MAIN_EXEC" "$main_exec")
contents=$(replace "$contents" "EXEC" "$executable")

contents=$(replace "$contents" "BC_TEST" "$bc_test")
contents=$(replace "$contents" "BC_TIME_TEST" "$bc_time_test")

contents=$(replace "$contents" "DC_TEST" "$dc_test")
contents=$(replace "$contents" "DC_TIME_TEST" "$dc_time_test")

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

printf '%s\n' "$contents" > "$scriptdir/Makefile"

cd "$scriptdir"

cp -f manuals/bc/$manpage_args.1.md manuals/bc.1.md
cp -f manuals/bc/$manpage_args.1 manuals/bc.1
cp -f manuals/dc/$manpage_args.1.md manuals/dc.1.md
cp -f manuals/dc/$manpage_args.1 manuals/dc.1

make clean > /dev/null
