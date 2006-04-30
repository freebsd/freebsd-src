# Autoconf macros for groff.
# Copyright (C) 1989-1995, 2001, 2002, 2003, 2004, 2005
# Free Software Foundation, Inc.
#
# This file is part of groff.
#
# groff is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2, or (at your option) any later
# version.
#
# groff is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with groff; see the file COPYING.  If not, write to the Free Software
# Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA.

AC_DEFUN([GROFF_PRINT],
  [if test -z "$PSPRINT"; then
     AC_CHECK_PROGS([LPR], [lpr])
     AC_CHECK_PROGS([LP], [lp])
     if test -n "$LPR" && test -n "$LP"; then
       # HP-UX provides an lpr command that emulates lpr using lp,
       # but it doesn't have lpq; in this case we want to use lp
       # rather than lpr.
       AC_CHECK_PROGS([LPQ], [lpq])
       test -n "$LPQ" || LPR=
     fi
     if test -n "$LPR"; then
       PSPRINT="$LPR"
     elif test -n "$LP"; then
       PSPRINT="$LP"
     fi
   fi
   AC_SUBST([PSPRINT])
   AC_MSG_CHECKING([for command to use for printing PostScript files])
   AC_MSG_RESULT([$PSPRINT])

   # Figure out DVIPRINT from PSPRINT.
   AC_MSG_CHECKING([for command to use for printing dvi files])
   if test -n "$PSPRINT" && test -z "$DVIPRINT"; then
     if test "x$PSPRINT" = "xlpr"; then
       DVIPRINT="lpr -d"
     else
       DVIPRINT="$PSPRINT"
     fi
   fi
   AC_SUBST([DVIPRINT])
   AC_MSG_RESULT([$DVIPRINT])])

# Bison generated parsers have problems with C++ compilers other than g++.
# So byacc is preferred over bison.

AC_DEFUN([GROFF_PROG_YACC],
  [AC_CHECK_PROGS([YACC], [byacc 'bison -y'], [yacc])])

# The following programs are needed for grohtml.

AC_DEFUN([GROFF_HTML_PROGRAMS],
  [AC_REQUIRE([GROFF_GHOSTSCRIPT_PATH])
   make_html=html
   make_install_html=install_html

   missing=
   AC_FOREACH([groff_prog],
     [pnmcut pnmcrop pnmtopng psselect pnmtops],
     [AC_CHECK_PROG(groff_prog, groff_prog, [found], [missing])
      if test $[]groff_prog = missing; then
	missing="$missing \`groff_prog'"
      fi;])

   test "$GHOSTSCRIPT" = "missing" && missing="$missing \`gs'"

   if test -n "$missing"; then
     plural=`set $missing; test $[#] -gt 1 && echo s`
     missing=`set $missing
       missing=""
       while test $[#] -gt 0
	 do
	   case $[#] in
	     1) missing="$missing$[1]" ;;
	     2) missing="$missing$[1] and " ;;
	     *) missing="$missing$[1], " ;;
	   esac
	   shift
	 done
	 echo $missing`

     make_html=
     make_install_html=

     AC_MSG_WARN([missing program$plural:

  The program$plural
     $missing
  cannot be found in the PATH.
  Consequently, groff's HTML backend (grohtml) will not work properly;
  therefore, it will neither be possible to prepare, nor to install,
  documentation in HTML format.
     ])
   fi

   AC_SUBST([make_html])
   AC_SUBST([make_install_html])])

# To produce PDF docs, we need both awk and ghostscript.

AC_DEFUN([GROFF_PDFDOC_PROGRAMS],
  [AC_REQUIRE([GROFF_AWK_PATH])
   AC_REQUIRE([GROFF_GHOSTSCRIPT_PATH])

   make_pdfdoc=pdfdoc
   make_install_pdfdoc=install_pdfdoc

   missing=""
   test "$AWK" = missing && missing="\`awk'"
   test "$GHOSTSCRIPT" = missing && missing="$missing \`gs'"
   if test -n "$missing"; then
     plural=`set $missing; test $[#] -eq 2 && echo s`
     test x$plural = xs \
       && missing=`set $missing; echo "$[1] and $[2]"` \
       || missing=`echo $missing`

     make_pdfdoc=
     make_install_pdfdoc=

     AC_MSG_WARN([missing program$plural:

  The program$plural $missing cannot be found in the PATH.
  Consequently, groff's PDF formatter (pdfroff) will not work properly;
  therefore, it will neither be possible to prepare, nor to install,
  documentation in PDF format.
     ])
   fi

   AC_SUBST([make_pdfdoc])
   AC_SUBST([make_install_pdfdoc])])

# Check whether pnmtops can handle the -nosetpage option.

AC_DEFUN([GROFF_PNMTOPS_NOSETPAGE],
  [AC_MSG_CHECKING([whether pnmtops can handle the -nosetpage option])
   if echo P2 2 2 255 0 1 2 0 | pnmtops -nosetpage > /dev/null 2>&1 ; then
     AC_MSG_RESULT([yes])
     pnmtops_nosetpage="pnmtops -nosetpage"
   else
     AC_MSG_RESULT([no])
     pnmtops_nosetpage="pnmtops"
   fi
   AC_SUBST([pnmtops_nosetpage])])

# Check location of `gs'; allow `--with-gs=PROG' option to override.

AC_DEFUN([GROFF_GHOSTSCRIPT_PATH],
  [AC_REQUIRE([GROFF_GHOSTSCRIPT_PREFS])
   AC_ARG_WITH([gs],
     [AS_HELP_STRING([--with-gs=PROG],
       [actual [/path/]name of ghostscript executable])],
     [GHOSTSCRIPT=$withval],
     [AC_CHECK_TOOLS(GHOSTSCRIPT, [$ALT_GHOSTSCRIPT_PROGS], [missing])])
   test "$GHOSTSCRIPT" = "no" && GHOSTSCRIPT=missing])

# Preferences for choice of `gs' program...
# (allow --with-alt-gs="LIST" to override).

AC_DEFUN([GROFF_GHOSTSCRIPT_PREFS],
  [AC_ARG_WITH([alt-gs],
    [AS_HELP_STRING([--with-alt-gs=LIST],
      [alternative names for ghostscript executable])],
    [ALT_GHOSTSCRIPT_PROGS="$withval"],
    [ALT_GHOSTSCRIPT_PROGS="gs gswin32c gsos2"])
   AC_SUBST([ALT_GHOSTSCRIPT_PROGS])])

# Check location of `awk'; allow `--with-awk=PROG' option to override.

AC_DEFUN([GROFF_AWK_PATH],
  [AC_REQUIRE([GROFF_AWK_PREFS])
   AC_ARG_WITH([awk],
     [AS_HELP_STRING([--with-awk=PROG],
       [actual [/path/]name of awk executable])],
     [AWK=$withval],
     [AC_CHECK_TOOLS(AWK, [$ALT_AWK_PROGS], [missing])])
   test "$AWK" = "no" && AWK=missing])

# Preferences for choice of `awk' program; allow --with-alt-awk="LIST"
# to override.

AC_DEFUN([GROFF_AWK_PREFS],
  [AC_ARG_WITH([alt-awk],
    [AS_HELP_STRING([--with-alt-awk=LIST],
      [alternative names for awk executable])],
    [ALT_AWK_PROGS="$withval"],
    [ALT_AWK_PROGS="gawk mawk nawk awk"])
   AC_SUBST([ALT_AWK_PROGS])])

# GROFF_CSH_HACK(if hack present, if not present)

AC_DEFUN([GROFF_CSH_HACK],
  [AC_MSG_CHECKING([for csh hash hack])

cat <<EOF >conftest.sh
#! /bin/sh
true || exit 0
export PATH || exit 0
exit 1
EOF

   chmod +x conftest.sh
   if echo ./conftest.sh | (csh >/dev/null 2>&1) >/dev/null 2>&1; then
     AC_MSG_RESULT([yes])
     $1
   else
     AC_MSG_RESULT([no])
     $2
   fi
   rm -f conftest.sh])

# From udodo!hans@relay.NL.net (Hans Zuidam)

AC_DEFUN([GROFF_ISC_SYSV3],
  [AC_MSG_CHECKING([for ISC 3.x or 4.x])
   if grep ['[34]\.'] /usr/options/cb.name >/dev/null 2>&1
   then
     AC_MSG_RESULT([yes])
     AC_DEFINE([_SYSV3], [1], [Define if you have ISC 3.x or 4.x.])
   else
     AC_MSG_RESULT([no])
   fi])

AC_DEFUN([GROFF_POSIX],
  [AC_MSG_CHECKING([whether -D_POSIX_SOURCE is necessary])
   AC_LANG_PUSH([C++])
   AC_COMPILE_IFELSE([
       AC_LANG_PROGRAM([[

#include <stdio.h>
extern "C" { void fileno(int); }

       ]])
     ],
     [AC_MSG_RESULT([yes])
      AC_DEFINE([_POSIX_SOURCE], [1],
	[Define if -D_POSIX_SOURCE is necessary.])],
     [AC_MSG_RESULT([no])])
   AC_LANG_POP([C++])])

# srand() of SunOS 4.1.3 has return type int instead of void

AC_DEFUN([GROFF_SRAND],
  [AC_LANG_PUSH([C++])
   AC_MSG_CHECKING([for return type of srand])
   AC_COMPILE_IFELSE([
       AC_LANG_PROGRAM([[

#include <stdlib.h>
extern "C" { void srand(unsigned int); }

       ]])
     ],
     [AC_MSG_RESULT([void])
      AC_DEFINE([RET_TYPE_SRAND_IS_VOID], [1],
	[Define if srand() returns void not int.])],
     [AC_MSG_RESULT([int])])
   AC_LANG_POP([C++])])

# In April 2005, autoconf's AC_TYPE_SIGNAL is still broken.

AC_DEFUN([GROFF_TYPE_SIGNAL],
  [AC_MSG_CHECKING([for return type of signal handlers])
   for groff_declaration in \
     'extern "C" void (*signal (int, void (*)(int)))(int);' \
     'extern "C" void (*signal (int, void (*)(int)) throw ())(int);' \
     'void (*signal ()) ();' 
   do
     AC_COMPILE_IFELSE([
	 AC_LANG_PROGRAM([[

#include <sys/types.h>
#include <signal.h>
#ifdef signal
# undef signal
#endif
$groff_declaration

	 ]],
	 [[

int i;

	 ]])
       ],
       [break],
       [continue])
   done

   if test -n "$groff_declaration"; then
     AC_MSG_RESULT([void])
     AC_DEFINE([RETSIGTYPE], [void],
       [Define as the return type of signal handlers
	(`int' or `void').])
   else
     AC_MSG_RESULT([int])
     AC_DEFINE([RETSIGTYPE], [int],
       [Define as the return type of signal handlers
	(`int' or `void').])
   fi])

AC_DEFUN([GROFF_SYS_NERR],
  [AC_LANG_PUSH([C++])
   AC_MSG_CHECKING([for sys_nerr in <errno.h>, <stdio.h>, or <stdlib.h>])
   AC_COMPILE_IFELSE([
       AC_LANG_PROGRAM([[

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

       ]],
       [[

int k;
k = sys_nerr;

       ]])
     ],
     [AC_MSG_RESULT([yes])
      AC_DEFINE([HAVE_SYS_NERR], [1],
	[Define if you have sys_nerr in <errno.h>, <stdio.h>, or <stdio.h>.])],
     [AC_MSG_RESULT([no])])
   AC_LANG_POP([C++])])

AC_DEFUN([GROFF_SYS_ERRLIST],
  [AC_MSG_CHECKING([for sys_errlist[] in <errno.h>, <stdio.h>, or <stdlib.h>])
   AC_COMPILE_IFELSE([
       AC_LANG_PROGRAM([[

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

       ]],
       [[

int k;
k = (int)sys_errlist[0];

       ]])
     ],
     [AC_MSG_RESULT([yes])
      AC_DEFINE([HAVE_SYS_ERRLIST], [1],
	[Define if you have sys_errlist in <errno.h>, <stdio.h>, or <stdlib.h>.])],
     [AC_MSG_RESULT([no])])])

AC_DEFUN([GROFF_OSFCN_H],
  [AC_LANG_PUSH([C++])
   AC_MSG_CHECKING([C++ <osfcn.h>])
   AC_COMPILE_IFELSE([
       AC_LANG_PROGRAM([[

#include <osfcn.h>

       ]],
       [[

read(0, 0, 0);
open(0, 0);

       ]])
     ],
     [AC_MSG_RESULT([yes])
      AC_DEFINE([HAVE_CC_OSFCN_H], [1],
	[Define if you have a C++ <osfcn.h>.])],
     [AC_MSG_RESULT([no])])
   AC_LANG_POP([C++])])

AC_DEFUN([GROFF_LIMITS_H],
  [AC_LANG_PUSH([C++])
   AC_MSG_CHECKING([C++ <limits.h>])
   AC_COMPILE_IFELSE([
       AC_LANG_PROGRAM([[

#include <limits.h>

       ]],
       [[

int x = INT_MIN;
int y = INT_MAX;
int z = UCHAR_MAX;

       ]])
     ],
     [AC_MSG_RESULT([yes])
      AC_DEFINE([HAVE_CC_LIMITS_H], [1],
	[Define if you have a C++ <limits.h>.])],
     [AC_MSG_RESULT([no])])
   AC_LANG_POP([C++])])

AC_DEFUN([GROFF_TIME_T],
  [AC_LANG_PUSH([C++])
   AC_MSG_CHECKING([for declaration of time_t])
   AC_COMPILE_IFELSE([
       AC_LANG_PROGRAM([[

#include <time.h>

       ]],
       [[

time_t t = time(0);
struct tm *p = localtime(&t);

       ]])
     ],
     [AC_MSG_RESULT([yes])],
     [AC_MSG_RESULT([no])
      AC_DEFINE([LONG_FOR_TIME_T], [1],
	[Define if localtime() takes a long * not a time_t *.])])
   AC_LANG_POP([C++])])

AC_DEFUN([GROFF_STRUCT_EXCEPTION],
  [AC_MSG_CHECKING([struct exception])
   AC_COMPILE_IFELSE([
       AC_LANG_PROGRAM([[

#include <math.h>

       ]],
       [[

struct exception e;

       ]])
     ],
     [AC_MSG_RESULT([yes])
      AC_DEFINE([HAVE_STRUCT_EXCEPTION], [1],
	[Define if <math.h> defines struct exception.])],
     [AC_MSG_RESULT([no])])])

AC_DEFUN([GROFF_ARRAY_DELETE],
  [AC_LANG_PUSH([C++])
   AC_MSG_CHECKING([whether ANSI array delete syntax is supported])
   AC_COMPILE_IFELSE([
       AC_LANG_PROGRAM(, [[

char *p = new char[5];
delete [] p;

       ]])
     ],
     [AC_MSG_RESULT([yes])],
     [AC_MSG_RESULT([no])
      AC_DEFINE([ARRAY_DELETE_NEEDS_SIZE], [1],
	[Define if your C++ doesn't understand `delete []'.])])
   AC_LANG_POP([C++])])

AC_DEFUN([GROFF_TRADITIONAL_CPP],
  [AC_LANG_PUSH([C++])
   AC_MSG_CHECKING([traditional preprocessor])
   AC_COMPILE_IFELSE([
       AC_LANG_PROGRAM([[

#define name2(a, b) a/**/b

       ]],
       [[

int name2(foo, bar);

       ]])
     ],
     [AC_MSG_RESULT([yes])
      AC_DEFINE([TRADITIONAL_CPP], [1],
	[Define if your C++ compiler uses a traditional (Reiser) preprocessor.])],
     [AC_MSG_RESULT([no])])
   AC_LANG_POP([C++])])

AC_DEFUN([GROFF_WCOREFLAG],
  [AC_MSG_CHECKING([w_coredump])
   AC_RUN_IFELSE([
       AC_LANG_PROGRAM([[

#include <sys/types.h>
#include <sys/wait.h>

       ]],
       [[

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

       ]])
     ],
     [AC_MSG_RESULT([yes])
      AC_DEFINE(WCOREFLAG, 0200,
	[Define if the 0200 bit of the status returned by wait() indicates
	 whether a core image was produced for a process that was terminated
	 by a signal.])],
     [AC_MSG_RESULT([no])],
     [AC_MSG_RESULT([no])])])

AC_DEFUN([GROFF_BROKEN_SPOOLER_FLAGS],
  [AC_MSG_CHECKING([default value for grops -b option])
   test -n "${BROKEN_SPOOLER_FLAGS}" || BROKEN_SPOOLER_FLAGS=0
   AC_MSG_RESULT([$BROKEN_SPOOLER_FLAGS])
   AC_SUBST([BROKEN_SPOOLER_FLAGS])])

AC_DEFUN([GROFF_PAGE],
  [AC_MSG_CHECKING([default paper size])
   groff_prefix=$prefix
   test "x$prefix" = "xNONE" && groff_prefix=$ac_default_prefix
   if test -z "$PAGE"; then
     descfile=
     if test -r $groff_prefix/share/groff/font/devps/DESC; then
       descfile=$groff_prefix/share/groff/font/devps/DESC
     elif test -r $groff_prefix/lib/groff/font/devps/DESC; then
       descfile=$groff_prefix/lib/groff/font/devps/DESC
     else
       for f in $groff_prefix/share/groff/*/font/devps/DESC; do
	 if test -r $f; then
	   descfile=$f
	   break
	 fi
       done
     fi

     if test -n "$descfile"; then
       if grep ['^paperlength[	 ]\+841890'] $descfile >/dev/null 2>&1; then
	 PAGE=A4
       elif grep ['^papersize[	 ]\+[aA]4'] $descfile >/dev/null 2>&1; then
	 PAGE=A4
       fi
     fi
   fi

   if test -z "$PAGE"; then
     dom=`awk '([$]1 == "dom" || [$]1 == "search") { print [$]2; exit}' \
	 /etc/resolv.conf 2>/dev/null`
     if test -z "$dom"; then
       dom=`(domainname) 2>/dev/null | tr -d '+'`
       if test -z "$dom" \
	  || test "$dom" = '(none)'; then
	 dom=`(hostname) 2>/dev/null | grep '\.'`
       fi
     fi
     # If the top-level domain is two letters and it's not `us' or `ca'
     # then they probably use A4 paper.
     case "$dom" in
     [*.[Uu][Ss]|*.[Cc][Aa])]
       ;;
     [*.[A-Za-z][A-Za-z])]
       PAGE=A4 ;;
     esac
   fi

   test -n "$PAGE" || PAGE=letter
   if test "x$PAGE" = "xA4"; then
     AC_DEFINE([PAGEA4], [1],
       [Define if the printer's page size is A4.])
   fi
   AC_MSG_RESULT([$PAGE])
   AC_SUBST([PAGE])])

AC_DEFUN([GROFF_CXX_CHECK],
  [AC_REQUIRE([AC_PROG_CXX])
   AC_LANG_PUSH([C++])
   if test "$cross_compiling" = no; then
     AC_MSG_CHECKING([that C++ compiler can compile simple program])
   fi
   AC_RUN_IFELSE([
       AC_LANG_SOURCE([[

int main() {
  return 0;
}

       ]])
     ],
     [AC_MSG_RESULT([yes])],
     [AC_MSG_RESULT([no])
      AC_MSG_ERROR([a working C++ compiler is required])],
     [:])

   if test "$cross_compiling" = no; then
     AC_MSG_CHECKING([that C++ static constructors and destructors are called])
   fi
   AC_RUN_IFELSE([
       AC_LANG_SOURCE([[

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

int main()
{
  return 1;
}

       ]])
     ],
     [AC_MSG_RESULT([yes])],
     [AC_MSG_RESULT([no])
      AC_MSG_ERROR([a working C++ compiler is required])],
     [:])

   AC_MSG_CHECKING([that header files support C++])
   AC_LINK_IFELSE([
       AC_LANG_PROGRAM([[

#include <stdio.h>

       ]],
       [[

fopen(0, 0);

       ]])
     ],
     [AC_MSG_RESULT([yes])],
     [AC_MSG_RESULT([no])
      AC_MSG_ERROR([header files do not support C++
		   (if you are using a version of gcc/g++ earlier than 2.5,
		   you should install libg++)])])
   AC_LANG_POP([C++])])

AC_DEFUN([GROFF_TMAC],
  [AC_MSG_CHECKING([for prefix of system macro packages])
   sys_tmac_prefix=
   sys_tmac_file_prefix=
   for d in /usr/share/lib/tmac /usr/lib/tmac; do
     for t in "" tmac.; do
       for m in an s m; do
	 f=$d/$t$m
	 if test -z "$sys_tmac_prefix" \
	    && test -f $f \
	    && grep '^\.if' $f >/dev/null 2>&1; then
	   sys_tmac_prefix=$d/$t
	   sys_tmac_file_prefix=$t
	 fi
       done
     done
   done
   AC_MSG_RESULT([$sys_tmac_prefix])
   AC_SUBST([sys_tmac_prefix])

   AC_MSG_CHECKING([which system macro packages should be made available])
   tmac_wrap=
   if test "x$sys_tmac_file_prefix" = "xtmac."; then
     for f in $sys_tmac_prefix*; do
       suff=`echo $f | sed -e "s;$sys_tmac_prefix;;"`
       case "$suff" in
       e)
	 ;;
       *)
	 grep "Copyright.*Free Software Foundation" $f >/dev/null \
	      || tmac_wrap="$tmac_wrap $suff" ;;
       esac
     done
   elif test -n "$sys_tmac_prefix"; then
     files=`echo $sys_tmac_prefix*`
     grep "\\.so" $files >conftest.sol
     for f in $files; do
       case "$f" in
       ${sys_tmac_prefix}e)
	 ;;
       *.me)
	 ;;
       */ms.*)
	 ;;
       *)
	 b=`basename $f`
	 if grep "\\.so.*/$b\$" conftest.sol >/dev/null \
	    || grep -l "Copyright.*Free Software Foundation" $f >/dev/null; then
	   :
	 else
	   suff=`echo $f | sed -e "s;$sys_tmac_prefix;;"`
	   case "$suff" in
	   tmac.*)
	     ;;
	   *)
	     tmac_wrap="$tmac_wrap $suff" ;;
	   esac
	 fi
       esac
     done
     rm -f conftest.sol
   fi
   AC_MSG_RESULT([$tmac_wrap])
   AC_SUBST([tmac_wrap])])

AC_DEFUN([GROFF_G],
  [AC_MSG_CHECKING([for existing troff installation])
   if test "x`(echo .tm '|n(.g' | tr '|' '\\\\' | troff -z -i 2>&1) 2>/dev/null`" = x0; then
     AC_MSG_RESULT([yes])
     g=g
   else
     AC_MSG_RESULT([no])
     g=
   fi
   AC_SUBST([g])])

# We need the path to install-sh to be absolute.

AC_DEFUN([GROFF_INSTALL_SH],
  [AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT])
   ac_dir=`cd $ac_aux_dir; pwd`
   ac_install_sh="$ac_dir/install-sh -c"])

# Test whether install-info is available.

AC_DEFUN([GROFF_INSTALL_INFO],
  [AC_CHECK_PROGS([INSTALL_INFO], [install-info], [:])])

# At least one UNIX system, Apple Macintosh Rhapsody 5.5,
# does not have -lm ...

AC_DEFUN([GROFF_LIBM],
  [AC_CHECK_LIB([m], [sin], [LIBM=-lm])
   AC_SUBST([LIBM])])

# ... while the MinGW implementation of GCC for Microsoft Win32
# does not seem to have -lc.

AC_DEFUN([GROFF_LIBC],
  [AC_CHECK_LIB([c], [main], [LIBC=-lc])
   AC_SUBST([LIBC])])

# Check for EBCDIC -- stolen from the OS390 Unix LYNX port

AC_DEFUN([GROFF_EBCDIC],
  [AC_MSG_CHECKING([whether character set is EBCDIC])
   AC_COMPILE_IFELSE([
       AC_LANG_PROGRAM([[

/* Treat any failure as ASCII for compatibility with existing art.
   Use compile-time rather than run-time tests for cross-compiler
   tolerance. */
#if '0' != 240
make an error "Character set is not EBCDIC"
#endif

       ]])
     ],
     [groff_cv_ebcdic="yes"
      TTYDEVDIRS="font/devcp1047"
      AC_MSG_RESULT([yes])
      AC_DEFINE(IS_EBCDIC_HOST, 1,
	[Define if the host's encoding is EBCDIC.])],
     [groff_cv_ebcdic="no"
     TTYDEVDIRS="font/devascii font/devlatin1"
     OTHERDEVDIRS="font/devlj4 font/devlbp"
     AC_MSG_RESULT([no])])
   AC_SUBST([TTYDEVDIRS])
   AC_SUBST([OTHERDEVDIRS])])

# Check for OS/390 Unix.  We test for EBCDIC also -- the Linux port (with
# gcc) to OS/390 uses ASCII internally.

AC_DEFUN([GROFF_OS390],
  [if test "$groff_cv_ebcdic" = "yes"; then
     AC_MSG_CHECKING([for OS/390 Unix])
     case `uname` in
     OS/390)
       CFLAGS="$CFLAGS -D_ALL_SOURCE"
       AC_MSG_RESULT([yes]) ;;
     *)
       AC_MSG_RESULT([no]) ;;
     esac
   fi])

# Check whether we need a declaration for a function.
#
# Stolen from GNU bfd.

AC_DEFUN([GROFF_NEED_DECLARATION],
  [AC_MSG_CHECKING([whether $1 must be declared])
   AC_LANG_PUSH([C++])
   AC_CACHE_VAL([groff_cv_decl_needed_$1],
     [AC_COMPILE_IFELSE([
	  AC_LANG_PROGRAM([[

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif

	  ]],
	  [[

#ifndef $1
  char *p = (char *) $1;
#endif

	  ]])
      ],
      [groff_cv_decl_needed_$1=no],
      [groff_cv_decl_needed_$1=yes])])
   AC_MSG_RESULT([$groff_cv_decl_needed_$1])
   if test $groff_cv_decl_needed_$1 = yes; then
     AC_DEFINE([NEED_DECLARATION_]translit($1, [a-z], [A-Z]), [1],
       [Define if your C++ doesn't declare ]$1[().])
   fi
   AC_LANG_POP([C++])])

# If mkstemp() isn't available, use our own mkstemp.cpp file.

AC_DEFUN([GROFF_MKSTEMP],
  [AC_MSG_CHECKING([for mkstemp])
   AC_LANG_PUSH([C++])
   AC_LIBSOURCE([mkstemp.cpp])
   AC_LINK_IFELSE([
       AC_LANG_PROGRAM([[

#include <stdlib.h>
#include <unistd.h>
int (*f) (char *);

       ]],
       [[

f = mkstemp;

       ]])
     ],
     [AC_MSG_RESULT([yes])
      AC_DEFINE([HAVE_MKSTEMP], [1], [Define if you have mkstemp().])],
     [AC_MSG_RESULT([no])
      _AC_LIBOBJ([mkstemp])])
   AC_LANG_POP([C++])])

# Test whether <inttypes.h> exists, doesn't clash with <sys/types.h>,
# and declares uintmax_t.  Taken from the fileutils package.

AC_DEFUN([GROFF_INTTYPES_H],
  [AC_LANG_PUSH([C++])
   AC_MSG_CHECKING([C++ <inttypes.h>])
   AC_COMPILE_IFELSE([
       AC_LANG_PROGRAM([[

#include <sys/types.h>
#include <inttypes.h>

       ]],
       [[

uintmax_t i = (uintmax_t)-1;

       ]])
     ],
     [groff_cv_header_inttypes_h=yes
      AC_DEFINE([HAVE_CC_INTTYPES_H], [1],
	[Define if you have a C++ <inttypes.h>.])],
     [groff_cv_header_inttypes_h=no])
   AC_MSG_RESULT([$groff_cv_header_inttypes_h])
   AC_LANG_POP([C++])])

# Test for working `unsigned long long'.  Taken from the fileutils package.

AC_DEFUN([GROFF_UNSIGNED_LONG_LONG],
  [AC_LANG_PUSH([C++])
   AC_MSG_CHECKING([for unsigned long long])
   AC_LINK_IFELSE([
       AC_LANG_PROGRAM([[

unsigned long long ull = 1;
int i = 63;
unsigned long long ullmax = (unsigned long long)-1;

       ]],
       [[

return ull << i | ull >> i | ullmax / ull | ullmax % ull;

       ]])
     ],
     [groff_cv_type_unsigned_long_long=yes],
     [groff_cv_type_unsigned_long_long=no])
   AC_MSG_RESULT([$groff_cv_type_unsigned_long_long])
   AC_LANG_POP([C++])])

# Define uintmax_t to `unsigned long' or `unsigned long long'
# if <inttypes.h> does not exist.  Taken from the fileutils package.

AC_DEFUN([GROFF_UINTMAX_T],
  [AC_REQUIRE([GROFF_INTTYPES_H])
   if test $groff_cv_header_inttypes_h = no; then
     AC_REQUIRE([GROFF_UNSIGNED_LONG_LONG])
     test $groff_cv_type_unsigned_long_long = yes \
	  && ac_type='unsigned long long' \
	  || ac_type='unsigned long'
     AC_DEFINE_UNQUOTED([uintmax_t], [$ac_type],
       [Define uintmax_t to `unsigned long' or `unsigned long long' if
	<inttypes.h> does not exist.])
   fi])

# Identify PATH_SEPARATOR character to use in GROFF_FONT_PATH and
# GROFF_TMAC_PATH which is appropriate for the target system (POSIX=':',
# MS-DOS/Win32=';').
#
# The logic to resolve this test is already encapsulated in
# `${srcdir}/src/include/nonposix.h'.

AC_DEFUN([GROFF_TARGET_PATH_SEPARATOR],
  [AC_MSG_CHECKING([separator character to use in groff search paths])
   cp ${srcdir}/src/include/nonposix.h conftest.h
   AC_COMPILE_IFELSE([
       AC_LANG_PROGRAM([[
	
#include <ctype.h>
#include "conftest.h"

       ]],
       [[

#if PATH_SEP_CHAR == ';'
make an error "Path separator is ';'"
#endif

       ]])
     ],
     [GROFF_PATH_SEPARATOR=":"],
     [GROFF_PATH_SEPARATOR=";"])
   AC_MSG_RESULT([$GROFF_PATH_SEPARATOR])
   AC_SUBST(GROFF_PATH_SEPARATOR)])

# Check for X11.

AC_DEFUN([GROFF_X11],
  [AC_REQUIRE([AC_PATH_XTRA])
   groff_no_x=$no_x
   if test -z "$groff_no_x"; then
     OLDCFLAGS=$CFLAGS
     OLDLDFLAGS=$LDFLAGS
     OLDLIBS=$LIBS
     CFLAGS="$CFLAGS $X_CFLAGS"
     LDFLAGS="$LDFLAGS $X_LIBS"
     LIBS="$LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"

     LIBS="$LIBS -lXaw"
     AC_MSG_CHECKING([for Xaw library and header files])
     AC_LINK_IFELSE([
	 AC_LANG_PROGRAM([[

#include <X11/Intrinsic.h>
#include <X11/Xaw/Simple.h>

	 ]],
	 [])
       ],
       [AC_MSG_RESULT([yes])],
       [AC_MSG_RESULT([no])
	groff_no_x="yes"])

     LIBS="$LIBS -lXmu"
     AC_MSG_CHECKING([for Xmu library and header files])
     AC_LINK_IFELSE([
	 AC_LANG_PROGRAM([[

#include <X11/Intrinsic.h>
#include <X11/Xmu/Converters.h>

	 ]],
	 [])
       ],
       [AC_MSG_RESULT([yes])],
       [AC_MSG_RESULT([no])
	groff_no_x="yes"])

     CFLAGS=$OLDCFLAGS
     LDFLAGS=$OLDLDFLAGS
     LIBS=$OLDLIBS
   fi

   if test "x$groff_no_x" = "xyes"; then
     AC_MSG_NOTICE([gxditview and xtotroff won't be built])
   else
     XDEVDIRS="font/devX75 font/devX75-12 font/devX100 font/devX100-12"
     XPROGDIRS="src/devices/xditview src/utils/xtotroff"
     XLIBDIRS="src/libs/libxutil"
   fi

   AC_SUBST([XDEVDIRS])
   AC_SUBST([XPROGDIRS])
   AC_SUBST([XLIBDIRS])])

# Set up the `--with-appresdir' command line option.

AC_DEFUN([GROFF_APPRESDIR_OPTION],
  [AC_ARG_WITH([appresdir],
     dnl Don't quote AS_HELP_STRING!
     AS_HELP_STRING([--with-appresdir=DIR],
		    [X11 application resource files]))])

# Get a default value for the application resource directory.
#
# We ignore the `XAPPLRES' and `XUSERFILESEARCHPATH' environment variables.
#
# The goal is to find the `root' of X11.  Under most systems this is
# `/usr/X11/lib'.  Application default files are then in
# `/usr/X11/lib/X11/app-defaults'.
#
# Based on autoconf's AC_PATH_X macro.

AC_DEFUN([GROFF_APPRESDIR_DEFAULT],
  [if test -z "$groff_no_x"; then
     # Create an Imakefile, run `xmkmf', then `make'.
     rm -f -r conftest.dir
     if mkdir conftest.dir; then
       cd conftest.dir
       # Make sure to not put `make' in the Imakefile rules,
       # since we grep it out.
       cat >Imakefile <<'EOF'

xlibdirs:
	@echo 'groff_x_usrlibdir="${USRLIBDIR}"; groff_x_libdir="${LIBDIR}"'
EOF

       if (xmkmf) >/dev/null 2>/dev/null && test -f Makefile; then
	 # GNU make sometimes prints "make[1]: Entering...",
	 # which would confuse us.
	 eval `${MAKE-make} xlibdirs 2>/dev/null | grep -v make`

	 # Open Windows `xmkmf' reportedly sets LIBDIR instead of USRLIBDIR.
	 for groff_extension in a so sl; do
	   if test ! -f $groff_x_usrlibdir/libX11.$groff_extension &&
	      test -f $groff_x_libdir/libX11.$groff_extension; then
	     groff_x_usrlibdir=$groff_x_libdir
	     break
	   fi
	 done
       fi

       cd ..
       rm -f -r conftest.dir
     fi

     # In case the test with `xmkmf' wasn't successful, try a suite of
     # standard directories.  Check `X11' before `X11Rn' because it is often
     # a symlink to the current release.
     groff_x_libdirs='
       /usr/X11/lib
       /usr/X11R6/lib
       /usr/X11R5/lib
       /usr/X11R4/lib

       /usr/lib/X11
       /usr/lib/X11R6
       /usr/lib/X11R5
       /usr/lib/X11R4

       /usr/local/X11/lib
       /usr/local/X11R6/lib
       /usr/local/X11R5/lib
       /usr/local/X11R4/lib

       /usr/local/lib/X11
       /usr/local/lib/X11R6
       /usr/local/lib/X11R5
       /usr/local/lib/X11R4

       /usr/X386/lib
       /usr/x386/lib
       /usr/XFree86/lib/X11

       /usr/lib
       /usr/local/lib
       /usr/unsupported/lib
       /usr/athena/lib
       /usr/local/x11r5/lib
       /usr/lpp/Xamples/lib

       /usr/openwin/lib
       /usr/openwin/share/lib'

     if test -z "$groff_x_usrlibdir"; then
       # We only test whether libX11 exists.
       for groff_dir in $groff_x_libdirs; do
	 for groff_extension in a so sl; do
	   if test ! -r $groff_dir/libX11.$groff_extension; then
	     groff_x_usrlibdir=$groff_dir
	     break 2
	   fi
	 done
       done
     fi

     if test "x$with_appresdir" = "x"; then
       appresdir=$groff_x_usrlibdir/X11/app-defaults
     else
       appresdir=$with_appresdir
     fi
   fi
   AC_SUBST([appresdir])])


# Emit warning if --with-appresdir hasn't been used.

AC_DEFUN([GROFF_APPRESDIR_CHECK],
  [if test -z "$groff_no_x"; then
     if test "x$with_appresdir" = "x"; then
       AC_MSG_NOTICE([

  The application resource file for gxditview will be installed as

    $appresdir/GXditview

  (an existing file will be saved as `GXditview.old').
  To install it into a different directory, say, `/etc/gxditview',
  add `--with-appresdir=/etc/gxditview' to the configure script
  command line options and rerun it.  The environment variable
  `APPLRESDIR' must then be set to `/etc/' (note the trailing slash),
  omitting the `gxditview' part which is automatically appended by
  the X11 searching routines for resource files.  More details can be
  found in the X(7) manual page.
       ])
     fi
   fi])
