AC_DEFUN(hs_ULONG_CONST,
[ AC_EGREP_CPP(Circus,
 [#define ACAT(a,b)a ## b
ACAT(Cir,cus)
], AC_DEFINE([ULONG_CONST(a)], [a ## UL]),
    AC_EGREP_CPP(Reiser,
[#define RCAT(a,b)a/**/b
RCAT(Rei,ser)
], AC_DEFINE([ULONG_CONST(a)], [a/**/L]),
    AC_MSG_ERROR([How do we create an unsigned long constant?])))])
dnl @synopsis AC_DEFINE_DIR(VARNAME, DIR [, DESCRIPTION])
dnl
dnl This macro defines (with AC_DEFINE) VARNAME to the expansion of the DIR
dnl variable, taking care of fixing up ${prefix} and such.
dnl
dnl Note that the 3 argument form is only supported with autoconf 2.13 and
dnl later (i.e. only where AC_DEFINE supports 3 arguments).
dnl
dnl Examples:
dnl
dnl    AC_DEFINE_DIR(DATADIR, datadir)
dnl    AC_DEFINE_DIR(PROG_PATH, bindir, [Location of installed binaries])
dnl
dnl @version $Id: acinclude.m4,v 1.3 2000/08/04 03:26:22 stenn Exp $
dnl @author Alexandre Oliva <oliva@lsd.ic.unicamp.br>

AC_DEFUN(AC_DEFINE_DIR, [
	ac_expanded=`(
	    test "x$prefix" = xNONE && prefix="$ac_default_prefix"
	    test "x$exec_prefix" = xNONE && exec_prefix="${prefix}"
	    eval echo \""[$]$2"\"
        )`
	ifelse($3, ,
	  AC_DEFINE_UNQUOTED($1, "$ac_expanded"),
	  AC_DEFINE_UNQUOTED($1, "$ac_expanded", $3))
])
