dnl Check whether the compiler supports particular flags.
dnl
dnl Provides RRA_PROG_CC_FLAG and RRA_PROG_LD_FLAG, which checks whether a
dnl compiler supports a given flag for either compilation or linking,
dnl respectively.  If it does, the commands in the second argument are run.
dnl If not, the commands in the third argument are run.
dnl
dnl Provides RRA_PROG_CC_WARNINGS_FLAGS, which checks whether a compiler
dnl supports a large set of warning flags and sets the WARNINGS_CFLAGS
dnl substitution variable to all of the supported warning flags.  (Note that
dnl this may be too aggressive for some people.)
dnl
dnl Depends on RRA_PROG_CC_CLANG.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Copyright 2016-2021 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2006, 2009, 2016
dnl     by Internet Systems Consortium, Inc. ("ISC")
dnl
dnl Permission to use, copy, modify, and/or distribute this software for any
dnl purpose with or without fee is hereby granted, provided that the above
dnl copyright notice and this permission notice appear in all copies.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
dnl REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
dnl MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
dnl SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
dnl WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
dnl ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
dnl IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
dnl
dnl SPDX-License-Identifier: ISC

dnl Used to build the result cache name.
AC_DEFUN([_RRA_PROG_CC_FLAG_CACHE],
[translit([rra_cv_compiler_c_$1], [-=+,], [____])])
AC_DEFUN([_RRA_PROG_LD_FLAG_CACHE],
[translit([rra_cv_linker_c_$1], [-=+,], [____])])

dnl Check whether a given flag is supported by the compiler when compiling a C
dnl source file.
AC_DEFUN([RRA_PROG_CC_FLAG],
[AC_REQUIRE([AC_PROG_CC])
 AC_MSG_CHECKING([if $CC supports $1])
 AC_CACHE_VAL([_RRA_PROG_CC_FLAG_CACHE([$1])],
    [save_CFLAGS=$CFLAGS
     AS_CASE([$1],
        [-Wno-*], [CFLAGS="$CFLAGS `AS_ECHO(["$1"]) | sed 's/-Wno-/-W/'`"],
        [*],      [CFLAGS="$CFLAGS $1"])
     AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [int foo = 0;])],
        [_RRA_PROG_CC_FLAG_CACHE([$1])=yes],
        [_RRA_PROG_CC_FLAG_CACHE([$1])=no])
     CFLAGS=$save_CFLAGS])
 AC_MSG_RESULT([$_RRA_PROG_CC_FLAG_CACHE([$1])])
 AS_IF([test x"$_RRA_PROG_CC_FLAG_CACHE([$1])" = xyes], [$2], [$3])])

dnl Check whether a given flag is supported by the compiler when linking an
dnl executable.
AC_DEFUN([RRA_PROG_LD_FLAG],
[AC_REQUIRE([AC_PROG_CC])
 AC_MSG_CHECKING([if $CC supports $1 for linking])
 AC_CACHE_VAL([_RRA_PROG_LD_FLAG_CACHE([$1])],
    [save_LDFLAGS=$LDFLAGS
     LDFLAGS="$LDFLAGS $1"
     AC_LINK_IFELSE([AC_LANG_PROGRAM([], [int foo = 0;])],
        [_RRA_PROG_LD_FLAG_CACHE([$1])=yes],
        [_RRA_PROG_LD_FLAG_CACHE([$1])=no])
     LDFLAGS=$save_LDFLAGS])
 AC_MSG_RESULT([$_RRA_PROG_LD_FLAG_CACHE([$1])])
 AS_IF([test x"$_RRA_PROG_LD_FLAG_CACHE([$1])" = xyes], [$2], [$3])])

dnl Determine the full set of viable warning flags for the current compiler.
dnl
dnl This is based partly on personal preference and is a fairly aggressive set
dnl of warnings.  Desirable CC warnings that can't be turned on due to other
dnl problems:
dnl
dnl   -Wsign-conversion  Too many fiddly changes for the benefit
dnl   -Wstack-protector  Too many false positives from small buffers
dnl
dnl Last checked against gcc 9.2.1 (2019-09-01).  -D_FORTIFY_SOURCE=2 enables
dnl warn_unused_result attribute markings on glibc functions on Linux, which
dnl catches a few more issues.  Add -O2 because gcc won't find some warnings
dnl without optimization turned on.
dnl
dnl For Clang, we try to use -Weverything, but we have to disable some of the
dnl warnings:
dnl
dnl   -Wcast-qual                     Some structs require casting away const
dnl   -Wdisabled-macro-expansion      Triggers on libc (sigaction.sa_handler)
dnl   -Wpadded                        Not an actual problem
dnl   -Wreserved-id-macros            Autoconf sets several of these normally
dnl   -Wsign-conversion               Too many fiddly changes for the benefit
dnl   -Wtautological-pointer-compare  False positives with for loops
dnl   -Wundef                         Conflicts with Autoconf probe results
dnl   -Wunreachable-code              Happens with optional compilation
dnl   -Wunreachable-code-return       Other compilers get confused
dnl   -Wunused-macros                 Often used on suppressed branches
dnl   -Wused-but-marked-unused        Happens a lot with conditional code
dnl
dnl Sets WARNINGS_CFLAGS as a substitution variable.
AC_DEFUN([RRA_PROG_CC_WARNINGS_FLAGS],
[AC_REQUIRE([RRA_PROG_CC_CLANG])
 AS_IF([test x"$CLANG" = xyes],
    [WARNINGS_CFLAGS="-Werror"
     m4_foreach_w([flag],
        [-Weverything -Wno-cast-qual -Wno-disabled-macro-expansion -Wno-padded
         -Wno-sign-conversion -Wno-reserved-id-macro
         -Wno-tautological-pointer-compare -Wno-undef -Wno-unreachable-code
         -Wno-unreachable-code-return -Wno-unused-macros
         -Wno-used-but-marked-unused],
        [RRA_PROG_CC_FLAG(flag,
            [WARNINGS_CFLAGS="${WARNINGS_CFLAGS} flag"])])],
    [WARNINGS_CFLAGS="-g -O2 -D_FORTIFY_SOURCE=2 -Werror"
     m4_foreach_w([flag],
        [-fstrict-overflow -fstrict-aliasing -Wall -Wextra -Wformat=2
         -Wformat-overflow=2 -Wformat-signedness -Wformat-truncation=2
         -Wnull-dereference -Winit-self -Wswitch-enum -Wstrict-overflow=5
         -Wmissing-format-attribute -Walloc-zero -Wduplicated-branches
         -Wduplicated-cond -Wtrampolines -Wfloat-equal
         -Wdeclaration-after-statement -Wshadow -Wpointer-arith
         -Wbad-function-cast -Wcast-align -Wwrite-strings -Wconversion
         -Wno-sign-conversion -Wdate-time -Wjump-misses-init -Wlogical-op
         -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes
         -Wmissing-declarations -Wnormalized=nfc -Wpacked -Wredundant-decls
         -Wrestrict -Wnested-externs -Winline -Wvla],
        [RRA_PROG_CC_FLAG(flag,
            [WARNINGS_CFLAGS="${WARNINGS_CFLAGS} flag"])])])
 AC_SUBST([WARNINGS_CFLAGS])])
