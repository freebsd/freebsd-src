dnl Autoconf macros for groff.
dnl Copyright (C) 1989, 1990, 1991, 1992, 1995 Free Software Foundation, Inc.
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
dnl Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
dnl
AC_DEFUN(GROFF_PRINT,
[if test -z "$PSPRINT"
then
	AC_CHECK_PROGS(LPR,lpr)
	AC_CHECK_PROGS(LP,lp)
	if test -n "$LPR" && test -n "$LP"
	then
		# HP-UX provides an lpr command that emulates lpr using lp,
		# but it doesn't have lpq; in this case we want to use lp
		# rather than lpr.
		AC_CHECK_PROGS(LPQ,lpq)
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
AC_MSG_CHECKING([for command to use for printing PostScript files])
AC_MSG_RESULT($PSPRINT)
# Figure out DVIPRINT from PSPRINT.
AC_MSG_CHECKING([for command to use for printing dvi files])
if test -n "$PSPRINT" && test -z "$DVIPRINT"
then
	if test "X$PSPRINT" = "Xlpr"
	then
		DVIPRINT="lpr -d"
	else
		DVIPRINT="$PSPRINT"
	fi
fi
AC_SUBST(DVIPRINT)
AC_MSG_RESULT($DVIPRINT)])dnl
dnl Bison generated parsers have problems with C++ compilers other than g++.
dnl So byacc is preferred over bison.
AC_DEFUN(GROFF_PROG_YACC,
[AC_CHECK_PROGS(YACC, byacc 'bison -y', yacc)])
dnl GROFF_CSH_HACK(if hack present, if not present)
AC_DEFUN(GROFF_CSH_HACK,
[AC_MSG_CHECKING([for csh hash hack])
cat <<EOF >conftest.sh
#!/bin/sh
true || exit 0
export PATH || exit 0
exit 1
EOF
chmod +x conftest.sh
if echo ./conftest.sh | (csh >/dev/null 2>&1) >/dev/null 2>&1
then
	AC_MSG_RESULT(yes); $1
else
	AC_MSG_RESULT(no); $2
fi
rm -f conftest.sh
])dnl
dnl From udodo!hans@relay.NL.net (Hans Zuidam)
AC_DEFUN(GROFF_ISC_SYSV3,
[AC_MSG_CHECKING([for ISC 3.x or 4.x])
changequote(,)dnl
if grep '[34]\.' /usr/options/cb.name >/dev/null 2>&1
changequote([,])dnl
then
	AC_MSG_RESULT(yes)
	AC_DEFINE(_SYSV3)
else
	AC_MSG_RESULT(no)
fi])dnl
AC_DEFUN(GROFF_POSIX,
[AC_MSG_CHECKING([whether -D_POSIX_SOURCE is necessary])
AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_TRY_COMPILE([#include <stdio.h>
extern "C" { void fileno(int); }],,
AC_MSG_RESULT(yes);AC_DEFINE(_POSIX_SOURCE),
AC_MSG_RESULT(no))
AC_LANG_RESTORE])dnl
AC_DEFUN(GROFF_GETOPT,
[AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_MSG_CHECKING([declaration of getopt in stdlib.h])
AC_TRY_COMPILE(
[#include <stdlib.h>
extern "C" { void getopt(int); }],,AC_MSG_RESULT(no),
AC_MSG_RESULT(yes);AC_DEFINE(STDLIB_H_DECLARES_GETOPT))
AC_MSG_CHECKING([declaration of getopt in unistd.h])
AC_TRY_COMPILE([#include <sys/types.h>
#include <unistd.h>
extern "C" { void getopt(int); }],,AC_MSG_RESULT(no),
AC_MSG_RESULT(yes);AC_DEFINE(UNISTD_H_DECLARES_GETOPT))
AC_LANG_RESTORE])dnl
AC_DEFUN(GROFF_PUTENV,
[AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_MSG_CHECKING([declaration of putenv])
AC_TRY_COMPILE([#include <stdlib.h>
extern "C" { void putenv(int); }],,AC_MSG_RESULT(no),
AC_MSG_RESULT(yes)
AC_DEFINE(STDLIB_H_DECLARES_PUTENV))
AC_LANG_RESTORE])dnl
AC_DEFUN(GROFF_POPEN,
[AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_MSG_CHECKING([declaration of popen])
AC_TRY_COMPILE([#include <stdio.h>
extern "C" { void popen(int); }],,AC_MSG_RESULT(no),
AC_MSG_RESULT(yes);AC_DEFINE(STDIO_H_DECLARES_POPEN))
AC_LANG_RESTORE])dnl
AC_DEFUN(GROFF_PCLOSE,
[AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_MSG_CHECKING([declaration of pclose])
AC_TRY_COMPILE([#include <stdio.h>
extern "C" { void pclose(int); }],,AC_MSG_RESULT(no),
AC_MSG_RESULT(yes);AC_DEFINE(STDIO_H_DECLARES_PCLOSE))
AC_LANG_RESTORE])dnl
AC_DEFUN(GROFF_OSFCN_H,
[AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_MSG_CHECKING([C++ <osfcn.h>])
AC_TRY_COMPILE([#include <osfcn.h>],
[read(0, 0, 0); open(0, 0);],AC_MSG_RESULT(yes);AC_DEFINE(HAVE_CC_OSFCN_H),
AC_MSG_RESULT(no))
AC_LANG_RESTORE])dnl
AC_DEFUN(GROFF_LIMITS_H,
[AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_MSG_CHECKING([C++ <limits.h>])
AC_TRY_COMPILE([#include <limits.h>],
[int x = INT_MIN; int y = INT_MAX; int z = UCHAR_MAX;],
AC_MSG_RESULT(yes);AC_DEFINE(HAVE_CC_LIMITS_H),AC_MSG_RESULT(no))
AC_LANG_RESTORE])dnl
AC_DEFUN(GROFF_TIME_T,
[AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_MSG_CHECKING([for declaration of time_t])
AC_TRY_COMPILE([#include <time.h>],
[time_t t = time(0); struct tm *p = localtime(&t);],AC_MSG_RESULT(yes),
AC_MSG_RESULT(no);AC_DEFINE(LONG_FOR_TIME_T))
AC_LANG_RESTORE])dnl
AC_DEFUN(GROFF_STRUCT_EXCEPTION,
[AC_MSG_CHECKING([struct exception])
AC_TRY_COMPILE([#include <math.h>],
[struct exception e;],
AC_MSG_RESULT(yes);AC_DEFINE(HAVE_STRUCT_EXCEPTION),
AC_MSG_RESULT(no))])dnl
AC_DEFUN(GROFF_ARRAY_DELETE,
[AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_MSG_CHECKING([whether ANSI array delete syntax supported])
AC_TRY_COMPILE(,
changequote(,)dnl
char *p = new char[5]; delete [] p;changequote([,]),
AC_MSG_RESULT(yes),AC_MSG_RESULT(no);AC_DEFINE(ARRAY_DELETE_NEEDS_SIZE))
AC_LANG_RESTORE])dnl
dnl
AC_DEFUN(GROFF_TRADITIONAL_CPP,
[AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_MSG_CHECKING([traditional preprocessor])
AC_TRY_COMPILE([#define name2(a,b) a/**/b],[int name2(foo,bar);],
AC_MSG_RESULT(yes);AC_DEFINE(TRADITIONAL_CPP),
AC_MSG_RESULT(no))
AC_LANG_RESTORE])dnl

AC_DEFUN(GROFF_WCOREFLAG,
[AC_MSG_CHECKING([w_coredump])
AC_TRY_RUN([
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
],AC_MSG_RESULT(yes);AC_DEFINE(WCOREFLAG,0200),AC_MSG_RESULT(no),
AC_MSG_RESULT(no))])dnl
dnl
AC_DEFUN(GROFF_BROKEN_SPOOLER_FLAGS,
[AC_MSG_CHECKING([default value for grops -b option])
test -n "${BROKEN_SPOOLER_FLAGS}" || BROKEN_SPOOLER_FLAGS=7
AC_MSG_RESULT($BROKEN_SPOOLER_FLAGS)
AC_SUBST(BROKEN_SPOOLER_FLAGS)])dnl
dnl
AC_DEFUN(GROFF_PAGE,
[AC_MSG_CHECKING([default paper size])
if test -z "$PAGE"
then
	descfile=
	if test -r $prefix/share/groff/font/devps/DESC
	then
		descfile=$prefix/share/groff/font/devps/DESC
	elif test -r $prefix/lib/groff/font/devps/DESC
	then
		descfile=$prefix/lib/groff/font/devps/DESC
	fi
	if test -n "$descfile" \
	  && grep "^paperlength 841890" $descfile >/dev/null 2>&1
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
AC_MSG_RESULT($PAGE)
AC_SUBST(PAGE)])dnl
dnl
AC_DEFUN(GROFF_CXX_CHECK,
[AC_REQUIRE([AC_C_CROSS])
AC_REQUIRE([AC_PROG_CXX])
AC_LANG_SAVE
AC_LANG_CPLUSPLUS
if test "$cross_compiling" = no; then
AC_MSG_CHECKING([that C++ compiler can compile simple program])
fi
AC_TRY_RUN([int main() { return 0; }],
AC_MSG_RESULT(yes),
AC_MSG_RESULT(no)
AC_MSG_ERROR([a working C++ compiler is required]),:)
if test "$cross_compiling" = no; then
AC_MSG_CHECKING([that C++ static constructors and destructors are called])
fi
AC_TRY_RUN([
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
],
AC_MSG_RESULT(yes),
AC_MSG_RESULT(no)
AC_MSG_ERROR([a working C++ compiler is required]),:)
AC_MSG_CHECKING([that header files support C++])
AC_TRY_LINK([#include <stdio.h>],
[fopen(0, 0);],AC_MSG_RESULT(yes),
AC_MSG_RESULT(no)
AC_MSG_ERROR([header files do not support C++ (if you are using a version of gcc/g++ earlier than 2.5, you should install libg++)]))
AC_LANG_RESTORE
])dnl
dnl
AC_DEFUN(GROFF_TMAC,
[
AC_MSG_CHECKING([for prefix of system macro packages])
sys_tmac_prefix=
sys_tmac_file_prefix=
for d in /usr/share/lib/tmac /usr/lib/tmac
do
	for t in "" tmac.
	do
		for m in an s m
		do
			f=$d/$t$m
			if test -z "$sys_tmac_prefix" \
			  && test -f $f \
			  && grep '^\.if' $f >/dev/null 2>&1
			then
				sys_tmac_prefix=$d/$t
				sys_tmac_file_prefix=$t
			fi
		done
	done
done
AC_MSG_RESULT($sys_tmac_prefix)
AC_SUBST(sys_tmac_prefix)
tmac_wrap=
AC_MSG_CHECKING([which system macro packages should be made available])
if test "x$sys_tmac_file_prefix" = "xtmac."
then
	for f in $sys_tmac_prefix*
	do
		suff=`echo $f | sed -e "s;$sys_tmac_prefix;;"`
		case "$suff" in
		e);;
		*)
		grep "Copyright.*Free Software Foundation" $f >/dev/null \
		  || tmac_wrap="$tmac_wrap $suff"
		;;
		esac 
	done
elif test -n "$sys_tmac_prefix"
then
	files=`echo $sys_tmac_prefix*`
	grep "\\.so" $files >conftest.sol
	for f in $files
	do
		case "$f" in
		${sys_tmac_prefix}e) ;;
		*.me) ;;
		*/ms.*) ;;
		*)
		b=`basename $f`
		if grep "\\.so.*/$b\$" conftest.sol >/dev/null \
		  || grep -l "Copyright.*Free Software Foundation" $f >/dev/null
		then
			:
		else
			suff=`echo $f | sed -e "s;$sys_tmac_prefix;;"`
			case "$suff" in
			tmac.*);;
			*) tmac_wrap="$tmac_wrap $suff" ;;
			esac
		fi
		esac
	done
	rm -f conftest.sol
fi
AC_MSG_RESULT([$tmac_wrap])
AC_SUBST(tmac_wrap)
])dnl
AC_DEFUN(GROFF_G,
[AC_MSG_CHECKING([for existing troff installation])
if test "x`(echo .tm '|n(.g' | tr '|' '\\\\' | troff -z -i 2>&1) 2>/dev/null`" \
  = x0
then
	AC_MSG_RESULT(yes)
	g=g
else
	AC_MSG_RESULT(no)
	g=
fi
AC_SUBST(g)
])dnl
dnl We need the path to install-sh to be absolute.
AC_DEFUN(GROFF_INSTALL_SH,
[AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT])dnl
ac_dir=`cd $ac_aux_dir; pwd`
ac_install_sh="$ac_dir/install-sh -c"
])
