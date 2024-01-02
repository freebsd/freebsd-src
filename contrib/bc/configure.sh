#! /bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2023 Gavin D. Howard and contributors.
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

builddir=$(pwd)

. "$scriptdir/scripts/functions.sh"

# Simply prints the help message and quits based on the argument.
# @param msg  The help message to print.
usage() {

	if [ $# -gt 0 ]; then

		_usage_val=1

		printf '%s\n\n' "$1"

	else
		_usage_val=0
	fi

	printf 'usage:\n'
	printf '    %s -h\n' "$script"
	printf '    %s --help\n' "$script"
	printf '    %s [-a|-bD|-dB|-c] [-CeEfgGHilmMNPrtTvz] [-O OPT_LEVEL] [-k KARATSUBA_LEN]\\\n' "$script"
	printf '       [-s SETTING] [-S SETTING] [-p TYPE]\n'
	printf '    %s \\\n' "$script"
	printf '       [--library|--bc-only --disable-dc|--dc-only --disable-bc|--coverage]  \\\n'
	printf '       [--force --debug --disable-extra-math --disable-generated-tests]      \\\n'
	printf '       [--disable-history --disable-man-pages --disable-nls --disable-strip] \\\n'
	printf '       [--enable-editline] [--enable-readline] [--enable-internal-history]   \\\n'
	printf '       [--disable-problematic-tests] [--install-all-locales]                 \\\n'
	printf '       [--opt=OPT_LEVEL] [--karatsuba-len=KARATSUBA_LEN]                     \\\n'
	printf '       [--set-default-on=SETTING] [--set-default-off=SETTING]                \\\n'
	printf '       [--predefined-build-type=TYPE]                                        \\\n'
	printf '       [--prefix=PREFIX] [--bindir=BINDIR] [--datarootdir=DATAROOTDIR]       \\\n'
	printf '       [--datadir=DATADIR] [--mandir=MANDIR] [--man1dir=MAN1DIR]             \\\n'
	printf '       [--man3dir=MAN3DIR]\n'

	if [ "$_usage_val" -ne 0 ]; then
		exit
	fi

	printf '\n'
	printf '    -a, --library\n'
	printf '        Build the libbcl instead of the programs. This is meant to be used with\n'
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
	printf '        Generate test coverage code. Requires gcov and gcovr.\n'
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
	printf '    -e, --enable-editline\n'
	printf '        Enable the use of libedit/editline. This is meant for those users that\n'
	printf '        want vi-like or Emacs-like behavior in history. This option is ignored\n'
	printf '        if history is disabled. If the -r or -i options are given with this\n'
	printf '        option, the last occurrence of all of the three is used.\n'
	printf '    -E, --disable-extra-math\n'
	printf '        Disable extra math. This includes: "$" operator (truncate to integer),\n'
	printf '        "@" operator (set number of decimal places), and r(x, p) (rounding\n'
	printf '        function). Additionally, this option disables the extra printing\n'
	printf '        functions in the math library.\n'
	printf '    -f, --force\n'
	printf '        Force use of all enabled options, even if they do not work. This\n'
	printf '        option is to allow the maintainer a way to test that certain options\n'
	printf '        are not failing invisibly. (Development only.)\n'
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
	printf '    -i, --enable-internal-history\n'
	printf '        Enable the internal history implementation and do not depend on either\n'
	printf '        editline or readline. This option is ignored if history is disabled.\n'
	printf '        If this option is given along with -e and -r, the last occurrence of\n'
	printf '        all of the three is used.\n'
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
	printf '        ***WARNING***: Locales ignore the prefix because they *must* be\n'
	printf '        installed at a fixed location to work at all. If you do not want that\n'
	printf '        to happen, you must disable locales (NLS) completely.\n'
	printf '    -O OPT_LEVEL, --opt OPT_LEVEL\n'
	printf '        Set the optimization level. This can also be included in the CFLAGS,\n'
	printf '        but it is provided, so maintainers can build optimized debug builds.\n'
	printf '        This is passed through to the compiler, so it must be supported.\n'
	printf '    -p TYPE, --predefined-build-type=TYPE\n'
	printf '        Sets a given predefined build type with specific defaults. This is for\n'
	printf '        easy setting of predefined builds. For example, to get a build that\n'
	printf '        acts like the GNU bc by default, TYPE should be "GNU" (without the\n'
	printf '        quotes) This option *must* come before any others that might change the\n'
	printf '        build options. Currently supported values for TYPE include: "BSD" (for\n'
	printf '        matching the BSD bc and BSD dc), "GNU" (for matching the GNU bc and\n'
	printf '        dc), "GDH" (for the preferred build of the author, Gavin D. Howard),\n'
	printf '        and "DBG" (for the preferred debug build of the author). This will\n'
	printf '        also automatically enable a release build (except for "DBG").\n'
	printf '    -P, --disable-problematic-tests\n'
	printf '        Disables problematic tests. These tests usually include tests that\n'
	printf '        can cause a SIGKILL because of too much memory usage.\n'
	printf '    -r, --enable-readline\n'
	printf '        Enable the use of libreadline/readline. This is meant for those users\n'
	printf '        that want vi-like or Emacs-like behavior in history. This option is\n'
	printf '        ignored if history is disabled. If this option is given along with -e\n'
	printf '        and -r, the last occurrence of all of the three is used.\n'
	printf '    -s SETTING, --set-default-on SETTING\n'
	printf '        Set the default named by SETTING to on. See below for possible values\n'
	printf '        for SETTING. For multiple instances of the -s or -S for the the same\n'
	printf '        setting, the last one is used.\n'
	printf '    -S SETTING, --set-default-off SETTING\n'
	printf '        Set the default named by SETTING to off. See below for possible values\n'
	printf '        for SETTING. For multiple instances of the -s or -S for the the same\n'
	printf '        setting, the last one is used.\n'
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
	printf '        ***WARNING***: Locales ignore the prefix because they *must* be\n'
	printf '        installed at a fixed location to work at all. If you do not want that to\n'
	printf '        happen, you must disable locales (NLS) completely.\n'
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
	printf '                 ***WARNING***: Locales ignore the prefix because they *must* be\n'
	printf '                 installed at a fixed location to work at all. If you do not\n'
	printf '                 want that to happen, you must disable locales (NLS) completely.\n'
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
	printf '    PC_PATH      The location to install pkg-config files to. Must be an\n'
	printf '                 path or contain one. Default is the first path given by the\n'
	printf '                 output of `pkg-config --variable=pc_path pkg-config`.\n'
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
	printf '                 `gen/lib2.bc` is over 4095 characters, the max supported length\n'
	printf '                 of a string literal in C99, and `gen/strgen.sh` generates a\n'
	printf '                 string literal instead of an array, as `gen/strgen.c` does. For\n'
	printf '                 most production-ready compilers, this limit probably is not\n'
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
	printf '\n'
	printf 'Settings\n'
	printf '========\n'
	printf '\n'
	printf 'bc and dc have some settings that, while they cannot be removed by build time\n'
	printf 'options, can have their defaults changed at build time by packagers. Users are\n'
	printf 'also able to change each setting with environment variables.\n'
	printf '\n'
	printf 'The following is a table of settings, along with their default values and the\n'
	printf 'environment variables users can use to change them. (For the defaults, non-zero\n'
	printf 'means on, and zero means off.)\n'
	printf '\n'
	printf '| Setting         | Description          | Default      | Env Variable         |\n'
	printf '| =============== | ==================== | ============ | ==================== |\n'
	printf '| bc.banner       | Whether to display   |            0 | BC_BANNER            |\n'
	printf '|                 | the bc version       |              |                      |\n'
	printf '|                 | banner when in       |              |                      |\n'
	printf '|                 | interactive mode.    |              |                      |\n'
	printf '| --------------- | -------------------- | ------------ | -------------------- |\n'
	printf '| bc.sigint_reset | Whether SIGINT will  |            1 | BC_SIGINT_RESET      |\n'
	printf '|                 | reset bc, instead of |              |                      |\n'
	printf '|                 | exiting, when in     |              |                      |\n'
	printf '|                 | interactive mode.    |              |                      |\n'
	printf '| --------------- | -------------------- | ------------ | -------------------- |\n'
	printf '| dc.sigint_reset | Whether SIGINT will  |            1 | DC_SIGINT_RESET      |\n'
	printf '|                 | reset dc, instead of |              |                      |\n'
	printf '|                 | exiting, when in     |              |                      |\n'
	printf '|                 | interactive mode.    |              |                      |\n'
	printf '| --------------- | -------------------- | ------------ | -------------------- |\n'
	printf '| bc.tty_mode     | Whether TTY mode for |            1 | BC_TTY_MODE          |\n'
	printf '|                 | bc should be on when |              |                      |\n'
	printf '|                 | available.           |              |                      |\n'
	printf '| --------------- | -------------------- | ------------ | -------------------- |\n'
	printf '| dc.tty_mode     | Whether TTY mode for |            0 | BC_TTY_MODE          |\n'
	printf '|                 | dc should be on when |              |                      |\n'
	printf '|                 | available.           |              |                      |\n'
	printf '| --------------- | -------------------- | ------------ | -------------------- |\n'
	printf '| bc.prompt       | Whether the prompt   | $BC_TTY_MODE | BC_PROMPT            |\n'
	printf '|                 | for bc should be on  |              |                      |\n'
	printf '|                 | in tty mode.         |              |                      |\n'
	printf '| --------------- | -------------------- | ------------ | -------------------- |\n'
	printf '| dc.prompt       | Whether the prompt   | $DC_TTY_MODE | DC_PROMPT            |\n'
	printf '|                 | for dc should be on  |              |                      |\n'
	printf '|                 | in tty mode.         |              |                      |\n'
	printf '| --------------- | -------------------- | ------------ | -------------------- |\n'
	printf '| bc.expr_exit    | Whether to exit bc   |            1 | BC_EXPR_EXIT         |\n'
	printf '|                 | if an expression or  |              |                      |\n'
	printf '|                 | expression file is   |              |                      |\n'
	printf '|                 | given with the -e or |              |                      |\n'
	printf '|                 | -f options.          |              |                      |\n'
	printf '| --------------- | -------------------- | ------------ | -------------------- |\n'
	printf '| dc.expr_exit    | Whether to exit dc   |            1 | DC_EXPR_EXIT         |\n'
	printf '|                 | if an expression or  |              |                      |\n'
	printf '|                 | expression file is   |              |                      |\n'
	printf '|                 | given with the -e or |              |                      |\n'
	printf '|                 | -f options.          |              |                      |\n'
	printf '| --------------- | -------------------- | ------------ | -------------------- |\n'
	printf '| bc.digit_clamp  | Whether to have bc   |            0 | BC_DIGIT_CLAMP       |\n'
	printf '|                 | clamp digits that    |              |                      |\n'
	printf '|                 | are greater than or  |              |                      |\n'
	printf '|                 | equal to the current |              |                      |\n'
	printf '|                 | ibase when parsing   |              |                      |\n'
	printf '|                 | numbers.             |              |                      |\n'
	printf '| --------------- | -------------------- | ------------ | -------------------- |\n'
	printf '| dc.digit_clamp  | Whether to have dc   |            0 | DC_DIGIT_CLAMP       |\n'
	printf '|                 | clamp digits that    |              |                      |\n'
	printf '|                 | are greater than or  |              |                      |\n'
	printf '|                 | equal to the current |              |                      |\n'
	printf '|                 | ibase when parsing   |              |                      |\n'
	printf '|                 | numbers.             |              |                      |\n'
	printf '| --------------- | -------------------- | ------------ | -------------------- |\n'
	printf '\n'
	printf 'These settings are not meant to be changed on a whim. They are meant to ensure\n'
	printf 'that this bc and dc will conform to the expectations of the user on each\n'
	printf 'platform.\n'

	exit "$_usage_val"
}

# Replaces a file extension in a filename. This is used mostly to turn filenames
# like `src/num.c` into `src/num.o`. In other words, it helps to link targets to
# the files they depend on.
#
# @param file  The filename.
# @param ext1  The extension to replace.
# @param ext2  The new extension.
replace_ext() {

	if [ "$#" -ne 3 ]; then
		err_exit "Invalid number of args to $0"
	fi

	_replace_ext_file="$1"
	_replace_ext_ext1="$2"
	_replace_ext_ext2="$3"

	_replace_ext_result="${_replace_ext_file%.$_replace_ext_ext1}.$_replace_ext_ext2"

	printf '%s\n' "$_replace_ext_result"
}

# Replaces a file extension in every filename given in a list. The list is just
# a space-separated list of words, so filenames are expected to *not* have
# spaces in them. See the documentation for `replace_ext()`.
#
# @param files  The list of space-separated filenames to replace extensions for.
# @param ext1   The extension to replace.
# @param ext2   The new extension.
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

# Finds a placeholder in @a str and replaces it. This is the workhorse of
# configure.sh. It's what replaces placeholders in Makefile.in with the data
# needed for the chosen build. Below, you will see a lot of calls to this
# function.
#
# Note that needle can never contain an exclamation point. For more information,
# see substring_replace() in scripts/functions.sh.
#
# @param str          The string to find and replace placeholders in.
# @param needle       The placeholder name.
# @param replacement  The string to use to replace the placeholder.
replace() {

	if [ "$#" -ne 3 ]; then
		err_exit "Invalid number of args to $0"
	fi

	_replace_str="$1"
	_replace_needle="$2"
	_replace_replacement="$3"

	substring_replace "$_replace_str" "%%$_replace_needle%%" "$_replace_replacement"
}

# This function finds all the source files that need to be built. If there is
# only one argument and it is empty, then all source files are built. Otherwise,
# the arguments are all assumed to be source files that should *not* be built.
find_src_files() {

	_find_src_files_args=""

	if [ "$#" -ge 1 ] && [ "$1" != "" ]; then

		while [ "$#" -ge 1 ]; do
			_find_src_files_a="${1## }"
			shift
			_find_src_files_args=$(printf '%s\n%s/src/%s\n' "$_find_src_files_args" "$scriptdir" "${_find_src_files_a}")
		done

	fi

	_find_src_files_files=$(find "$scriptdir/src" -depth -name "*.c" -print | LC_ALL=C sort)

	_find_src_files_result=""

	for _find_src_files_f in $_find_src_files_files; do

		# If this is true, the file is part of args, and therefore, unneeded.
		if [ "${_find_src_files_args##*$_find_src_files_f}" != "${_find_src_files_args}" ]; then
			continue
		fi

		_find_src_files_result=$(printf '%s\n%s\n' "$_find_src_files_result" "$_find_src_files_f")

	done

	printf '%s\n' "$_find_src_files_result"
}

# This function generates a list of files to go into the Makefile. It generates
# the list of object files, as well as the list of test coverage files.
#
# @param contents  The contents of the Makefile template to put the list of
#                  files into.
gen_file_list() {

	if [ "$#" -lt 1 ]; then
		err_exit "Invalid number of args to $0"
	fi

	_gen_file_list_contents="$1"
	shift

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

	_gen_file_list_cbases=""

	for _gen_file_list_f in $_gen_file_list_replacement; do
		_gen_file_list_b=$(basename "$_gen_file_list_f")
		_gen_file_list_cbases="$_gen_file_list_cbases src/$_gen_file_list_b"
	done

	_gen_file_list_replacement=$(replace_exts "$_gen_file_list_cbases" "c" "o")
	_gen_file_list_contents=$(replace "$_gen_file_list_contents" \
		"$_gen_file_list_needle_obj" "$_gen_file_list_replacement")

	_gen_file_list_replacement=$(replace_exts "$_gen_file_list_replacement" "o" "gcda")
	_gen_file_list_contents=$(replace "$_gen_file_list_contents" \
		"$_gen_file_list_needle_gcda" "$_gen_file_list_replacement")

	_gen_file_list_replacement=$(replace_exts "$_gen_file_list_replacement" "gcda" "gcno")
	_gen_file_list_contents=$(replace "$_gen_file_list_contents" \
		"$_gen_file_list_needle_gcno" "$_gen_file_list_replacement")

	printf '%s\n' "$_gen_file_list_contents"
}

# Generates the proper test targets for each test to have its own target. This
# allows `make test` to run in parallel.
#
# @param name        Which calculator to generate tests for.
# @param extra_math  An integer that, if non-zero, activates extra math tests.
# @param time_tests  An integer that, if non-zero, tells the test suite to time
#                    the execution of each test.
gen_std_tests() {

	_gen_std_tests_name="$1"
	shift

	_gen_std_tests_extra_math="$1"
	shift

	_gen_std_tests_time_tests="$1"
	shift

	_gen_std_tests_extra_required=$(cat "$scriptdir/tests/extra_required.txt")

	for _gen_std_tests_t in $(cat "$scriptdir/tests/$_gen_std_tests_name/all.txt"); do

		if [ "$_gen_std_tests_extra_math" -eq 0 ]; then

			if [ -z "${_gen_std_tests_extra_required##*$_gen_std_tests_t*}" ]; then
				printf 'test_%s_%s:\n\t@printf "Skipping %s %s\\n"\n\n' \
					"$_gen_std_tests_name" "$_gen_std_tests_t" "$_gen_std_tests_name" \
					"$_gen_std_tests_t" >> "Makefile"
				continue
			fi

		fi

		printf 'test_%s_%s:\n\t@export BC_TEST_OUTPUT_DIR="%s/tests"; sh $(TESTSDIR)/test.sh %s %s %s %s %s\n\n' \
			"$_gen_std_tests_name" "$_gen_std_tests_t" "$builddir" "$_gen_std_tests_name" \
			"$_gen_std_tests_t" "$generate_tests" "$time_tests" \
			"$*" >> "Makefile"

	done
}

# Generates a list of test targets that will be used as prerequisites for other
# targets.
#
# @param name  The name of the calculator to generate test targets for.
gen_std_test_targets() {

	_gen_std_test_targets_name="$1"
	shift

	_gen_std_test_targets_tests=$(cat "$scriptdir/tests/${_gen_std_test_targets_name}/all.txt")

	for _gen_std_test_targets_t in $_gen_std_test_targets_tests; do
		printf ' test_%s_%s' "$_gen_std_test_targets_name" "$_gen_std_test_targets_t"
	done

	printf '\n'
}

# Generates the proper test targets for each error test to have its own target.
# This allows `make test_bc_errors` and `make test_dc_errors` to run in
# parallel.
#
# @param name  Which calculator to generate tests for.
gen_err_tests() {

	_gen_err_tests_name="$1"
	shift

	_gen_err_tests_fs=$(ls "$scriptdir/tests/$_gen_err_tests_name/errors/")

	for _gen_err_tests_t in $_gen_err_tests_fs; do

		printf 'test_%s_error_%s:\n\t@export BC_TEST_OUTPUT_DIR="%s/tests"; sh $(TESTSDIR)/error.sh %s %s %s %s\n\n' \
			"$_gen_err_tests_name" "$_gen_err_tests_t" "$builddir" "$_gen_err_tests_name" \
			"$_gen_err_tests_t" "$problematic_tests" "$*" >> "Makefile"

	done

}

# Generates a list of error test targets that will be used as prerequisites for
# other targets.
#
# @param name  The name of the calculator to generate test targets for.
gen_err_test_targets() {

	_gen_err_test_targets_name="$1"
	shift

	_gen_err_test_targets_tests=$(ls "$scriptdir/tests/$_gen_err_test_targets_name/errors/")

	for _gen_err_test_targets_t in $_gen_err_test_targets_tests; do
		printf ' test_%s_error_%s' "$_gen_err_test_targets_name" "$_gen_err_test_targets_t"
	done

	printf '\n'
}

# Generates the proper script test targets for each script test to have its own
# target. This allows `make test` to run in parallel.
#
# @param name        Which calculator to generate tests for.
# @param extra_math  An integer that, if non-zero, activates extra math tests.
# @param generate    An integer that, if non-zero, activates generated tests.
# @param time_tests  An integer that, if non-zero, tells the test suite to time
#                    the execution of each test.
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

		printf 'test_%s_script_%s:\n\t@export BC_TEST_OUTPUT_DIR="%s/tests"; sh $(TESTSDIR)/script.sh %s %s %s 1 %s %s %s\n\n' \
			"$_gen_script_tests_name" "$_gen_script_tests_b" "$builddir" "$_gen_script_tests_name" \
			"$_gen_script_tests_f" "$_gen_script_tests_extra_math" "$_gen_script_tests_generate" \
			"$_gen_script_tests_time" "$*" >> "Makefile"
	done
}

set_default() {

	_set_default_on="$1"
	shift

	_set_default_name="$1"
	shift

	# The reason that the variables that are being set do not have the same
	# non-collision avoidance that the other variables do is that we *do* want
	# the settings of these variables to leak out of the function. They adjust
	# the settings outside of the function.
	case "$_set_default_name" in

		bc.banner) bc_default_banner="$_set_default_on" ;;
		bc.sigint_reset) bc_default_sigint_reset="$_set_default_on" ;;
		dc.sigint_reset) dc_default_sigint_reset="$_set_default_on" ;;
		bc.tty_mode) bc_default_tty_mode="$_set_default_on" ;;
		dc.tty_mode) dc_default_tty_mode="$_set_default_on" ;;
		bc.prompt) bc_default_prompt="$_set_default_on" ;;
		dc.prompt) dc_default_prompt="$_set_default_on" ;;
		bc.expr_exit) bc_default_expr_exit="$_set_default_on";;
		dc.expr_exit) dc_default_expr_exit="$_set_default_on";;
		bc.digit_clamp) bc_default_digit_clamp="$_set_default_on";;
		dc.digit_clamp) dc_default_digit_clamp="$_set_default_on";;
		?) usage "Invalid setting: $_set_default_name" ;;

	esac
}

predefined_build() {

	_predefined_build_type="$1"
	shift

	# The reason that the variables that are being set do not have the same
	# non-collision avoidance that the other variables do is that we *do* want
	# the settings of these variables to leak out of the function. They adjust
	# the settings outside of the function.
	case "$_predefined_build_type" in

		BSD)
			bc_only=0
			dc_only=0
			coverage=0
			debug=0
			optimization="3"
			hist=1
			hist_impl="editline"
			extra_math=1
			generate_tests=$generate_tests
			install_manpages=0
			nls=1
			force=0
			strip_bin=1
			all_locales=0
			library=0
			fuzz=0
			time_tests=0
			vg=0
			memcheck=0
			clean=1
			bc_default_banner=0
			bc_default_sigint_reset=1
			dc_default_sigint_reset=1
			bc_default_tty_mode=1
			dc_default_tty_mode=0
			bc_default_prompt=""
			dc_default_prompt=""
			bc_default_expr_exit=1
			dc_default_expr_exit=1
			bc_default_digit_clamp=0
			dc_default_digit_clamp=0;;

		GNU)
			bc_only=0
			dc_only=0
			coverage=0
			debug=0
			optimization="3"
			hist=1
			hist_impl="internal"
			extra_math=1
			generate_tests=$generate_tests
			install_manpages=1
			nls=1
			force=0
			strip_bin=1
			all_locales=0
			library=0
			fuzz=0
			time_tests=0
			vg=0
			memcheck=0
			clean=1
			bc_default_banner=1
			bc_default_sigint_reset=1
			dc_default_sigint_reset=0
			bc_default_tty_mode=1
			dc_default_tty_mode=0
			bc_default_prompt=""
			dc_default_prompt=""
			bc_default_expr_exit=1
			dc_default_expr_exit=1
			bc_default_digit_clamp=1
			dc_default_digit_clamp=0;;

		GDH)
			CFLAGS="-flto -Weverything -Wno-padded -Wno-unsafe-buffer-usage -Wno-poison-system-directories -Werror -pedantic -std=c11"
			bc_only=0
			dc_only=0
			coverage=0
			debug=0
			optimization="3"
			hist=1
			hist_impl="internal"
			extra_math=1
			generate_tests=1
			install_manpages=1
			nls=0
			force=0
			strip_bin=1
			all_locales=0
			library=0
			fuzz=0
			time_tests=0
			vg=0
			memcheck=0
			clean=1
			bc_default_banner=1
			bc_default_sigint_reset=1
			dc_default_sigint_reset=1
			bc_default_tty_mode=1
			dc_default_tty_mode=1
			bc_default_prompt=""
			dc_default_prompt=""
			bc_default_expr_exit=0
			dc_default_expr_exit=0
			bc_default_digit_clamp=1
			dc_default_digit_clamp=1;;

		DBG)
			CFLAGS="-Weverything -Wno-padded -Wno-unsafe-buffer-usage -Wno-poison-system-directories -Werror -pedantic -std=c11"
			bc_only=0
			dc_only=0
			coverage=0
			debug=1
			optimization="0"
			hist=1
			hist_impl="internal"
			extra_math=1
			generate_tests=1
			install_manpages=1
			nls=1
			force=0
			strip_bin=1
			all_locales=0
			library=0
			fuzz=0
			time_tests=0
			vg=0
			memcheck=1
			clean=1
			bc_default_banner=1
			bc_default_sigint_reset=1
			dc_default_sigint_reset=1
			bc_default_tty_mode=1
			dc_default_tty_mode=1
			bc_default_prompt=""
			dc_default_prompt=""
			bc_default_expr_exit=0
			dc_default_expr_exit=0
			bc_default_digit_clamp=1
			dc_default_digit_clamp=1;;

		?|'') usage "Invalid user build: \"$_predefined_build_type\". Accepted types are BSD, GNU, GDH, DBG.";;

	esac
}

# Generates a list of script test targets that will be used as prerequisites for
# other targets.
#
# @param name  The name of the calculator to generate script test targets for.
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

# This is a list of defaults, but it is also the list of possible options for
# users to change.
#
# The development options are: force (force options even if they fail), valgrind
# (build in a way suitable for valgrind testing), memcheck (same as valgrind),
# and fuzzing (build in a way suitable for fuzzing).
bc_only=0
dc_only=0
coverage=0
karatsuba_len=32
debug=0
hist=1
hist_impl="internal"
extra_math=1
optimization=""
generate_tests=1
install_manpages=1
nls=1
force=0
strip_bin=1
all_locales=0
library=0
fuzz=0
time_tests=0
vg=0
memcheck=0
clean=1
problematic_tests=1

# The empty strings are because they depend on TTY mode. If they are directly
# set, though, they will be integers. We test for empty strings later.
bc_default_banner=0
bc_default_sigint_reset=1
dc_default_sigint_reset=1
bc_default_tty_mode=1
dc_default_tty_mode=0
bc_default_prompt=""
dc_default_prompt=""
bc_default_expr_exit=1
dc_default_expr_exit=1
bc_default_digit_clamp=0
dc_default_digit_clamp=0

# getopts is a POSIX utility, but it cannot handle long options. Thus, the
# handling of long options is done by hand, and that's the reason that short and
# long options cannot be mixed.
while getopts "abBcdDeEfgGhHik:lMmNO:p:PrS:s:tTvz-" opt; do

	case "$opt" in
		a) library=1 ;;
		b) bc_only=1 ;;
		B) dc_only=1 ;;
		c) coverage=1 ;;
		C) clean=0 ;;
		d) dc_only=1 ;;
		D) bc_only=1 ;;
		e) hist_impl="editline" ;;
		E) extra_math=0 ;;
		f) force=1 ;;
		g) debug=1 ;;
		G) generate_tests=0 ;;
		h) usage ;;
		H) hist=0 ;;
		i) hist_impl="internal" ;;
		k) karatsuba_len="$OPTARG" ;;
		l) all_locales=1 ;;
		m) memcheck=1 ;;
		M) install_manpages=0 ;;
		N) nls=0 ;;
		O) optimization="$OPTARG" ;;
		p) predefined_build "$OPTARG" ;;
		P) problematic_tests=0 ;;
		r) hist_impl="readline" ;;
		S) set_default 0 "$OPTARG" ;;
		s) set_default 1 "$OPTARG" ;;
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
				set-default-on=?*) set_default 1 "$LONG_OPTARG" ;;
				set-default-on)
					if [ "$#" -lt 2 ]; then
						usage "No argument given for '--$arg' option"
					fi
					set_default 1 "$1"
					shift ;;
				set-default-off=?*) set_default 0 "$LONG_OPTARG" ;;
				set-default-off)
					if [ "$#" -lt 2 ]; then
						usage "No argument given for '--$arg' option"
					fi
					set_default 0 "$1"
					shift ;;
				predefined-build-type=?*) predefined_build "$LONG_OPTARG" ;;
				predefined-build-type)
					if [ "$#" -lt 2 ]; then
						usage "No argument given for '--$arg' option"
					fi
					predefined_build "$1"
					shift ;;
				disable-bc) dc_only=1 ;;
				disable-dc) bc_only=1 ;;
				disable-clean) clean=0 ;;
				disable-extra-math) extra_math=0 ;;
				disable-generated-tests) generate_tests=0 ;;
				disable-history) hist=0 ;;
				disable-man-pages) install_manpages=0 ;;
				disable-nls) nls=0 ;;
				disable-strip) strip_bin=0 ;;
				disable-problematic-tests) problematic_tests=0 ;;
				enable-editline) hist_impl="editline" ;;
				enable-readline) hist_impl="readline" ;;
				enable-internal-history) hist_impl="internal" ;;
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
				disable-problematic-tests*)
					usage "No arg allowed for --$arg option" ;;
				enable-fuzz-mode* | enable-test-timing* | enable-valgrind*)
					usage "No arg allowed for --$arg option" ;;
				enable-memcheck* | install-all-locales*)
					usage "No arg allowed for --$arg option" ;;
				enable-editline* | enable-readline*)
					usage "No arg allowed for --$arg option" ;;
				enable-internal-history*)
					usage "No arg allowed for --$arg option" ;;
				'') break ;; # "--" terminates argument processing
				* ) usage "Invalid option $LONG_OPTARG" ;;
			esac
			shift
			OPTIND=1 ;;
		?) usage "Invalid option: $opt" ;;
	esac

done

# Sometimes, developers don't want configure.sh to do a config clean. But
# sometimes they do.
if [ "$clean" -ne 0 ]; then
	if [ -f ./Makefile ]; then
		make clean_config > /dev/null
	fi
fi

# It is an error to say that bc only should be built and likewise for dc.
if [ "$bc_only" -eq 1 ] && [ "$dc_only" -eq 1 ]; then
	usage "Can only specify one of -b(-D) or -d(-B)"
fi

# The library is mutually exclusive to the calculators, so it's an error to
# give an option for either of them.
if [ "$library" -ne 0 ]; then
	if [ "$bc_only" -eq 1 ] || [ "$dc_only" -eq 1 ]; then
		usage "Must not specify -b(-D) or -d(-B) when building the library"
	fi
fi

# KARATSUBA_LEN must be an integer and must be 16 or greater.
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
	LONG_BIT_DEFINE="-DBC_LONG_BIT=$LONG_BIT"
fi

if [ -z "$CC" ]; then
	CC="c99"
else

	# I had users complain that, if they gave CFLAGS as part of CC, which
	# autotools allows in its braindead way, the build would fail with an error.
	# I don't like adjusting for autotools, but oh well. These lines puts the
	# stuff after the first space into CFLAGS.
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

	# Like above, this splits HOSTCC and HOSTCFLAGS.
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

# Store these for the cross compilation detection later.
OLDCFLAGS="$CFLAGS"
OLDHOSTCFLAGS="$HOSTCFLAGS"

link="@printf 'No link necessary\\\\n'"
main_exec="BC"
executable="BC_EXEC"

tests="test_bc timeconst test_dc"

bc_test="@export BC_TEST_OUTPUT_DIR=\"$builddir/tests\"; \$(TESTSDIR)/all.sh bc $extra_math 1 $generate_tests $problematic_tests $time_tests \$(BC_EXEC)"
bc_test_np="@export BC_TEST_OUTPUT_DIR=\"$builddir/tests\"; \$(TESTSDIR)/all.sh -n bc $extra_math 1 $generate_tests $problematic_tests $time_tests \$(BC_EXEC)"
dc_test="@export BC_TEST_OUTPUT_DIR=\"$builddir/tests\"; \$(TESTSDIR)/all.sh dc $extra_math 1 $generate_tests $problematic_tests $time_tests \$(DC_EXEC)"
dc_test_np="@export BC_TEST_OUTPUT_DIR=\"$builddir/tests\"; \$(TESTSDIR)/all.sh -n dc $extra_math 1 $generate_tests $problematic_tests $time_tests \$(DC_EXEC)"

timeconst="@export BC_TEST_OUTPUT_DIR=\"$builddir/tests\"; \$(TESTSDIR)/bc/timeconst.sh \$(TESTSDIR)/bc/scripts/timeconst.bc \$(BC_EXEC)"

# In order to have cleanup at exit, we need to be in
# debug mode, so don't run valgrind without that.
if [ "$vg" -ne 0 ]; then
	debug=1
	bc_test_exec='valgrind $(VALGRIND_ARGS) $(BC_EXEC)'
	dc_test_exec='valgrind $(VALGRIND_ARGS) $(DC_EXEC)'
	bcl_test_exec='valgrind $(VALGRIND_ARGS) $(BCL_TEST)'
else
	bc_test_exec='$(BC_EXEC)'
	dc_test_exec='$(DC_EXEC)'
	bcl_test_exec='$(BCL_TEST)'
fi

test_bc_history_prereqs="test_bc_history_all"
test_dc_history_prereqs="test_dc_history_all"

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

# This if/else if chain is for setting the defaults that change based on whether
# the library is being built, bc only, dc only, or both calculators.
if [ "$library" -ne 0 ]; then

	extra_math=1
	nls=0
	hist=0
	bc=1
	dc=1

	default_target_prereqs="\$(BIN) \$(OBJ)"
	default_target_cmd="ar -r -cu \$(LIBBC) \$(OBJ)"
	default_target="\$(LIBBC)"
	tests="test_library"
	test_bc_history_prereqs=" test_bc_history_skip"
	test_dc_history_prereqs=" test_dc_history_skip"

	install_prereqs=" install_library"
	uninstall_prereqs=" uninstall_library"
	install_man_prereqs=" install_bcl_manpage"
	uninstall_man_prereqs=" uninstall_bcl_manpage"

elif [ "$bc_only" -eq 1 ]; then

	bc=1
	dc=0

	dc_help=""

	executables="bc"

	dc_test="@printf 'No dc tests to run\\\\n'"
	dc_test_np="@printf 'No dc tests to run\\\\n'"
	test_dc_history_prereqs=" test_dc_history_skip"

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
	bc_test_np="@printf 'No bc tests to run\\\\n'"
	test_bc_history_prereqs=" test_bc_history_skip"

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

# We need specific stuff for fuzzing.
if [ "$fuzz" -ne 0 ]; then
	debug=1
	hist=0
	nls=0
	optimization="3"
fi

# This sets some necessary things for debug mode.
if [ "$debug" -eq 1 ]; then

	if [ -z "$CFLAGS" ] && [ -z "$optimization" ]; then
		CFLAGS="-O0"
	fi

	CFLAGS="-g $CFLAGS"

else
	CPPFLAGS="-DNDEBUG $CPPFLAGS"
fi

# Set optimization CFLAGS.
if [ -n "$optimization" ]; then
	CFLAGS="-O$optimization $CFLAGS"
fi

# Set test coverage defaults.
if [ "$coverage" -eq 1 ]; then

	if [ "$bc_only" -eq 1 ] || [ "$dc_only" -eq 1 ]; then
		usage "Can only specify -c without -b or -d"
	fi

	CFLAGS="-fprofile-arcs -ftest-coverage -g -O0 $CFLAGS"
	CPPFLAGS="-DNDEBUG $CPPFLAGS"

	COVERAGE_OUTPUT="@gcov -pabcdf \$(GCDA) \$(BC_GCDA) \$(DC_GCDA) \$(HISTORY_GCDA) \$(RAND_GCDA)"
	COVERAGE_OUTPUT="$COVERAGE_OUTPUT;\$(RM) -f \$(GEN)*.gc*"
	COVERAGE_OUTPUT="$COVERAGE_OUTPUT;gcovr --exclude-unreachable-branches --exclude-throw-branches --html-details --output index.html"
	COVERAGE_PREREQS=" test coverage_output"

else
	COVERAGE_OUTPUT="@printf 'Coverage not generated\\\\n'"
	COVERAGE_PREREQS=""
fi


# Set some defaults.
if [ -z "${DESTDIR+set}" ]; then
	destdir=""
else
	destdir="DESTDIR = $DESTDIR"
fi

# defprefix is for a warning about locales later.
if [ -z "${PREFIX+set}" ]; then
	PREFIX="/usr/local"
	defprefix=1
else
	defprefix=0
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

if [ -z "${PC_PATH+set}" ]; then

	set +e

	command -v pkg-config > /dev/null
	err=$?

	set -e

	if [ "$err" -eq 0 ]; then
		PC_PATH=$(pkg-config --variable=pc_path pkg-config)
		PC_PATH="${PC_PATH%%:*}"
	else
		PC_PATH=""
	fi

fi

# Set a default for the DATAROOTDIR. This is done if either manpages will be
# installed, or locales are enabled because that's probably where NLSPATH
# points.
if [ "$install_manpages" -ne 0 ] || [ "$nls" -ne 0 ]; then
	if [ -z "${DATAROOTDIR+set}" ]; then
		DATAROOTDIR="$PREFIX/share"
	fi
fi

# Set defaults for manpage environment variables.
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

# Here is where we test NLS (the locale system). This is done by trying to
# compile src/vm.c, which has the relevant code. If it fails, then it is
# disabled.
if [ "$nls" -ne 0 ]; then

	set +e

	printf 'Testing NLS...\n'

	flags="-DBC_ENABLE_NLS=1 -DBC_ENABLED=$bc -DDC_ENABLED=$dc"
	flags="$flags -DBC_ENABLE_HISTORY=$hist -DBC_ENABLE_LIBRARY=0 -DBC_ENABLE_AFL=0"
	flags="$flags -DBC_ENABLE_EXTRA_MATH=$extra_math -I$scriptdir/include/"
	flags="$flags -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700"

	ccbase=$(basename "$CC")

	if [ "$ccbase" = "clang" ]; then
		flags="$flags -Wno-unreachable-code"
	fi

	"$CC" $CPPFLAGS $CFLAGS $flags -c "$scriptdir/src/vm.c" -o "./vm.o" > /dev/null 2>&1

	err="$?"

	rm -rf "./vm.o"

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
		gencat "./en_US.cat" "$scriptdir/locales/en_US.msg" > /dev/null 2>&1

		err="$?"

		rm -rf "./en_US.cat"

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

			# It turns out that POSIX locales are really terrible, and running
			# gencat on one machine is not guaranteed to make those cat files
			# portable to another machine, so we had better warn the user here.
			if [ "$HOSTCC" != "$CC" ] || [ "$OLDHOSTCFLAGS" != "$OLDCFLAGS" ]; then
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

# Like the above tested locale support, this tests history.
if [ "$hist" -eq 1 ]; then

	if [ "$hist_impl" = "editline" ]; then
		editline=1
		readline=0
	elif [ "$hist_impl" = "readline" ]; then
		editline=0
		readline=1
	else
		editline=0
		readline=0
	fi

	set +e

	printf 'Testing history...\n'

	flags="-DBC_ENABLE_HISTORY=1 -DBC_ENABLED=$bc -DDC_ENABLED=$dc"
	flags="$flags -DBC_ENABLE_NLS=$nls -DBC_ENABLE_LIBRARY=0 -DBC_ENABLE_AFL=0"
	flags="$flags -DBC_ENABLE_EDITLINE=$editline -DBC_ENABLE_READLINE=$readline"
	flags="$flags -DBC_ENABLE_EXTRA_MATH=$extra_math -I$scriptdir/include/"
	flags="$flags -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700"

	"$CC" $CPPFLAGS $CFLAGS $flags -c "$scriptdir/src/history.c" -o "./history.o" > /dev/null 2>&1

	err="$?"

	rm -rf "./history.o"

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

else

	editline=0
	readline=0

fi

# We have to disable the history tests if it is disabled or valgrind is on. Or
# if we are using editline or readline.
if [ "$hist" -eq 0 ] || [ "$vg" -ne 0 ]; then
	test_bc_history_prereqs=" test_bc_history_skip"
	test_dc_history_prereqs=" test_dc_history_skip"
	history_tests="@printf 'Skipping history tests...\\\\n'"
	CFLAGS="$CFLAGS -DBC_ENABLE_EDITLINE=0 -DBC_ENABLE_READLINE=0"
else

	if [ "$editline" -eq 0 ] && [ "$readline" -eq 0 ]; then
		history_tests="@printf '\$(TEST_STARS)\\\\n\\\\nRunning history tests...\\\\n\\\\n'"
		history_tests="$history_tests \&\& \$(TESTSDIR)/history.sh bc -a \&\&"
		history_tests="$history_tests \$(TESTSDIR)/history.sh dc -a \&\& printf"
		history_tests="$history_tests '\\\\nAll history tests passed.\\\\n\\\\n\$(TEST_STARS)\\\\n'"
	else
		test_bc_history_prereqs=" test_bc_history_skip"
		test_dc_history_prereqs=" test_dc_history_skip"
		history_tests="@printf 'Skipping history tests...\\\\n'"
	fi

	# We are also setting the CFLAGS and LDFLAGS here.
	if [ "$editline" -ne 0 ]; then
		LDFLAGS="$LDFLAGS -ledit"
		CPPFLAGS="$CPPFLAGS -DBC_ENABLE_EDITLINE=1 -DBC_ENABLE_READLINE=0"
	elif [ "$readline" -ne 0 ]; then
		LDFLAGS="$LDFLAGS -lreadline"
		CPPFLAGS="$CPPFLAGS -DBC_ENABLE_EDITLINE=0 -DBC_ENABLE_READLINE=1"
	else
		CPPFLAGS="$CPPFLAGS -DBC_ENABLE_EDITLINE=0 -DBC_ENABLE_READLINE=0"
	fi

fi

# Test FreeBSD. This is not in an if statement because regardless of whatever
# the user says, we need to know if we are on FreeBSD. If we are, we cannot set
# _POSIX_C_SOURCE and _XOPEN_SOURCE. The FreeBSD headers turn *off* stuff when
# that is done.
set +e
printf 'Testing for FreeBSD...\n'

flags="-DBC_TEST_FREEBSD -DBC_ENABLE_AFL=0"
"$CC" $CPPFLAGS $CFLAGS $flags "-I$scriptdir/include" -E "$scriptdir/src/vm.c" > /dev/null 2>&1

err="$?"

if [ "$err" -ne 0 ]; then
	printf 'On FreeBSD. Not using _POSIX_C_SOURCE and _XOPEN_SOURCE.\n\n'
else
	printf 'Not on FreeBSD. Using _POSIX_C_SOURCE and _XOPEN_SOURCE.\n\n'
	CPPFLAGS="$CPPFLAGS -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700"
fi

# Test Mac OSX. This is not in an if statement because regardless of whatever
# the user says, we need to know if we are on Mac OSX. If we are, we have to set
# _DARWIN_C_SOURCE.
printf 'Testing for Mac OSX...\n'

flags="-DBC_TEST_APPLE -DBC_ENABLE_AFL=0"
"$CC" $CPPFLAGS $CFLAGS $flags "-I$scriptdir/include" -E "$scriptdir/src/vm.c" > /dev/null 2>&1

err="$?"

if [ "$err" -ne 0 ]; then
	printf 'On Mac OSX. Using _DARWIN_C_SOURCE.\n\n'
	apple="-D_DARWIN_C_SOURCE"
else
	printf 'Not on Mac OSX.\n\n'
	apple=""
fi

# We can't use the linker's strip flag on Mac OSX.
if [ "$debug" -eq 0 ] && [ "$apple" == "" ] && [ "$strip_bin" -ne 0 ]; then
	LDFLAGS="-s $LDFLAGS"
fi

# Test OpenBSD. This is not in an if statement because regardless of whatever
# the user says, we need to know if we are on OpenBSD to activate _BSD_SOURCE.
# No, I cannot `#define _BSD_SOURCE` in a header because OpenBSD's patched GCC
# and Clang complain that that is only allowed for system headers. Sigh....So we
# have to check at configure time and set it on the compiler command-line. And
# we have to set it because we also set _POSIX_C_SOURCE, which OpenBSD headers
# detect, and when they detect it, they turn off _BSD_SOURCE unless it is
# specifically requested.
printf 'Testing for OpenBSD...\n'

flags="-DBC_TEST_OPENBSD -DBC_ENABLE_AFL=0"
"$CC" $CPPFLAGS $CFLAGS $flags "-I$scriptdir/include" -E "$scriptdir/src/vm.c" > /dev/null 2>&1

err="$?"

if [ "$err" -ne 0 ]; then

	printf 'On OpenBSD. Using _BSD_SOURCE.\n\n'
	bsd="-D_BSD_SOURCE"

	# Readline errors on OpenBSD, for some weird reason.
	if [ "$readline" -ne 0 ]; then
		usage "Cannot use readline on OpenBSD"
	fi

else
	printf 'Not on OpenBSD.\n\n'
	bsd=""
fi

set -e

if [ "$library" -eq 1 ]; then
	bc_lib=""
fi

if [ "$extra_math" -eq 1 ] && [ "$bc" -ne 0 ] && [ "$library" -eq 0 ]; then
	BC_LIB2_O="\$(GEN_DIR)/lib2.o"
else
	BC_LIB2_O=""
fi

GEN_DIR="$scriptdir/gen"

# These lines set the appropriate targets based on whether `gen/strgen.c` or
# `gen/strgen.sh` is used.
GEN="strgen"
GEN_EXEC_TARGET="\$(HOSTCC) -DBC_ENABLE_AFL=0 -I$scriptdir/include/  \$(HOSTCFLAGS) -o \$(GEN_EXEC) \$(GEN_C)"
CLEAN_PREREQS=" clean_gen clean_coverage"

if [ -z "${GEN_HOST+set}" ]; then
	GEN_HOST=1
else
	if [ "$GEN_HOST" -eq 0 ]; then
		GEN="strgen.sh"
		GEN_EXEC_TARGET="@printf 'Do not need to build gen/strgen.c\\\\n'"
		CLEAN_PREREQS=" clean_coverage"
	fi
fi

manpage_args=""
unneeded=""
headers="\$(HEADERS)"

# This series of if statements figure out what source files are *not* needed.
if [ "$extra_math" -eq 0 ]; then
	exclude_extra_math=1
	manpage_args="E"
	unneeded="$unneeded rand.c"
else
	exclude_extra_math=0
	headers="$headers \$(EXTRA_MATH_HEADERS)"
fi

# All of these next if statements set the build type and mark certain source
# files as unneeded so that they won't have targets generated for them.

if [ "$hist" -eq 0 ]; then
	manpage_args="${manpage_args}H"
	unneeded="$unneeded history.c"
else
	headers="$headers \$(HISTORY_HEADERS)"
fi

if [ "$nls" -eq 0 ]; then
	manpage_args="${manpage_args}N"
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

# This convoluted mess does pull the version out. If you change the format of
# include/version.h, you may have to change this line.
version=$(cat "$scriptdir/include/version.h" | grep "VERSION " - | awk '{ print $3 }' -)

if [ "$library" -ne 0 ]; then

	unneeded="$unneeded args.c opt.c read.c file.c main.c"
	unneeded="$unneeded lang.c lex.c parse.c program.c"
	unneeded="$unneeded bc.c bc_lex.c bc_parse.c"
	unneeded="$unneeded dc.c dc_lex.c dc_parse.c"
	headers="$headers \$(LIBRARY_HEADERS)"

	if [ "$PC_PATH" != "" ]; then

		contents=$(cat "$scriptdir/bcl.pc.in")

		contents=$(replace "$contents" "INCLUDEDIR" "$INCLUDEDIR")
		contents=$(replace "$contents" "LIBDIR" "$LIBDIR")
		contents=$(replace "$contents" "VERSION" "$version")

		printf '%s\n' "$contents" > "$scriptdir/bcl.pc"

		pkg_config_install="\$(SAFE_INSTALL) \$(PC_INSTALL_ARGS) \"\$(BCL_PC)\" \"\$(DESTDIR)\$(PC_PATH)/\$(BCL_PC)\""
		pkg_config_uninstall="\$(RM) -f \"\$(DESTDIR)\$(PC_PATH)/\$(BCL_PC)\""

	else

		pkg_config_install=""
		pkg_config_uninstall=""

	fi

else

	unneeded="$unneeded library.c"

	PC_PATH=""
	pkg_config_install=""
	pkg_config_uninstall=""

fi

# library.c is not needed under normal circumstances.
if [ "$unneeded" = "" ]; then
	unneeded="library.c"
fi

# This sets the appropriate manpage for a full build.
if [ "$manpage_args" = "" ]; then
	manpage_args="A"
fi

if [ "$vg" -ne 0 ]; then
	memcheck=1
fi

if [ "$bc_default_prompt" = "" ]; then
	bc_default_prompt="$bc_default_tty_mode"
fi

if [ "$dc_default_prompt" = "" ]; then
	dc_default_prompt="$dc_default_tty_mode"
fi

# Generate the test targets and prerequisites.
bc_tests=$(gen_std_test_targets bc)
bc_script_tests=$(gen_script_test_targets bc)
bc_err_tests=$(gen_err_test_targets bc)
dc_tests=$(gen_std_test_targets dc)
dc_script_tests=$(gen_script_test_targets dc)
dc_err_tests=$(gen_err_test_targets dc)

printf 'unneeded: %s\n' "$unneeded"

# Print out the values; this is for debugging.
printf 'Version: %s\n' "$version"

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
printf 'BC_ENABLE_NLS=%s\n\n' "$nls"
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
printf 'PC_PATH=%s\n' "$PC_PATH"
printf 'EXECSUFFIX=%s\n' "$EXECSUFFIX"
printf 'EXECPREFIX=%s\n' "$EXECPREFIX"
printf 'DESTDIR=%s\n' "$DESTDIR"
printf 'LONG_BIT=%s\n' "$LONG_BIT"
printf 'GEN_HOST=%s\n' "$GEN_HOST"
printf 'GEN_EMU=%s\n' "$GEN_EMU"
printf '\n'
printf 'Setting Defaults\n'
printf '================\n'
printf 'bc.banner=%s\n' "$bc_default_banner"
printf 'bc.sigint_reset=%s\n' "$bc_default_sigint_reset"
printf 'dc.sigint_reset=%s\n' "$dc_default_sigint_reset"
printf 'bc.tty_mode=%s\n' "$bc_default_tty_mode"
printf 'dc.tty_mode=%s\n' "$dc_default_tty_mode"
printf 'bc.prompt=%s\n' "$bc_default_prompt"
printf 'dc.prompt=%s\n' "$dc_default_prompt"
printf 'bc.expr_exit=%s\n' "$bc_default_expr_exit"
printf 'dc.expr_exit=%s\n' "$dc_default_expr_exit"
printf 'bc.digit_clamp=%s\n' "$bc_default_digit_clamp"
printf 'dc.digit_clamp=%s\n' "$dc_default_digit_clamp"

# This code outputs a warning. The warning is to not surprise users when locales
# are installed outside of the prefix. This warning is suppressed when the
# default prefix is used, as well, so as not to panic users just installing by
# hand. I believe this will be okay because NLSPATH is usually in /usr and the
# default prefix is /usr/local, so they'll be close that way.
if [ "$nls" -ne 0 ] && [ "${NLSPATH#$PREFIX}" = "${NLSPATH}" ] && [ "$defprefix" -eq 0 ]; then
	printf '\n********************************************************************************\n\n'
	printf 'WARNING: Locales will *NOT* be installed in $PREFIX (%s).\n' "$PREFIX"
	printf '\n'
	printf '         This is because they *MUST* be installed at a fixed location to even\n'
	printf '         work, and that fixed location is $NLSPATH (%s).\n' "$NLSPATH"
	printf '\n'
	printf '         This location is *outside* of $PREFIX. If you do not wish to install\n'
	printf '         locales outside of $PREFIX, you must disable NLS with the -N or the\n'
	printf '         --disable-nls options.\n'
	printf '\n'
	printf '         The author apologizes for the inconvenience, but the need to install\n'
	printf '         the locales at a fixed location is mandated by POSIX, and it is not\n'
	printf '         possible for the author to change that requirement.\n'
	printf '\n********************************************************************************\n'
fi

# This is where the real work begins. This is the point at which the Makefile.in
# template is edited and output to the Makefile.

contents=$(cat "$scriptdir/Makefile.in")

needle="WARNING"
replacement='*** WARNING: Autogenerated from Makefile.in. DO NOT MODIFY ***'

contents=$(replace "$contents" "$needle" "$replacement")

# The contents are edited to have the list of files to build.
contents=$(gen_file_list "$contents" $unneeded)

SRC_TARGETS=""

# This line and loop generates the individual targets for source files. I used
# to just use an implicit target, but that was found to be inadequate when I
# added the library.
src_files=$(find_src_files $unneeded)

for f in $src_files; do
	o=$(replace_ext "$f" "c" "o")
	o=$(basename "$o")
	SRC_TARGETS=$(printf '%s\n\nsrc/%s: src %s %s\n\t$(CC) $(CFLAGS) -o src/%s -c %s\n' \
		"$SRC_TARGETS" "$o" "$headers" "$f" "$o" "$f")
done

# Replace all the placeholders.
contents=$(replace "$contents" "ROOTDIR" "$scriptdir")
contents=$(replace "$contents" "BUILDDIR" "$builddir")

contents=$(replace "$contents" "HEADERS" "$headers")

contents=$(replace "$contents" "BC_ENABLED" "$bc")
contents=$(replace "$contents" "DC_ENABLED" "$dc")

contents=$(replace "$contents" "BC_ALL_TESTS" "$bc_test")
contents=$(replace "$contents" "BC_ALL_TESTS_NP" "$bc_test_np")
contents=$(replace "$contents" "BC_TESTS" "$bc_tests")
contents=$(replace "$contents" "BC_SCRIPT_TESTS" "$bc_script_tests")
contents=$(replace "$contents" "BC_ERROR_TESTS" "$bc_err_tests")
contents=$(replace "$contents" "BC_TEST_EXEC" "$bc_test_exec")
contents=$(replace "$contents" "TIMECONST_ALL_TESTS" "$timeconst")

contents=$(replace "$contents" "DC_ALL_TESTS" "$dc_test")
contents=$(replace "$contents" "DC_ALL_TESTS_NP" "$dc_test_np")
contents=$(replace "$contents" "DC_TESTS" "$dc_tests")
contents=$(replace "$contents" "DC_SCRIPT_TESTS" "$dc_script_tests")
contents=$(replace "$contents" "DC_ERROR_TESTS" "$dc_err_tests")
contents=$(replace "$contents" "DC_TEST_EXEC" "$dc_test_exec")

contents=$(replace "$contents" "BCL_TEST_EXEC" "$bcl_test_exec")

contents=$(replace "$contents" "BUILD_TYPE" "$manpage_args")
contents=$(replace "$contents" "EXCLUDE_EXTRA_MATH" "$exclude_extra_math")

contents=$(replace "$contents" "LIBRARY" "$library")
contents=$(replace "$contents" "HISTORY" "$hist")
contents=$(replace "$contents" "EXTRA_MATH" "$extra_math")
contents=$(replace "$contents" "NLS" "$nls")
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

contents=$(replace "$contents" "PC_PATH" "$PC_PATH")
contents=$(replace "$contents" "PKG_CONFIG_INSTALL" "$pkg_config_install")
contents=$(replace "$contents" "PKG_CONFIG_UNINSTALL" "$pkg_config_uninstall")

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

contents=$(replace "$contents" "BC_HISTORY_TEST_PREREQS" "$test_bc_history_prereqs")
contents=$(replace "$contents" "DC_HISTORY_TEST_PREREQS" "$test_dc_history_prereqs")
contents=$(replace "$contents" "HISTORY_TESTS" "$history_tests")

contents=$(replace "$contents" "VG_BC_TEST" "$vg_bc_test")
contents=$(replace "$contents" "VG_DC_TEST" "$vg_dc_test")

contents=$(replace "$contents" "TIMECONST" "$timeconst")

contents=$(replace "$contents" "KARATSUBA" "$karatsuba")
contents=$(replace "$contents" "KARATSUBA_TEST" "$karatsuba_test")

contents=$(replace "$contents" "LONG_BIT_DEFINE" "$LONG_BIT_DEFINE")

contents=$(replace "$contents" "GEN_DIR" "$GEN_DIR")
contents=$(replace "$contents" "GEN" "$GEN")
contents=$(replace "$contents" "GEN_EXEC_TARGET" "$GEN_EXEC_TARGET")
contents=$(replace "$contents" "CLEAN_PREREQS" "$CLEAN_PREREQS")
contents=$(replace "$contents" "GEN_EMU" "$GEN_EMU")

contents=$(replace "$contents" "BSD" "$bsd")
contents=$(replace "$contents" "APPLE" "$apple")

contents=$(replace "$contents" "BC_DEFAULT_BANNER" "$bc_default_banner")
contents=$(replace "$contents" "BC_DEFAULT_SIGINT_RESET" "$bc_default_sigint_reset")
contents=$(replace "$contents" "DC_DEFAULT_SIGINT_RESET" "$dc_default_sigint_reset")
contents=$(replace "$contents" "BC_DEFAULT_TTY_MODE" "$bc_default_tty_mode")
contents=$(replace "$contents" "DC_DEFAULT_TTY_MODE" "$dc_default_tty_mode")
contents=$(replace "$contents" "BC_DEFAULT_PROMPT" "$bc_default_prompt")
contents=$(replace "$contents" "DC_DEFAULT_PROMPT" "$dc_default_prompt")
contents=$(replace "$contents" "BC_DEFAULT_EXPR_EXIT" "$bc_default_expr_exit")
contents=$(replace "$contents" "DC_DEFAULT_EXPR_EXIT" "$dc_default_expr_exit")
contents=$(replace "$contents" "BC_DEFAULT_DIGIT_CLAMP" "$bc_default_digit_clamp")
contents=$(replace "$contents" "DC_DEFAULT_DIGIT_CLAMP" "$dc_default_digit_clamp")

# Do the first print to the Makefile.
printf '%s\n%s\n\n' "$contents" "$SRC_TARGETS" > "Makefile"

# Generate the individual test targets.
if [ "$bc" -ne 0 ]; then
	gen_std_tests bc "$extra_math" "$time_tests" $bc_test_exec
	gen_script_tests bc "$extra_math" "$generate_tests" "$time_tests" $bc_test_exec
	gen_err_tests bc $bc_test_exec
fi

if [ "$dc" -ne 0 ]; then
	gen_std_tests dc "$extra_math" "$time_tests" $dc_test_exec
	gen_script_tests dc "$extra_math" "$generate_tests" "$time_tests" $dc_test_exec
	gen_err_tests dc $dc_test_exec
fi

# Copy the correct manuals to the expected places.
mkdir -p manuals
cp -f "$scriptdir/manuals/bc/$manpage_args.1.md" manuals/bc.1.md
cp -f "$scriptdir/manuals/bc/$manpage_args.1" manuals/bc.1
cp -f "$scriptdir/manuals/dc/$manpage_args.1.md" manuals/dc.1.md
cp -f "$scriptdir/manuals/dc/$manpage_args.1" manuals/dc.1

make clean > /dev/null
