#
# Copyright (c) 2004-2005
#	Hartmut Brandt.
#	All rights reserved.
# Copyright (c) 2003-2003
#	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
#	All rights reserved.
#
# Author: Harti Brandt <harti@freebsd.org>
#
# $Begemot: bsnmp/configure.ac,v 1.8 2005/10/04 11:21:26 brandt_h Exp $
#
AC_INIT([BSNMP], [1.0], [harti@freebsd.org], bsnmp)
AC_PREREQ(2.53)
AC_COPYRIGHT([Copyright (c)1996-2003 Fraunhofer Fokus. All rights reserved.])
AC_REVISION($Revision$)

AC_CONFIG_SRCDIR(config/install.sh)
AC_CONFIG_AUX_DIR(config)
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_PROG_CC
AC_PROG_CPP
AC_PROG_MAKE_SET
AC_PROG_INSTALL

AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

AC_ARG_WITH(libbegemot,
	AC_HELP_STRING([--with-libbegemot],
		[use libbegemot instead of libisc and set path to where the
includes and lib are found(default is NO, if no path specified default=/usr/local)]),
	ac_cv_use_libbegemot=$withval, ac_cv_use_libbegemot=no)
AC_CACHE_CHECK(whether to use libbegemot,
	ac_cv_use_libbegemot, ac_cv_use_libbegemot=no)

if test $ac_cv_use_libbegemot == "yes" ; then
	ac_cv_use_libbegemot="/usr/local"
fi

if test $ac_cv_use_libbegemot != "no" ; then
	AC_DEFINE(USE_LIBBEGEMOT)
	AC_SUBST(LIBEV, -lbegemot)

	CFLAGS="${CFLAGS} -I$ac_cv_use_libbegemot/include"
	LDFLAGS="${LDFLAGS} -L$ac_cv_use_libbegemot/lib"

	AC_CHECK_LIB(begemot, poll_register, ,
		[AC_ERROR([libbegemot not found])])
else
	AC_SUBST(LIBEV, -lisc)
fi

AC_ARG_WITH(tcpwrappers,
	AC_HELP_STRING([--with-tcpwrappers],
		[use libwrap to control access]),
	ac_cv_use_tcpwrappers=$withval, ac_cv_use_tcpwrappers=no)

if test $ac_cv_use_tcpwrappers != "no" ; then
	AC_DEFINE(USE_TCPWRAPPERS)
	AC_SUBST(LIBWRAP, -lwrap)
fi

LDFLAGS="${LDFLAGS} -L/usr/local/lib"

AC_CHECK_LIB(smi, smiGetNode, HAVE_LIBSMI=yes, HAVE_LIBSMI=no)
AC_SUBST(HAVE_LIBSMI)

# check for BSD's err.h 
AC_CHECK_HEADERS([err.h])

# check for strlcpy
AC_CHECK_FUNCS(strlcpy)

# check for getaddrinfo
AC_CHECK_FUNCS(getaddrinfo)

AC_CHECK_HEADER(stdint.h,
   AC_DEFINE(HAVE_STDINT_H))
AC_CHECK_HEADER(inttypes.h,
   [AC_DEFINE(HAVE_INTTYPES_H)
    ac_have_inttypes_h=yes])
AC_CACHE_CHECK([how to print quads],
    ac_cv_quad_fmt,
   [if test "$ac_have_inttypes_h" = "yes" ; then
       AC_EGREP_CPP(yes,
[
# include <inttypes.h>
# ifdef PRIu64
yes
# endif
],
        ac_cv_quad_fmt=PRIu64,
       AC_TRY_RUN(
[[
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
int
main()
{
   char buf[100];

   sprintf(buf, "%llx", 0x1234567887654321LLU);
   exit((strcmp(buf, "1234567887654321") == 0) ? 0 : 1);
}
]],
            ac_cv_quad_fmt='"ll"',
            ac_cv_quad_fmt='"q"',
            ac_cv_quad_fmt='"q"')
   fi ]))

if test "$ac_cv_quad_fmt" = "PRIu64" ; then
   AC_DEFINE(QUADFMT, PRIu64)	
   AC_DEFINE(QUADXFMT, PRIx64)
else
   AC_DEFINE_UNQUOTED(QUADFMT, ${ac_cv_quad_fmt}"u")
   AC_DEFINE_UNQUOTED(QUADXFMT, ${ac_cv_quad_fmt}"x")
fi


AC_CONFIG_FILES([
	Makefile:config/Makefile.pre:Makefile.in
	lib/Makefile:config/Makefile.pre:config/Makefile.build:lib/Makefile.in:config/Makefile.post
	gensnmptree/Makefile:config/Makefile.pre:config/Makefile.build:gensnmptree/Makefile.in:config/Makefile.post
	gensnmpdef/Makefile:config/Makefile.pre:config/Makefile.build:gensnmpdef/Makefile.in:config/Makefile.post
	snmpd/Makefile:config/Makefile.pre:config/Makefile.build:snmpd/Makefile.in:config/Makefile.post
	snmp_mibII/Makefile:config/Makefile.pre:config/Makefile.build:snmp_mibII/Makefile.in:config/Makefile.post
	snmp_ntp/Makefile:config/Makefile.pre:config/Makefile.build:snmp_ntp/Makefile.in:config/Makefile.post
])
AC_OUTPUT
