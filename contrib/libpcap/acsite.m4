dnl @(#) $Header: acsite.m4,v 1.41 96/11/29 15:30:40 leres Exp $ (LBL)
dnl
dnl Copyright (c) 1995, 1996
dnl	The Regents of the University of California.  All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that: (1) source code distributions
dnl retain the above copyright notice and this paragraph in its entirety, (2)
dnl distributions including binary code include the above copyright notice and
dnl this paragraph in its entirety in the documentation or other materials
dnl provided with the distribution, and (3) all advertising materials mentioning
dnl features or use of this software display the following acknowledgement:
dnl ``This product includes software developed by the University of California,
dnl Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
dnl the University nor the names of its contributors may be used to endorse
dnl or promote products derived from this software without specific prior
dnl written permission.
dnl THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
dnl WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
dnl MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
dnl
dnl LBL autoconf macros
dnl

dnl
dnl Determine which compiler we're using (cc or gcc)
dnl If using gcc, determine the version number
dnl If using cc, require that it support ansi prototypes
dnl If using gcc, use -O2 (otherwise use -O)
dnl If using cc, explicitly specify /usr/local/include
dnl
dnl usage:
dnl
dnl	AC_LBL_C_INIT(copt, incls)
dnl
dnl results:
dnl
dnl	$1 (copt set)
dnl	$2 (incls set)
dnl	CC
dnl	ac_cv_gcc_vers
dnl	LBL_CFLAGS
dnl
dnl XXX need to add test to make sure ac_prog_cc hasn't been called
AC_DEFUN(AC_LBL_C_INIT,
    [AC_PREREQ(2.12)
    $1=-O
    $2=""
    if test "${CFLAGS+set}" = set; then
	    LBL_CFLAGS="$CFLAGS"
    fi
    if test -z "$CC" ; then
	    case "$target_os" in

	    bsdi*)
		    AC_CHECK_PROG(SHLICC2, shlicc2, yes, no)
		    if test $SHLICC2 = yes ; then
			    CC=shlicc2
			    export CC
		    fi
		    ;;
	    esac
    fi
    AC_PROG_CC
    if test $ac_cv_prog_gcc = yes ; then
	    if test "$SHLICC2" = yes ; then
		    ac_cv_gcc_vers=2
		    $1=-O2
	    else
		    AC_MSG_CHECKING(gcc version)
		    AC_CACHE_VAL(ac_cv_gcc_vers,
			ac_cv_gcc_vers=`$CC -v 2>&1 | \
			    sed -n -e '$s/.* //' -e '$s/\..*//p'`)
		    AC_MSG_RESULT($ac_cv_gcc_vers)
		    if test $ac_cv_gcc_vers -gt 1 ; then
			    $1=-O2
		    fi
	    fi
    else
	    AC_MSG_CHECKING(that $CC handles ansi prototypes)
	    AC_CACHE_VAL(ac_cv_cc_ansi_prototypes,
		AC_TRY_COMPILE(
		    [#include <sys/types.h>],
		    [int frob(int, char *)],
		    ac_cv_cc_ansi_prototypes=yes,
		    ac_cv_cc_ansi_prototypes=no))
	    AC_MSG_RESULT($ac_cv_cc_ansi_prototypes)
	    if test $ac_cv_cc_ansi_prototypes = no ; then
		    case "$target_os" in

		    hpux*)
			    AC_MSG_CHECKING(for HP-UX ansi compiler ($CC -Aa -D_HPUX_SOURCE))
			    savedcflags="$CFLAGS"
			    CFLAGS="-Aa -D_HPUX_SOURCE $CFLAGS"
			    AC_CACHE_VAL(ac_cv_cc_hpux_cc_aa,
				AC_TRY_COMPILE(
				    [#include <sys/types.h>],
				    [int frob(int, char *)],
				    ac_cv_cc_hpux_cc_aa=yes,
				    ac_cv_cc_hpux_cc_aa=no))
			    AC_MSG_RESULT($ac_cv_cc_hpux_cc_aa)
			    if test $ac_cv_cc_hpux_cc_aa = no ; then
				    AC_MSG_ERROR(see the INSTALL for more info)
			    fi
			    CFLAGS="$savedcflags"
			    V_CCOPT="-Aa $V_CCOPT"
			    AC_DEFINE(_HPUX_SOURCE)
			    ;;

		    *)
			    AC_MSG_ERROR(see the INSTALL for more info)
			    ;;
		    esac
	    fi
	    $2=-I/usr/local/include

	    case "$target_os" in

	    irix*)
		    V_CCOPT="$V_CCOPT -xansi -signed -g3"
		    ;;

	    osf*)
		    V_CCOPT="$V_CCOPT -g3"
		    ;;

	    ultrix*)
		    AC_MSG_CHECKING(that Ultrix $CC hacks const in prototypes)
		    AC_CACHE_VAL(ac_cv_cc_const_proto,
			AC_TRY_COMPILE(
			    [#include <sys/types.h>],
			    [struct a { int b; };
			    void c(const struct a *)],
			    ac_cv_cc_const_proto=yes,
			    ac_cv_cc_const_proto=no))
		    AC_MSG_RESULT($ac_cv_cc_const_proto)
		    if test $ac_cv_cc_const_proto = no ; then
			    AC_DEFINE(const,)
		    fi
		    ;;
	    esac
    fi
])

dnl
dnl Use pfopen.c if available and pfopen() not in standard libraries
dnl Require libpcap
dnl Look for libpcap in ..
dnl Use the installed libpcap if there is no local version
dnl
dnl usage:
dnl
dnl	AC_LBL_LIBPCAP(pcapdep, incls)
dnl
dnl results:
dnl
dnl	$1 (pcapdep set)
dnl	$2 (incls appended)
dnl	LIBS
dnl
AC_DEFUN(AC_LBL_LIBPCAP,
    [pfopen=/usr/examples/packetfilter/pfopen.c
    if test -f $pfopen ; then
	    AC_CHECK_FUNCS(pfopen)
	    if test $ac_cv_func_pfopen = "no" ; then
		    AC_MSG_RESULT(Using $pfopen)
		    LIBS="$LIBS $pfopen"
	    fi
    fi
    AC_MSG_CHECKING(for local pcap library)
    libpcap=FAIL
    lastdir=FAIL
    places=`ls .. | sed -e 's,/$,,' -e 's,^,../,' | \
	egrep '/libpcap-[[0-9]]*\.[[0-9]]*(\.[[0-9]]*)?([[ab]][[0-9]]*)?$'`
    for dir in $places ../libpcap libpcap ; do
	    basedir=`echo $dir | sed -e 's/[[ab]][[0-9]]*$//'`
	    if test $lastdir = $basedir ; then
		    dnl skip alphas when an actual release is present
		    continue;
	    fi
	    lastdir=$dir
	    if test -r $dir/pcap.c ; then
		    libpcap=$dir/libpcap.a
		    d=$dir
		    dnl continue and select the last one that exists
	    fi
    done
    if test $libpcap = FAIL ; then
	    AC_MSG_RESULT(not found)
	    AC_CHECK_LIB(pcap, main, libpcap="-lpcap")
	    if test $libpcap = FAIL ; then
		    AC_MSG_ERROR(see the INSTALL doc for more info)
	    fi
    else
	    $1=$libpcap
	    $2="-I$d $$2"
	    AC_MSG_RESULT($libpcap)
    fi
    LIBS="$libpcap $LIBS"])

dnl
dnl Define RETSIGTYPE and RETSIGVAL
dnl
dnl usage:
dnl
dnl	AC_LBL_TYPE_SIGNAL
dnl
dnl results:
dnl
dnl	RETSIGTYPE (defined)
dnl	RETSIGVAL (defined)
dnl
AC_DEFUN(AC_LBL_TYPE_SIGNAL,
    [AC_TYPE_SIGNAL
    if test "$ac_cv_type_signal" = void ; then
	    AC_DEFINE(RETSIGVAL,)
    else
	    AC_DEFINE(RETSIGVAL,(0))
    fi
    case "$target_os" in

    irix*)
	    AC_DEFINE(_BSD_SIGNALS)
	    ;;

    *)
	    AC_CHECK_FUNCS(sigset)
	    if test $ac_cv_func_sigset = yes ; then
		    AC_DEFINE(signal, sigset)
	    fi
	    ;;
    esac])

dnl
dnl If using gcc, see if fixincludes should be run
dnl
dnl usage:
dnl
dnl	AC_LBL_FIXINCLUDES
dnl
AC_DEFUN(AC_LBL_FIXINCLUDES,
    [if test $ac_cv_prog_gcc = yes ; then
	    AC_MSG_CHECKING(if fixincludes is needed)
	    AC_CACHE_VAL(ac_cv_gcc_fixincludes,
		AC_TRY_COMPILE(
		    [/*
		     * This generates a "duplicate case value" when fixincludes
		     * has not be run.
		     */
#		include <sys/types.h>
#		include <sys/time.h>
#		include <sys/ioctl.h>
#		ifdef HAVE_SYS_IOCCOM_H
#		include <sys/ioccom.h>
#		endif],
		    [switch (0) {
		    case _IO('A', 1):;
		    case _IO('B', 1):;
		    }],
		    ac_cv_gcc_fixincludes=yes,
		    ac_cv_gcc_fixincludes=no))
	    AC_MSG_RESULT($ac_cv_gcc_fixincludes)
	    if test $ac_cv_gcc_fixincludes = no ; then
		    # Don't cache failure
		    unset ac_cv_gcc_fixincludes
		    AC_MSG_ERROR(see the INSTALL for more info)
	    fi
    fi])

dnl
dnl Check for flex, default to lex
dnl Require flex 2.4 or higher
dnl Check for bison, default to yacc
dnl Default to lex/yacc if both flex and bison are not available
dnl Define the yy prefix string if using flex and bison
dnl
dnl usage:
dnl
dnl	AC_LBL_LEX_AND_YACC(lex, yacc, yyprefix)
dnl
dnl results:
dnl
dnl	$1 (lex set)
dnl	$2 (yacc appended)
dnl	$3 (optional flex and bison -P prefix)
dnl
AC_DEFUN(AC_LBL_LEX_AND_YACC,
    [AC_CHECK_PROGS($1, flex, lex)
    if test "$$1" = flex ; then
	    # The -V flag was added in 2.4
	    AC_MSG_CHECKING(for flex 2.4 or higher)
	    AC_CACHE_VAL(ac_cv_flex_v24,
		if flex -V >/dev/null 2>&1; then
			ac_cv_flex_v24=yes
		else
			ac_cv_flex_v24=no
		fi)
	    AC_MSG_RESULT($ac_cv_flex_v24)
	    if test $ac_cv_flex_v24 = no ; then
		    s="2.4 or higher required"
		    AC_MSG_WARN(ignoring obsolete flex executable ($s))
		    $1=lex
	    fi
    fi
    AC_CHECK_PROGS($2, bison, yacc)
    if test "$$2" = bison ; then
	    $2="$$2 -y"
    fi
    if test "$$1" != lex -a "$$2" = yacc -o "$$1" = lex -a "$$2" != yacc ; then
	    AC_MSG_WARN(don't have both flex and bison; reverting to lex/yacc)
	    $1=lex
	    $2=yacc
    fi
    if test "$$1" = flex -a -n "$3" ; then
	    $1="$$1 -P$3"
	    $2="$$2 -p $3"
    fi])

dnl
dnl Checks to see if union wait is used with WEXITSTATUS()
dnl
dnl usage:
dnl
dnl	AC_LBL_UNION_WAIT
dnl
dnl results:
dnl
dnl	DECLWAITSTATUS (defined)
dnl
AC_DEFUN(AC_LBL_UNION_WAIT,
    [AC_MSG_CHECKING(if union wait is used)
    AC_CACHE_VAL(ac_cv_union_wait,
	AC_TRY_COMPILE([
#	include <sys/types.h>
#	include <sys/wait.h>],
	    [int status;
	    u_int i = WEXITSTATUS(status);
	    u_int j = waitpid(0, &status, 0);],
	    ac_cv_union_wait=no,
	    ac_cv_union_wait=yes))
    AC_MSG_RESULT($ac_cv_union_wait)
    if test $ac_cv_union_wait = yes ; then
	    AC_DEFINE(DECLWAITSTATUS,union wait)
    else
	    AC_DEFINE(DECLWAITSTATUS,int)
    fi])

dnl
dnl Checks to see if the sockaddr struct has the 4.4 BSD sa_len member
dnl
dnl usage:
dnl
dnl	AC_LBL_SOCKADDR_SA_LEN
dnl
dnl results:
dnl
dnl	HAVE_SOCKADDR_SA_LEN (defined)
dnl
AC_DEFUN(AC_LBL_SOCKADDR_SA_LEN,
    [AC_MSG_CHECKING(if sockaddr struct has sa_len member)
    AC_CACHE_VAL(ac_cv_sockaddr_has_sa_len,
	AC_TRY_COMPILE([
#	include <sys/types.h>
#	include <sys/socket.h>],
	[u_int i = sizeof(((struct sockaddr *)0)->sa_len)],
	ac_cv_sockaddr_has_sa_len=yes,
	ac_cv_sockaddr_has_sa_len=no))
    AC_MSG_RESULT($ac_cv_sockaddr_has_sa_len)
    if test $ac_cv_sockaddr_has_sa_len = yes ; then
	    AC_DEFINE(HAVE_SOCKADDR_SA_LEN)
    fi])

dnl
dnl Checks to see if -R is used
dnl
dnl usage:
dnl
dnl	AC_LBL_HAVE_RUN_PATH
dnl
dnl results:
dnl
dnl	ac_cv_have_run_path (yes or no)
dnl
AC_DEFUN(AC_LBL_HAVE_RUN_PATH,
    [AC_MSG_CHECKING(for ${CC-cc} -R)
    AC_CACHE_VAL(ac_cv_have_run_path,
	[echo 'main(){}' > conftest.c
	${CC-cc} -o conftest conftest.c -R/a1/b2/c3 >conftest.out 2>&1
	if test ! -s conftest.out ; then
		ac_cv_have_run_path=yes
	else
		ac_cv_have_run_path=no
	fi
	rm -f conftest*])
    AC_MSG_RESULT($ac_cv_have_run_path)
    ])

dnl
dnl Checks to see if unaligned memory accesses fail
dnl
dnl usage:
dnl
dnl	AC_LBL_UNALIGNED_ACCESS
dnl
dnl results:
dnl
dnl	LBL_ALIGN (DEFINED)
dnl
AC_DEFUN(AC_LBL_UNALIGNED_ACCESS,
    [AC_MSG_CHECKING(if unaligned accesses fail)
    AC_CACHE_VAL(ac_cv_unaligned_fail,
	[case "$target_cpu" in

	alpha|hp*|mips|sparc)
		ac_cv_unaligned_fail=yes
		;;

	*)
		cat >conftest.c <<EOF
#		include <sys/types.h>
#		include <sys/wait.h>
#		include <stdio.h>
		unsigned char a[[5]] = { 1, 2, 3, 4, 5 };
		main() {
		unsigned int i;
		pid_t pid;
		int status;
		/* avoid "core dumped" message */
		pid = fork();
		if (pid <  0)
			exit(2);
		if (pid > 0) {
			/* parent */
			pid = waitpid(pid, &status, 0);
			if (pid < 0)
				exit(3);
			exit(!WIFEXITED(status));
		}
		/* child */
		i = *(unsigned int *)&a[[1]];
		printf("%d\n", i);
		exit(0);
		}
EOF
		${CC-cc} -o conftest $CFLAGS $CPPFLAGS $LDFLAGS \
		    conftest.c $LIBS >/dev/null 2>&1
		if test ! -x conftest ; then
			dnl failed to compile for some reason
			ac_cv_unaligned_fail=yes
		else
			./conftest >conftest.out
			if test ! -s conftest.out ; then
				ac_cv_unaligned_fail=yes
			else
				ac_cv_unaligned_fail=no
			fi
		fi
		rm -f conftest* core core.conftest
		;;
	esac])
    AC_MSG_RESULT($ac_cv_unaligned_fail)
    if test $ac_cv_unaligned_fail = yes ; then
	    AC_DEFINE(LBL_ALIGN)
    fi])

dnl
dnl If using gcc and the file .devel exists:
dnl	Compile with -g (if supported) and -Wall
dnl	If using gcc 2, do extra prototype checking
dnl	If an os prototype include exists, symlink os-proto.h to it
dnl
dnl usage:
dnl
dnl	AC_LBL_DEVEL(copt)
dnl
dnl results:
dnl
dnl	$1 (copt appended)
dnl	HAVE_OS_PROTO_H (defined)
dnl	os-proto.h (symlinked)
dnl
AC_DEFUN(AC_LBL_DEVEL,
    [rm -f os-proto.h
    if test "${LBL_CFLAGS+set}" = set; then
	    $1="$$1 ${LBL_CFLAGS}"
    fi
    if test $ac_cv_prog_gcc = yes -a -f .devel ; then
	    if test "${LBL_CFLAGS+set}" != set; then
		    if test "$ac_cv_prog_cc_g" = yes ; then
			    $1="-g $$1"
		    fi
		    $1="$$1 -Wall"
		    if test $ac_cv_gcc_vers -gt 1 ; then
			    $1="$$1 -Wmissing-prototypes -Wstrict-prototypes"
		    fi
	    fi
	    os=`echo $target_os | sed -e 's/\([[0-9]][[0-9]]*\)[[^0-9]].*$/\1/'`
	    name="lbl/os-$os.h"
	    if test -f $name ; then
		    ln -s $name os-proto.h
		    AC_DEFINE(HAVE_OS_PROTO_H)
	    else
		    AC_MSG_WARN(can't find $name)
	    fi
    fi])
