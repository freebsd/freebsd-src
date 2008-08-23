dnl $Id: configure.ac 411 2007-12-21 11:44:14Z des $

AC_PREREQ(2.59)
AC_REVISION([$Id: configure.ac 411 2007-12-21 11:44:14Z des $])
AC_INIT([OpenPAM],[20071221],[des@des.no])
AC_CONFIG_SRCDIR(lib/pam_start.c)
AM_CONFIG_HEADER(config.h)

AC_CANONICAL_SYSTEM
AC_LANG(C)
AC_C_VOLATILE
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AM_INIT_AUTOMAKE

LIB_MAJ=2
AC_SUBST(LIB_MAJ)
AC_DEFINE_UNQUOTED(LIB_MAJ, $LIB_MAJ, [OpenPAM library major number])

AC_ARG_ENABLE(debug,
	AC_HELP_STRING([--enable-debug],
		[turn debugging on by default]),
	AC_DEFINE(OPENPAM_DEBUG, 1, [Turn debugging on by default]))

AC_MSG_CHECKING([whether loading unversioned modules support is enabled])
AC_ARG_ENABLE(unversioned-modules,
	AC_HELP_STRING([--disable-unversioned-modules],
		[support loading of unversioned modules]),
	[if test "$enableval" = "no"; then
		AC_DEFINE(DISABLE_UNVERSIONED_MODULES,
			1,
			[Whether loading unversioned modules support is disabled])
	fi
	AC_MSG_RESULT(no)],
	AC_MSG_RESULT(yes))

AC_MSG_CHECKING([for modules directory support])
AC_ARG_WITH(modules-dir,
	AC_HELP_STRING([--with-modules-dir=DIR],
		[OpenPAM modules directory]),
	[if test "$withval" != "no"; then
		OPENPAM_MODULES_DIR="$withval"
		AC_DEFINE_UNQUOTED(OPENPAM_MODULES_DIR,
			"$OPENPAM_MODULES_DIR",
			[OpenPAM modules directory])
		AC_MSG_RESULT($OPENPAM_MODULES_DIR)
	else
		OPENPAM_MODULES_DIR="$libdir"
		AC_MSG_RESULT(no)
	fi],
	[OPENPAM_MODULES_DIR="$libdir"
	AC_MSG_RESULT(no)])
AC_SUBST(OPENPAM_MODULES_DIR)

AC_MSG_CHECKING([whether to build the documentation])
AC_ARG_WITH(doc,
	AC_HELP_STRING([--with-doc],
		[build documentation]),
	,
	[with_doc=yes])
AC_MSG_RESULT($with_doc)

AC_MSG_CHECKING([whether to build example version of pam_unix.so])
AC_ARG_WITH(pam-unix,
	AC_HELP_STRING([--with-pam-unix],
		[compile example version of pam_unix.so]),
	,
	[with_pam_unix=no])
AC_MSG_RESULT($with_pam_unix)

AC_MSG_CHECKING([whether to build example version of su(1)])
AC_ARG_WITH(su,
	AC_HELP_STRING([--with-su],
		[compile example version of su(1)]),
	,
	[with_su=no])
AC_MSG_RESULT($with_su)

AM_CONDITIONAL(WITH_DOC, test "x$with_doc" = "xyes")
AM_CONDITIONAL(WITH_PAM_UNIX, test "x$with_pam_unix" = "xyes")
AM_CONDITIONAL(WITH_SU, test "x$with_su" = "xyes")

AC_PROG_INSTALL

AC_CHECK_HEADERS(crypt.h)

AC_CHECK_FUNCS(fpurge)

DL_LIBS=
AC_CHECK_LIB(dl, dlopen, DL_LIBS=-ldl)
AC_SUBST(DL_LIBS)

CRYPT_LIBS=
AC_CHECK_LIB(crypt, crypt, CRYPT_LIBS=-lcrypt)
AC_SUBST(CRYPT_LIBS)

CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"

# This corresponds to FreeBSD's WARNS level 6
DEVELOPER_CFLAGS="-Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch -Wshadow -Wcast-align -Wunused-parameter -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls -Wformat"

AC_ARG_ENABLE(developer-warnings,
        AS_HELP_STRING([--enable-developer-warnings],[enable strict warnings (default is NO)]),
        CFLAGS="${CFLAGS} ${DEVELOPER_CFLAGS}")
AC_ARG_ENABLE(debugging-symbols,
        AS_HELP_STRING([--enable-debugging-symbols],[enable debugging symbols (default is NO)]),
        CFLAGS="${CFLAGS} -O0 -g -fno-inline")
AC_ARG_ENABLE(werror,
        AS_HELP_STRING([--enable-werror],[use -Werror (default is NO)]),
        CFLAGS="${CFLAGS} -Werror")

AC_CONFIG_FILES([
	bin/Makefile
	bin/su/Makefile
	include/Makefile
	include/security/Makefile
	lib/Makefile
	modules/Makefile
	modules/pam_unix/Makefile
	modules/pam_deny/Makefile
	modules/pam_permit/Makefile
	doc/Makefile
	doc/man/Makefile
	Makefile
])
AC_OUTPUT
