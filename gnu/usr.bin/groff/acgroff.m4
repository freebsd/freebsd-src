dnl Autoconf macros for groff.
dnl Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
dnl 
dnl This file is part of groff.
dnl 
dnl groff is free software; you can redistribute it and/or modify it under
dnl the terms of the GNU General Public License as published by the Free
dnl Software Foundation; either version 2, or (at your option) any later
dnl version.
dnl 
dnl groff is distributed in the hope that it will be useful, but WITHOUT ANY
dnl WARRANTY; without even the implied warranty of MERCHANTABILITY or
dnl FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
dnl for more details.
dnl 
dnl You should have received a copy of the GNU General Public License along
dnl with groff; see the file COPYING.  If not, write to the Free Software
dnl Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
dnl
define(GROFF_EXIT,[rm -f conftest* core; exit 1])dnl
define(GROFF_PREFIX,[AC_PROVIDE([$0])AC_PREFIX(grops)AC_PREFIX(gcc)])dnl
define(GROFF_PROG_CCC,
[AC_PROVIDE([$0])AC_REQUIRE([AC_PROG_CC])dnl
cc_compile='$CCC conftest.cc -o conftest $CCLIBS $LIBS >/dev/null 2>&1'
AC_SUBST(CCLIBS)
if test -z "$CCC"; then
# See whether the C compiler is also a C++ compiler.
echo checking if C compiler is also a C++ compiler
cat <<EOF > conftest.cc
#ifdef __cplusplus
  yes
#endif
EOF
$CC -E conftest.cc >conftest.out 2>&1
if egrep yes conftest.out >/dev/null 2>&1; then
  CCC="$CC"
fi
fi
AC_PROGRAM_CHECK(CCC,g++,g++,)
AC_PROGRAM_CHECK(CCC,CC,CC,)
AC_PROGRAM_CHECK(CCC,cc++,cc++,)
if test -z "$CCC"; then
cat <<EOM
This package requires a C++ compiler, but I couldn't find one.
Set the environment variable CCC to the name of your C++ compiler.
EOM
GROFF_EXIT
fi
echo checking that C++ compiler can compile very simple C++ program
GROFF_CC_TEST_PROGRAM([
int main() { return 0; }
],,
cat <<EOM
$CCC was unable successfully to compile a very simple C++ program
(the C++ program was in a file with a suffix of .cc)
EOM
GROFF_EXIT
,)
echo checking that C++ static constructors and destructors are called
GROFF_CC_TEST_PROGRAM([
extern "C" {
  void _exit(int);
}
int i;
struct A {
  char dummy;
  A() { i = 1; }
  ~A() { if (i == 1) _exit(0); }
};
A a;
int main() { return 1; }
],,
cat <<EOM
$CCC is not installed correctly: static constructors and destructors do not work
EOM
GROFF_EXIT
,)
GROFF_CC_COMPILE_CHECK([C++ header files],[#include <stdio.h>],
[fopen(0, 0);],,
[cat <<\EOF
Your header files do not appear to support C++.
I was unable to compile and link a simple C++ program that used a function
declared in <stdio.h>.
If you're using a version of gcc/g++ earlier than 2.5,
you should install libg++.
EOF
GROFF_EXIT])])dnl
define(GROFF_CC_COMPILE_CHECK,
[AC_PROVIDE([$0])AC_REQUIRE([GROFF_PROG_CCC])echo checking for $1
cat <<EOF >conftest.cc
[$2]
int main() { return 0; } void t() { [$3] }
EOF
dnl Don't try to run the program, which would prevent cross-configuring.
if eval $cc_compile; then
  ifelse([$4], , :, [$4])
ifelse([$5], , , [else
  $5
])dnl
fi
rm -f conftest*])dnl
dnl
define(GROFF_CC_TEST_PROGRAM,
[AC_PROVIDE([$0])AC_REQUIRE([GROFF_PROG_CCC])ifelse([$4], , ,
[AC_REQUIRE([AC_CROSS_CHECK])if $cross_compiling
then
  $4
else
])dnl
cat <<EOF > conftest.cc
[$1]
EOF
rm -f conftest
eval $cc_compile
if test -s conftest && (./conftest) 2>/dev/null; then
  ifelse([$2], , :, [$2])
ifelse([$3], , , [else
  $3
])dnl
fi
ifelse([$4], , , fi
)dnl
rm -f conftest*])dnl
dnl
define(GROFF_PAGE,
[AC_REQUIRE([GROFF_PREFIX])
if test -z "$PAGE" && test -r $prefix/lib/groff/font/devps/DESC
then
	if grep "^paperlength 841890" \
		$prefix/lib/groff/font/devps/DESC >/dev/null 2>&1
	then
		PAGE=A4
	else
		PAGE=letter
	fi
fi
if test -z "$PAGE"
then
	dom=`awk '([$]1 == "dom" || [$]1 == "search") { print [$]2; exit}' \
	    /etc/resolv.conf 2>/dev/null`

	if test -z "$dom"
	then
		dom=`(domainname) 2>/dev/null | tr -d '+'`
		if test -z "$dom"
		then
			dom=`(hostname) 2>/dev/null | grep '\.'`
		fi
	fi

changequote(,)dnl
	# If the top-level domain is two letters and it's not `us' or `ca'
	# then they probably use A4 paper.
	case "$dom" in
	*.[Uu][Ss]|*.[Cc][Aa]) ;;
	*.[A-Za-z][A-Za-z]) PAGE=A4 ;;
	esac
changequote([,])dnl
fi
test -n "$PAGE" || PAGE=letter
echo guessing $PAGE size paper
AC_SUBST(PAGE)])dnl
dnl
define(GROFF_PERL_PATH,
[echo checking for perl
PERLPATH=
saveifs="$IFS"; IFS="${IFS}:"
for dir in $PATH; do
  test -z "$dir" && dir=.
  if test -f $dir/perl; then
     PERLPATH="$dir/perl"
     break
  fi
done
IFS="$saveifs"
AC_SUBST(PERLPATH)])dnl
dnl
define(GROFF_WCOREFLAG,
[echo checking for w_coredump
AC_TEST_PROGRAM([
#include <sys/types.h>
#include <sys/wait.h>
main()
{
#ifdef WCOREFLAG
  exit(1);
#else
  int i = 0;
  ((union wait *)&i)->w_coredump = 1;
  exit(i != 0200);
#endif
}
],AC_DEFINE(WCOREFLAG,0200),,)])dnl
dnl
define(GROFF_MMAP,
[AC_COMPILE_CHECK([mmap],[#include <sys/types.h>
#include <sys/mman.h>],
[char *p = mmap(0, 0, PROT_READ, MAP_PRIVATE, 0, 0); munmap(p, 0);],
AC_DEFINE(HAVE_MMAP))])dnl
dnl;
define(GROFF_SYS_SIGLIST,
[AC_COMPILE_CHECK([sys_siglist],,changequote(,)dnl
extern char *sys_siglist[]; sys_siglist[0] = 0;,changequote([,])dnl
AC_DEFINE(HAVE_SYS_SIGLIST))])dnl
dnl
define(GROFF_STRUCT_EXCEPTION,
[AC_COMPILE_CHECK([struct exception],[#include <math.h>],
[struct exception e;],
AC_DEFINE(HAVE_STRUCT_EXCEPTION))])dnl
define(GROFF_COOKIE_BUG,
[echo checking for gcc/g++ delete bug
GROFF_CC_TEST_PROGRAM([
#include <stdlib.h>
#include <stddef.h>

int testit = 0;

int main()
{
  testit = 1;
  int *p = new int;
  delete p;
  testit = 0;
  return 1;
}

static unsigned dummy[3];

void *operator new(size_t n)
{
  if (testit) {
    dummy[1] = -(unsigned)(dummy + 2);
    return dummy + 2;
  }
  else
    return (void *)malloc(n);
}

void operator delete(void *p)
{
  if (testit) {
    if (p == dummy)
      exit(0);
  }
  else
    free(p);
}
],AC_DEFINE(COOKIE_BUG),,)])dnl
dnl
define(GROFF_CFRONT_ANSI_BUG,
[AC_REQUIRE([GROFF_LIMITS_H])echo checking for cfront ANSI C INT_MIN bug
GROFF_CC_TEST_PROGRAM([#include <stdlib.h>
#ifdef HAVE_CC_LIMITS_H
#include <limits.h>
#else
#define INT_MAX 2147483647
#endif

#undef INT_MIN
#define INT_MIN (-INT_MAX-1)

int main()
{
  int z = 0;
  return INT_MIN < z;
}
],AC_DEFINE(CFRONT_ANSI_BUG),,)])dnl
dnl
define(GROFF_ARRAY_DELETE,
[GROFF_CC_COMPILE_CHECK(new array delete syntax,,
changequote(,)dnl
char *p = new char[5]; delete [] p;changequote([,]),
,AC_DEFINE(ARRAY_DELETE_NEEDS_SIZE))])dnl
dnl
define(GROFF_BROKEN_SPOOLER_FLAGS,
[test -n "${BROKEN_SPOOLER_FLAGS}" || BROKEN_SPOOLER_FLAGS=7
echo using default value of ${BROKEN_SPOOLER_FLAGS} for grops -b option
AC_SUBST(BROKEN_SPOOLER_FLAGS)])dnl
dnl
define(GROFF_PRINT,
[if test -z "$PSPRINT"
then
	AC_PROGRAMS_CHECK(LPR,lpr)
	AC_PROGRAMS_CHECK(LP,lp)
	if test -n "$LPR" && test -n "$LP"
	then
		# HP-UX provides an lpr command that emulates lpr using lp,
		# but it doesn't have lpq; in this case we want to use lp
		# rather than lpr.
		AC_PROGRAMS_CHECK(LPQ,lpq)
		test -n "$LPQ" || LPR=
	fi
	if test -n "$LPR"
	then
		PSPRINT="$LPR"
	elif test -n "$LP"
	then
		PSPRINT="$LP"
	fi
fi
AC_SUBST(PSPRINT)
# Figure out DVIPRINT from PSPRINT.
if test -n "$PSPRINT" && test -z "$DVIPRINT"
then
	if test "X$PSPRINT" = "Xlpr"
	then
		DVIPRINT="lpr -d"
	else
		DVIPRINT="$PSPRINT"
	fi
fi
AC_SUBST(DVIPRINT)])dnl
define(GROFF_GETOPT,
[GROFF_CC_COMPILE_CHECK([declaration of getopt in stdlib.h],
[#include <stdlib.h>
extern "C" { void getopt(int); }],,,
AC_DEFINE(STDLIB_H_DECLARES_GETOPT))
GROFF_CC_COMPILE_CHECK([declaration of getopt in unistd.h],
[#include <sys/types.h>
#include <unistd.h>
extern "C" { void getopt(int); }],,,
AC_DEFINE(UNISTD_H_DECLARES_GETOPT))
])dnl
define(GROFF_PUTENV,
[GROFF_CC_COMPILE_CHECK([declaration of putenv],
[#include <stdlib.h>
extern "C" { void putenv(int); }],,,
AC_DEFINE(STDLIB_H_DECLARES_PUTENV))])dnl
define(GROFF_POPEN,
[GROFF_CC_COMPILE_CHECK([declaration of popen],
[#include <stdio.h>
extern "C" { void popen(int); }],,,
AC_DEFINE(STDIO_H_DECLARES_POPEN))])dnl
define(GROFF_PCLOSE,
[GROFF_CC_COMPILE_CHECK([declaration of pclose],
[#include <stdio.h>
extern "C" { void pclose(int); }],,,
AC_DEFINE(STDIO_H_DECLARES_PCLOSE))])dnl
define(GROFF_ETAGSCCFLAG,
[echo checking for etags C++ option
for flag in p C
do
	test -z "$ETAGSCCFLAG" || break
	>conftest.c
	(etags -$flag -o /dev/null conftest.c >/dev/null 2>&1) 2>/dev/null &&
		ETAGSCCFLAG="-$flag"
	rm -f conftest.c
done
AC_SUBST(ETAGSCCFLAG)])dnl
define(GROFF_LIMITS_H,
[AC_PROVIDE([$0])GROFF_CC_COMPILE_CHECK(['C++ <limits.h>'],
[#include <limits.h>],
[int x = INT_MIN; int y = INT_MAX; int z = UCHAR_MAX;],
AC_DEFINE(HAVE_CC_LIMITS_H))])dnl
define(GROFF_TRADITIONAL_CPP,
[GROFF_CC_COMPILE_CHECK([traditional preprocessor],
[#define name2(a,b) a/**/b],[int name2(foo,bar);],
AC_DEFINE(TRADITIONAL_CPP))])dnl
define(GROFF_TIME_T,
[GROFF_CC_COMPILE_CHECK([time_t],[#include <time.h>],
[time_t t = time(0); struct tm *p = localtime(&t);],,
AC_DEFINE(LONG_FOR_TIME_T))])dnl
define(GROFF_OSFCN_H,
[GROFF_CC_COMPILE_CHECK(['C++ <osfcn.h>'],[#include <osfcn.h>],
[read(0, 0, 0); open(0, 0);],AC_DEFINE(HAVE_CC_OSFCN_H))])dnl
dnl Bison generated parsers have problems with C++ compilers other than g++.
dnl So byacc is preferred over bison.
define(GROFF_PROG_YACC,
[AC_PROGRAM_CHECK(YACC, byacc, byacc, )
AC_PROGRAM_CHECK(YACC, bison, bison -y, yacc)
])dnl
dnl GROFF_CSH_HACK(if hack present, if not present)
define(GROFF_CSH_HACK,
[echo 'checking for csh # hack'
cat <<EOF >conftest.sh
#!/bin/sh
true || exit 0
export PATH || exit 0
exit 1
EOF
chmod +x conftest.sh
if echo ./conftest.sh | (csh >/dev/null 2>&1) >/dev/null 2>&1
then
	:; $1
else
	:; $2
fi
rm -f conftest.sh
])dnl
define(GROFF_POSIX,
[GROFF_CC_COMPILE_CHECK(whether -D_POSIX_SOURCE is necessary,
[#include <stdio.h>],
[(void)fileno(stdin);],,
AC_DEFINE(_POSIX_SOURCE))])dnl
dnl From udodo!hans@relay.NL.net (Hans Zuidam)
define(GROFF_ISC_SYSV3,
[echo 'checking for ISC 3.x or 4.x'
changequote(,)dnl
if grep '[34]\.' /usr/options/cb.name >/dev/null 2>&1
changequote([,])dnl
then
	AC_DEFINE(_SYSV3)
fi])dnl
