dnl***************************************************************************
dnl Copyright (c) 1998-2000 Free Software Foundation, Inc.                   *
dnl                                                                          *
dnl Permission is hereby granted, free of charge, to any person obtaining a  *
dnl copy of this software and associated documentation files (the            *
dnl "Software"), to deal in the Software without restriction, including      *
dnl without limitation the rights to use, copy, modify, merge, publish,      *
dnl distribute, distribute with modifications, sublicense, and/or sell       *
dnl copies of the Software, and to permit persons to whom the Software is    *
dnl furnished to do so, subject to the following conditions:                 *
dnl                                                                          *
dnl The above copyright notice and this permission notice shall be included  *
dnl in all copies or substantial portions of the Software.                   *
dnl                                                                          *
dnl THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
dnl OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
dnl MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
dnl IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
dnl DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
dnl OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
dnl THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
dnl                                                                          *
dnl Except as contained in this notice, the name(s) of the above copyright   *
dnl holders shall not be used in advertising or otherwise to promote the     *
dnl sale, use or other dealings in this Software without prior written       *
dnl authorization.                                                           *
dnl***************************************************************************
dnl
dnl Author: Thomas E. Dickey 1996,1997,1998,1999,2000
dnl
dnl $Id: aclocal.m4,v 1.248 2001/04/21 19:40:04 tom Exp $
dnl Macros used in NCURSES auto-configuration script.
dnl
dnl See http://dickey.his.com/autoconf/ for additional information.
dnl
dnl ---------------------------------------------------------------------------
dnl ---------------------------------------------------------------------------
dnl Construct the list of include-options for the C programs in the Ada95
dnl binding.
AC_DEFUN([CF_ADA_INCLUDE_DIRS],
[
ACPPFLAGS="$ACPPFLAGS -I. -I../../include"
if test "$srcdir" != "."; then
	ACPPFLAGS="$ACPPFLAGS -I\$(srcdir)/../../include"
fi
if test "$GCC" != yes; then
	ACPPFLAGS="$ACPPFLAGS -I\$(includedir)"
elif test "$includedir" != "/usr/include"; then
	if test "$includedir" = '${prefix}/include' ; then
		if test $prefix != /usr ; then
			ACPPFLAGS="$ACPPFLAGS -I\$(includedir)"
		fi
	else
		ACPPFLAGS="$ACPPFLAGS -I\$(includedir)"
	fi
fi
AC_SUBST(ACPPFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl Copy non-preprocessor flags to $CFLAGS, preprocessor flags to $CPPFLAGS
AC_DEFUN([CF_ADD_CFLAGS],
[
for cf_add_cflags in $1
do
	case $cf_add_cflags in #(vi
	-undef|-nostdinc*|-I*|-D*|-U*|-E|-P|-C) #(vi
		case "$CPPFLAGS" in
		*$cf_add_cflags)
			;;
		*)
			CPPFLAGS="$CPPFLAGS $cf_add_cflags"
			;;
		esac
		;;
	*)
		CFLAGS="$CFLAGS $cf_add_cflags"
		;;
	esac
done
])dnl
dnl ---------------------------------------------------------------------------
dnl This is adapted from the macros 'fp_PROG_CC_STDC' and 'fp_C_PROTOTYPES'
dnl in the sharutils 4.2 distribution.
AC_DEFUN([CF_ANSI_CC_CHECK],
[
AC_MSG_CHECKING(for ${CC-cc} option to accept ANSI C)
AC_CACHE_VAL(cf_cv_ansi_cc,[
cf_cv_ansi_cc=no
cf_save_CFLAGS="$CFLAGS"
cf_save_CPPFLAGS="$CPPFLAGS"
# Don't try gcc -ansi; that turns off useful extensions and
# breaks some systems' header files.
# AIX			-qlanglvl=ansi
# Ultrix and OSF/1	-std1
# HP-UX			-Aa -D_HPUX_SOURCE
# SVR4			-Xc
# UnixWare 1.2		(cannot use -Xc, since ANSI/POSIX clashes)
for cf_arg in "-DCC_HAS_PROTOS" \
	"" \
	-qlanglvl=ansi \
	-std1 \
	-Ae \
	"-Aa -D_HPUX_SOURCE" \
	-Xc
do
	CF_ADD_CFLAGS($cf_arg)
	AC_TRY_COMPILE(
[
#ifndef CC_HAS_PROTOS
#if !defined(__STDC__) || (__STDC__ != 1)
choke me
#endif
#endif
],[
	int test (int i, double x);
	struct s1 {int (*f) (int a);};
	struct s2 {int (*f) (double a);};],
	[cf_cv_ansi_cc="$cf_arg"; break])
done
CFLAGS="$cf_save_CFLAGS"
CPPFLAGS="$cf_save_CPPFLAGS"
])
AC_MSG_RESULT($cf_cv_ansi_cc)

if test "$cf_cv_ansi_cc" != "no"; then
if test ".$cf_cv_ansi_cc" != ".-DCC_HAS_PROTOS"; then
	CF_ADD_CFLAGS($cf_cv_ansi_cc)
else
	AC_DEFINE(CC_HAS_PROTOS)
fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl For programs that must use an ANSI compiler, obtain compiler options that
dnl will make it recognize prototypes.  We'll do preprocessor checks in other
dnl macros, since tools such as unproto can fake prototypes, but only part of
dnl the preprocessor.
AC_DEFUN([CF_ANSI_CC_REQD],
[AC_REQUIRE([CF_ANSI_CC_CHECK])
if test "$cf_cv_ansi_cc" = "no"; then
	AC_ERROR(
[Your compiler does not appear to recognize prototypes.
You have the following choices:
	a. adjust your compiler options
	b. get an up-to-date compiler
	c. use a wrapper such as unproto])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Test if 'bool' is a builtin type in the configured C++ compiler.  Some
dnl older compilers (e.g., gcc 2.5.8) don't support 'bool' directly; gcc
dnl 2.6.3 does, in anticipation of the ANSI C++ standard.
dnl
dnl Treat the configuration-variable specially here, since we're directly
dnl substituting its value (i.e., 1/0).
AC_DEFUN([CF_BOOL_DECL],
[
AC_MSG_CHECKING([for builtin bool type])
AC_CACHE_VAL(ifelse($1,,cf_cv_builtin_bool,[$1]),[
	AC_TRY_COMPILE([
#include <stdio.h>
#include <sys/types.h>
],[bool x = false],
		[ifelse($1,,cf_cv_builtin_bool,[$1])=1],
		[ifelse($1,,cf_cv_builtin_bool,[$1])=0])
	])
if test "$ifelse($1,,cf_cv_builtin_bool,[$1])" = 1
then	AC_MSG_RESULT(yes)
else	AC_MSG_RESULT(no)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Test for the size of 'bool' in the configured C++ compiler (e.g., a type).
dnl Don't bother looking for bool.h, since it's been deprecated.
AC_DEFUN([CF_BOOL_SIZE],
[
AC_MSG_CHECKING([for size of bool])
AC_CACHE_VAL(cf_cv_type_of_bool,[
	rm -f cf_test.out
	AC_TRY_RUN([
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_GXX_BUILTIN_H
#include <g++/builtin.h>
#elif HAVE_GPP_BUILTIN_H
#include <gpp/builtin.h>
#elif HAVE_BUILTIN_H
#include <builtin.h>
#endif
main()
{
	FILE *fp = fopen("cf_test.out", "w");
	if (fp != 0) {
		bool x = true;
		if ((bool)(-x) >= 0)
			fputs("unsigned ", fp);
		if (sizeof(x) == sizeof(int))       fputs("int",  fp);
		else if (sizeof(x) == sizeof(char)) fputs("char", fp);
		else if (sizeof(x) == sizeof(short))fputs("short",fp);
		else if (sizeof(x) == sizeof(long)) fputs("long", fp);
		fclose(fp);
	}
	exit(0);
}
		],
		[cf_cv_type_of_bool=`cat cf_test.out`],
		[cf_cv_type_of_bool=unknown],
		[cf_cv_type_of_bool=unknown])
	])
	rm -f cf_test.out
AC_MSG_RESULT($cf_cv_type_of_bool)
if test "$cf_cv_type_of_bool" = unknown ; then
	AC_MSG_WARN(Assuming unsigned for type of bool)
	cf_cv_type_of_bool=unsigned
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Determine the default configuration into which we'll install ncurses.  This
dnl can be overridden by the user's command-line options.  There's two items to
dnl look for:
dnl	1. the prefix (e.g., /usr)
dnl	2. the header files (e.g., /usr/include/ncurses)
dnl We'll look for a previous installation of ncurses and use the same defaults.
dnl
dnl We don't use AC_PREFIX_DEFAULT, because it gets evaluated too soon, and
dnl we don't use AC_PREFIX_PROGRAM, because we cannot distinguish ncurses's
dnl programs from a vendor's.
AC_DEFUN([CF_CFG_DEFAULTS],
[
AC_MSG_CHECKING(for prefix)
if test "x$prefix" = "xNONE" ; then
	case "$cf_cv_system_name" in
		# non-vendor systems don't have a conflict
	openbsd*|netbsd*|freebsd*|linux*|cygwin*)
		prefix=/usr
		;;
	*)	prefix=$ac_default_prefix
		;;
	esac
fi
AC_MSG_RESULT($prefix)

if test "x$prefix" = "xNONE" ; then
AC_MSG_CHECKING(for default include-directory)
test -n "$verbose" && echo 1>&AC_FD_MSG
for cf_symbol in \
	$includedir \
	$includedir/ncurses \
	$prefix/include \
	$prefix/include/ncurses \
	/usr/local/include \
	/usr/local/include/ncurses \
	/usr/include \
	/usr/include/ncurses
do
	cf_dir=`eval echo $cf_symbol`
	if test -f $cf_dir/curses.h ; then
	if ( fgrep NCURSES_VERSION $cf_dir/curses.h 2>&1 >/dev/null ) ; then
		includedir="$cf_symbol"
		test -n "$verbose"  && echo $ac_n "	found " 1>&AC_FD_MSG
		break
	fi
	fi
	test -n "$verbose"  && echo "	tested $cf_dir" 1>&AC_FD_MSG
done
AC_MSG_RESULT($includedir)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if the terminal-capability database functions are available.  If not,
dnl ncurses has a much-reduced version.
AC_DEFUN([CF_CGETENT],[
AC_MSG_CHECKING(for terminal-capability database functions)
AC_CACHE_VAL(cf_cv_cgetent,[
AC_TRY_LINK([
#include <stdlib.h>],[
	char temp[128];
	char *buf = temp;
	char *db_array = temp;
	cgetent(&buf, /* int *, */ &db_array, "vt100");
	cgetcap(buf, "tc", '=');
	cgetmatch(buf, "tc");
	],
	[cf_cv_cgetent=yes],
	[cf_cv_cgetent=no])
])
AC_MSG_RESULT($cf_cv_cgetent)
test "$cf_cv_cgetent" = yes && AC_DEFINE(HAVE_BSD_CGETENT)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if we're accidentally using a cache from a different machine.
dnl Derive the system name, as a check for reusing the autoconf cache.
dnl
dnl If we've packaged config.guess and config.sub, run that (since it does a
dnl better job than uname).  Normally we'll use AC_CANONICAL_HOST, but allow
dnl an extra parameter that we may override, e.g., for AC_CANONICAL_SYSTEM
dnl which is useful in cross-compiles.
AC_DEFUN([CF_CHECK_CACHE],
[
if test -f $srcdir/config.guess ; then
	ifelse([$1],,[AC_CANONICAL_HOST],[$1])
	system_name="$host_os"
else
	system_name="`(uname -s -r) 2>/dev/null`"
	if test -z "$system_name" ; then
		system_name="`(hostname) 2>/dev/null`"
	fi
fi
test -n "$system_name" && AC_DEFINE_UNQUOTED(SYSTEM_NAME,"$system_name")
AC_CACHE_VAL(cf_cv_system_name,[cf_cv_system_name="$system_name"])

test -z "$system_name" && system_name="$cf_cv_system_name"
test -n "$cf_cv_system_name" && AC_MSG_RESULT("Configuring for $cf_cv_system_name")

if test ".$system_name" != ".$cf_cv_system_name" ; then
	AC_MSG_RESULT(Cached system name ($system_name) does not agree with actual ($cf_cv_system_name))
	AC_ERROR("Please remove config.cache and try again.")
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for data that is usually declared in <stdio.h> or <errno.h>, e.g.,
dnl the 'errno' variable.  Define a DECL_xxx symbol if we must declare it
dnl ourselves.
dnl
dnl (I would use AC_CACHE_CHECK here, but it will not work when called in a
dnl loop from CF_SYS_ERRLIST).
dnl
dnl $1 = the name to check
AC_DEFUN([CF_CHECK_ERRNO],
[
AC_MSG_CHECKING(if external $1 is declared)
AC_CACHE_VAL(cf_cv_dcl_$1,[
    AC_TRY_COMPILE([
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>
#include <sys/types.h>
#include <errno.h> ],
    [long x = (long) $1],
    [eval 'cf_cv_dcl_'$1'=yes'],
    [eval 'cf_cv_dcl_'$1'=no'])
])

eval 'cf_result=$cf_cv_dcl_'$1
AC_MSG_RESULT($cf_result)

if test "$cf_result" = no ; then
    eval 'cf_result=DECL_'$1
    CF_UPPER(cf_result,$cf_result)
    AC_DEFINE_UNQUOTED($cf_result)
fi

# It's possible (for near-UNIX clones) that the data doesn't exist
CF_CHECK_EXTERN_DATA($1,int)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for existence of external data in the current set of libraries.  If
dnl we can modify it, it's real enough.
dnl $1 = the name to check
dnl $2 = its type
AC_DEFUN([CF_CHECK_EXTERN_DATA],
[
AC_MSG_CHECKING(if external $1 exists)
AC_CACHE_VAL(cf_cv_have_$1,[
    AC_TRY_LINK([
#undef $1
extern $2 $1;
],
    [$1 = 2],
    [eval 'cf_cv_have_'$1'=yes'],
    [eval 'cf_cv_have_'$1'=no'])])

eval 'cf_result=$cf_cv_have_'$1
AC_MSG_RESULT($cf_result)

if test "$cf_result" = yes ; then
    eval 'cf_result=HAVE_'$1
    CF_UPPER(cf_result,$cf_result)
    AC_DEFINE_UNQUOTED($cf_result)
fi

])dnl
dnl ---------------------------------------------------------------------------
dnl Check if the C++ compiler accepts duplicate parameter initialization.  This
dnl is a late feature for the standard and is not in some recent compilers
dnl (1999/9/11).
AC_DEFUN([CF_CPP_PARAM_INIT],
[
if test -n "$CXX"; then
AC_CACHE_CHECK(if $CXX accepts parameter initialization,cf_cv_cpp_param_init,[
	AC_LANG_SAVE
	AC_LANG_CPLUSPLUS
	AC_TRY_RUN([
class TEST {
private:
	int value;
public:
	TEST(int x = 1);
	~TEST();
};

TEST::TEST(int x = 1)	// some compilers do not like second initializer
{
	value = x;
}
void main() { }
],
	[cf_cv_cpp_param_init=yes],
	[cf_cv_cpp_param_init=no],
	[cf_cv_cpp_param_init=unknown])
	AC_LANG_RESTORE
])
fi
test "$cf_cv_cpp_param_init" = yes && AC_DEFINE(CPP_HAS_PARAM_INIT)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if the g++ compiler supports vscan function (not a standard feature).
AC_DEFUN([CF_CPP_VSCAN_FUNC],
[
if test -n "$CXX"; then

AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_CHECK_HEADERS(strstream.h)

AC_CACHE_CHECK(if $CXX supports vscan function,cf_cv_cpp_vscan_func,[
	for cf_vscan_func in strstream stdio
	do
	case $cf_vscan_func in #(vi
	stdio)		cf_vscan_defs=USE_STDIO_VSCAN ;; #(vi
	strstream)	cf_vscan_defs=USE_STRSTREAM_VSCAN ;;
	esac
	AC_TRY_LINK([
#include <stdio.h>
#include <stdarg.h>
#define $cf_vscan_defs 1
#if defined(USE_STDIO_VSCAN)
#elif defined(HAVE_STRSTREAM_H) && defined(USE_STRSTREAM_VSCAN)
#include <strstream.h>
#endif

int scanw(const char* fmt, ...)
{
    int result = -1;
#if defined(__GNUG__)
    char buf[BUFSIZ];

    va_list args;
    va_start(args, fmt);
#if defined(USE_STDIO_VSCAN)
    if (::vscanf(fmt, args) != -1)
	result = 0;
#elif defined(USE_STRSTREAM_VSCAN)
    strstreambuf ss(buf, sizeof(buf));
    if (ss.vscan(fmt, (_IO_va_list)args) != -1)
	result = 0;
#else
#error case $cf_vscan_func failed
#endif
    va_end(args);
#else
#error sorry, we only know about the GNU compiler case
#endif
    return result;
}
],[int tmp, foo = scanw("%d", &tmp)],
	[cf_cv_cpp_vscan_func=$cf_vscan_func; break],
	[cf_cv_cpp_vscan_func=no])
	test "$cf_cv_cpp_vscan_func" != no && break
	done
])

AC_LANG_RESTORE
fi

case $cf_cv_cpp_vscan_func in #(vi
stdio) #(vi
	AC_DEFINE(CPP_HAS_VSCAN_FUNC)
	AC_DEFINE(USE_STDIO_VSCAN)
	;;
strstream)
	AC_DEFINE(CPP_HAS_VSCAN_FUNC)
	AC_DEFINE(USE_STRSTREAM_VSCAN)
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl "dirname" is not portable, so we fake it with a shell script.
AC_DEFUN([CF_DIRNAME],[$1=`echo $2 | sed -e 's:/[[^/]]*$::'`])dnl
dnl ---------------------------------------------------------------------------
AC_DEFUN([CF_DIRS_TO_MAKE],
[
DIRS_TO_MAKE="lib"
for cf_item in $cf_list_models
do
	CF_OBJ_SUBDIR($cf_item,cf_subdir)
	DIRS_TO_MAKE="$DIRS_TO_MAKE $cf_subdir"
done
for cf_dir in $DIRS_TO_MAKE
do
	test ! -d $cf_dir && mkdir $cf_dir
done
AC_SUBST(DIRS_TO_MAKE)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if 'errno' is declared in <errno.h>
AC_DEFUN([CF_ERRNO],
[
CF_CHECK_ERRNO(errno)
])dnl
dnl ---------------------------------------------------------------------------
dnl Test for conflicting definitions of exception in gcc 2.8.0, etc., between
dnl math.h and builtin.h, only for ncurses
AC_DEFUN([CF_ETIP_DEFINES],
[
AC_MSG_CHECKING(for special defines needed for etip.h)
cf_save_CXXFLAGS="$CXXFLAGS"
cf_result="none"
for cf_math in "" MATH_H
do
for cf_excp in "" MATH_EXCEPTION
do
	CXXFLAGS="$cf_save_CXXFLAGS -I${srcdir}/c++ -I${srcdir}/menu"
	test -n "$cf_math" && CXXFLAGS="$CXXFLAGS -DETIP_NEEDS_${cf_math}"
	test -n "$cf_excp" && CXXFLAGS="$CXXFLAGS -DETIP_NEEDS_${cf_excp}"
AC_TRY_COMPILE([
#include <etip.h.in>
],[],[
	test -n "$cf_math" && AC_DEFINE_UNQUOTED(ETIP_NEEDS_${cf_math})
	test -n "$cf_excp" && AC_DEFINE_UNQUOTED(ETIP_NEEDS_${cf_excp})
	cf_result="$cf_math $cf_excp"
	break
],[])
done
done
AC_MSG_RESULT($cf_result)
CXXFLAGS="$cf_save_CXXFLAGS"
])
dnl ---------------------------------------------------------------------------
dnl Check for memmove, or a bcopy that can handle overlapping copy.  If neither
dnl is found, add our own version of memmove to the list of objects.
AC_DEFUN([CF_FUNC_MEMMOVE],
[
AC_CHECK_FUNC(memmove,,[
AC_CHECK_FUNC(bcopy,[
	AC_CACHE_CHECK(if bcopy does overlapping moves,cf_cv_good_bcopy,[
		AC_TRY_RUN([
int main() {
	static char data[] = "abcdefghijklmnopqrstuwwxyz";
	char temp[40];
	bcopy(data, temp, sizeof(data));
	bcopy(temp+10, temp, 15);
	bcopy(temp+5, temp+15, 10);
	exit (strcmp(temp, "klmnopqrstuwwxypqrstuwwxyz"));
}
		],
		[cf_cv_good_bcopy=yes],
		[cf_cv_good_bcopy=no],
		[cf_cv_good_bcopy=unknown])
		])
	],[cf_cv_good_bcopy=no])
	if test "$cf_cv_good_bcopy" = yes ; then
		AC_DEFINE(USE_OK_BCOPY)
	else
		AC_DEFINE(USE_MY_MEMMOVE)
	fi
])])dnl
dnl ---------------------------------------------------------------------------
dnl See if the poll function really works.  Some platforms have poll(), but
dnl it does not work for terminals or files.
AC_DEFUN([CF_FUNC_POLL],[
AC_CACHE_CHECK(if poll really works,cf_cv_working_poll,[
AC_TRY_RUN([
#include <stdio.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#else
#include <sys/poll.h>
#endif
int main() {
	struct pollfd myfds;
	int ret;

	myfds.fd = 0;
	myfds.events = POLLIN;

	ret = poll(&myfds, 1, 100);
	exit(ret != 0);
}],
	[cf_cv_working_poll=yes],
	[cf_cv_working_poll=no],
	[cf_cv_working_poll=unknown])])
test "$cf_cv_working_poll" = "yes" && AC_DEFINE(HAVE_WORKING_POLL)
])dnl
dnl ---------------------------------------------------------------------------
dnl Some old/broken variations define tcgetattr() only as a macro in
dnl termio(s).h
AC_DEFUN([CF_FUNC_TERMIOS],[
AC_REQUIRE([CF_STRUCT_TERMIOS])
AC_CACHE_CHECK(for tcgetattr, cf_cv_have_tcgetattr,[
AC_TRY_LINK([
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#define TTY struct termios
#else
#ifdef HAVE_TERMIO_H
#include <termio.h>
#define TTY struct termio
#endif
#endif
],[
TTY foo;
tcgetattr(1, &foo);],
[cf_cv_have_tcgetattr=yes],
[cf_cv_have_tcgetattr=no])])
test "$cf_cv_have_tcgetattr" = yes && AC_DEFINE(HAVE_TCGETATTR)
])dnl
dnl ---------------------------------------------------------------------------
dnl Test for availability of useful gcc __attribute__ directives to quiet
dnl compiler warnings.  Though useful, not all are supported -- and contrary
dnl to documentation, unrecognized directives cause older compilers to barf.
AC_DEFUN([CF_GCC_ATTRIBUTES],
[
if test "$GCC" = yes
then
cat > conftest.i <<EOF
#ifndef GCC_PRINTF
#define GCC_PRINTF 0
#endif
#ifndef GCC_SCANF
#define GCC_SCANF 0
#endif
#ifndef GCC_NORETURN
#define GCC_NORETURN /* nothing */
#endif
#ifndef GCC_UNUSED
#define GCC_UNUSED /* nothing */
#endif
EOF
if test "$GCC" = yes
then
	AC_CHECKING([for $CC __attribute__ directives])
cat > conftest.$ac_ext <<EOF
#line __oline__ "configure"
#include "confdefs.h"
#include "conftest.h"
#include "conftest.i"
#if	GCC_PRINTF
#define GCC_PRINTFLIKE(fmt,var) __attribute__((format(printf,fmt,var)))
#else
#define GCC_PRINTFLIKE(fmt,var) /*nothing*/
#endif
#if	GCC_SCANF
#define GCC_SCANFLIKE(fmt,var)  __attribute__((format(scanf,fmt,var)))
#else
#define GCC_SCANFLIKE(fmt,var)  /*nothing*/
#endif
extern void wow(char *,...) GCC_SCANFLIKE(1,2);
extern void oops(char *,...) GCC_PRINTFLIKE(1,2) GCC_NORETURN;
extern void foo(void) GCC_NORETURN;
int main(int argc GCC_UNUSED, char *argv[[]] GCC_UNUSED) { return 0; }
EOF
	for cf_attribute in scanf printf unused noreturn
	do
		CF_UPPER(CF_ATTRIBUTE,$cf_attribute)
		cf_directive="__attribute__(($cf_attribute))"
		echo "checking for $CC $cf_directive" 1>&AC_FD_CC
		case $cf_attribute in
		scanf|printf)
		cat >conftest.h <<EOF
#define GCC_$CF_ATTRIBUTE 1
EOF
			;;
		*)
		cat >conftest.h <<EOF
#define GCC_$CF_ATTRIBUTE $cf_directive
EOF
			;;
		esac
		if AC_TRY_EVAL(ac_compile); then
			test -n "$verbose" && AC_MSG_RESULT(... $cf_attribute)
			cat conftest.h >>confdefs.h
#		else
#			sed -e 's/__attr.*/\/*nothing*\//' conftest.h >>confdefs.h
		fi
	done
else
	fgrep define conftest.i >>confdefs.h
fi
rm -rf conftest*
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if the compiler supports useful warning options.  There's a few that
dnl we don't use, simply because they're too noisy:
dnl
dnl	-Wconversion (useful in older versions of gcc, but not in gcc 2.7.x)
dnl	-Wredundant-decls (system headers make this too noisy)
dnl	-Wtraditional (combines too many unrelated messages, only a few useful)
dnl	-Wwrite-strings (too noisy, but should review occasionally)
dnl	-pedantic
dnl
AC_DEFUN([CF_GCC_WARNINGS],
[
if test "$GCC" = yes
then
	cat > conftest.$ac_ext <<EOF
#line __oline__ "configure"
int main(int argc, char *argv[[]]) { return (argv[[argc-1]] == 0) ; }
EOF
	AC_CHECKING([for $CC warning options])
	cf_save_CFLAGS="$CFLAGS"
	EXTRA_CFLAGS="-W -Wall"
	cf_warn_CONST=""
	test "$with_ext_const" = yes && cf_warn_CONST="Wwrite-strings"
	for cf_opt in \
		Wbad-function-cast \
		Wcast-align \
		Wcast-qual \
		Winline \
		Wmissing-declarations \
		Wmissing-prototypes \
		Wnested-externs \
		Wpointer-arith \
		Wshadow \
		Wstrict-prototypes $cf_warn_CONST
	do
		CFLAGS="$cf_save_CFLAGS $EXTRA_CFLAGS -$cf_opt"
		if AC_TRY_EVAL(ac_compile); then
			test -n "$verbose" && AC_MSG_RESULT(... -$cf_opt)
			EXTRA_CFLAGS="$EXTRA_CFLAGS -$cf_opt"
			test "$cf_opt" = Wcast-qual && EXTRA_CFLAGS="$EXTRA_CFLAGS -DXTSTRINGDEFINES"
		fi
	done
	rm -f conftest*
	CFLAGS="$cf_save_CFLAGS"
fi
AC_SUBST(EXTRA_CFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl Verify that a test program compiles and runs with GNAT
dnl $cf_ada_make is set to the program that compiles/links
AC_DEFUN([CF_GNAT_TRY_RUN],
[
rm -f conftest*
cat >>conftest.ads <<CF_EOF
$1
CF_EOF
cat >>conftest.adb <<CF_EOF
$2
CF_EOF
if ( $cf_ada_make conftest 1>&AC_FD_CC 2>&1 ) ; then
   if ( ./conftest 1>&AC_FD_CC 2>&1 ) ; then
ifelse($3,,      :,[      $3])
ifelse($4,,,[   else
      $4])
   fi
ifelse($4,,,[else
   $4])
fi
rm -f conftest*
])dnl
dnl ---------------------------------------------------------------------------
dnl Verify Version of GNAT.
AC_DEFUN([CF_GNAT_VERSION],
[
cf_cv_gnat_version=`$cf_ada_make -v 2>&1 | grep '[[0-9]].[[0-9]][[0-9]]*' |\
  sed -e 's/[[^0-9 \.]]//g' | $AWK '{print $[1];}'`
case $cf_cv_gnat_version in
  3.1[[1-9]]*|3.[[2-9]]*|[[4-9]].*)
    cf_cv_prog_gnat_correct=yes
    ;;
  *) echo Unsupported GNAT version $cf_cv_gnat_version. Required is 3.11 or better. Disabling Ada95 binding.
     cf_cv_prog_gnat_correct=no
     ;;
esac
case $cf_cv_gnat_version in
  3.1*|[[4-9]].*)
      cf_compile_generics=generics
      cf_generic_objects="\$(GENOBJS)"
      ;;
  *)  cf_compile_generics=
      cf_generic_objects=
      ;;
esac
])
dnl ---------------------------------------------------------------------------
dnl If we're trying to use g++, test if libg++ is installed (a rather common
dnl problem :-).  If we have the compiler but no library, we'll be able to
dnl configure, but won't be able to build the c++ demo program.
AC_DEFUN([CF_GPP_LIBRARY],
[
cf_cxx_library=unknown
case $cf_cv_system_name in #(vi
os2*) #(vi
	cf_gpp_libname=gpp
	;;
*)
	cf_gpp_libname=g++
	;;
esac
if test "$ac_cv_prog_gxx" = yes; then
	AC_MSG_CHECKING([for lib$cf_gpp_libname])
	cf_save="$LIBS"
	LIBS="$LIBS -l$cf_gpp_libname"
	AC_TRY_LINK([
#include <$cf_gpp_libname/builtin.h>
	],
	[two_arg_error_handler_t foo2 = lib_error_handler],
	[cf_cxx_library=yes
	 CXXLIBS="$CXXLIBS -l$cf_gpp_libname"
	 if test "$cf_gpp_libname" = cpp ; then
	    AC_DEFINE(HAVE_GPP_BUILTIN_H)
	 else
	    AC_DEFINE(HAVE_GXX_BUILTIN_H)
	 fi],
	[AC_TRY_LINK([
#include <builtin.h>
	],
	[two_arg_error_handler_t foo2 = lib_error_handler],
	[cf_cxx_library=yes
	 CXXLIBS="$CXXLIBS -l$cf_gpp_libname"
	 AC_DEFINE(HAVE_BUILTIN_H)],
	[cf_cxx_library=no])])
	LIBS="$cf_save"
	AC_MSG_RESULT($cf_cxx_library)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Insert text into the help-message, for readability, from AC_ARG_WITH.
AC_DEFUN([CF_HELP_MESSAGE],
[AC_DIVERT_HELP([$1])dnl
])dnl
dnl ---------------------------------------------------------------------------
dnl Construct the list of include-options according to whether we're building
dnl in the source directory or using '--srcdir=DIR' option.  If we're building
dnl with gcc, don't append the includedir if it happens to be /usr/include,
dnl since that usually breaks gcc's shadow-includes.
AC_DEFUN([CF_INCLUDE_DIRS],
[
CPPFLAGS="$CPPFLAGS -I. -I../include"
if test "$srcdir" != "."; then
	CPPFLAGS="$CPPFLAGS -I\$(srcdir)/../include"
fi
if test "$GCC" != yes; then
	CPPFLAGS="$CPPFLAGS -I\$(includedir)"
elif test "$includedir" != "/usr/include"; then
	if test "$includedir" = '${prefix}/include' ; then
		if test $prefix != /usr ; then
			CPPFLAGS="$CPPFLAGS -I\$(includedir)"
		fi
	else
		CPPFLAGS="$CPPFLAGS -I\$(includedir)"
	fi
fi
AC_SUBST(CPPFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if we have either a function or macro for 'isascii()'.
AC_DEFUN([CF_ISASCII],
[
AC_MSG_CHECKING(for isascii)
AC_CACHE_VAL(cf_cv_have_isascii,[
	AC_TRY_LINK([#include <ctype.h>],[int x = isascii(' ')],
	[cf_cv_have_isascii=yes],
	[cf_cv_have_isascii=no])
])dnl
AC_MSG_RESULT($cf_cv_have_isascii)
test "$cf_cv_have_isascii" = yes && AC_DEFINE(HAVE_ISASCII)
])dnl
dnl ---------------------------------------------------------------------------
dnl Compute the library-prefix for the given host system
dnl $1 = variable to set
AC_DEFUN([CF_LIB_PREFIX],
[
	case $cf_cv_system_name in
	OS/2*)	LIB_PREFIX=''     ;;
	os2*)	LIB_PREFIX=''     ;;
	*)	LIB_PREFIX='lib'  ;;
	esac
ifelse($1,,,[$1=$LIB_PREFIX])
	AC_SUBST(LIB_PREFIX)
])dnl
dnl ---------------------------------------------------------------------------
dnl Append definitions and rules for the given models to the subdirectory
dnl Makefiles, and the recursion rule for the top-level Makefile.  If the
dnl subdirectory is a library-source directory, modify the LIBRARIES list in
dnl the corresponding makefile to list the models that we'll generate.
dnl
dnl For shared libraries, make a list of symbolic links to construct when
dnl generating each library.  The convention used for Linux is the simplest
dnl one:
dnl	lib<name>.so	->
dnl	lib<name>.so.<major>	->
dnl	lib<name>.so.<maj>.<minor>
AC_DEFUN([CF_LIB_RULES],
[
CF_LIB_PREFIX(cf_prefix)
AC_REQUIRE([CF_SUBST_NCURSES_VERSION])
for cf_dir in $SRC_SUBDIRS
do
	if test -f $srcdir/$cf_dir/modules; then

		cf_libs_to_make=
		for cf_item in $CF_LIST_MODELS
		do
			CF_LIB_SUFFIX($cf_item,cf_suffix)
			if test $cf_item = shared ; then
			if test "$cf_cv_do_symlinks" = yes ; then
				case "$cf_cv_shlib_version" in #(vi
				rel) cf_suffix="$cf_suffix"'.$(REL_VERSION)' ;; #(vi
				abi) cf_suffix="$cf_suffix"'.$(ABI_VERSION)' ;;
				esac
			fi
			fi
			cf_libs_to_make="$cf_libs_to_make ../lib/${cf_prefix}${cf_dir}${cf_suffix}"
		done

		if test $cf_dir = ncurses ; then
			case "$LIB_SUBSETS" in #(vi
			termlib+*) #(vi
				;;
			*) #(vi
				cf_item=`echo $cf_libs_to_make |sed -e s/$LIB_NAME/$TINFO_NAME/g`
				cf_libs_to_make="$cf_item $cf_libs_to_make"
				;;
			esac
		fi

		sed -e "s@\@LIBS_TO_MAKE\@@$cf_libs_to_make@" \
			$cf_dir/Makefile >$cf_dir/Makefile.out
		mv $cf_dir/Makefile.out $cf_dir/Makefile

		$AWK -f $srcdir/mk-0th.awk \
			name=$cf_dir \
			$srcdir/$cf_dir/modules >>$cf_dir/Makefile

		for cf_item in $CF_LIST_MODELS
		do
			echo 'Appending rules for '$cf_item' model ('$cf_dir')'
			CF_UPPER(CF_ITEM,$cf_item)
			CF_LIB_SUFFIX($cf_item,cf_suffix)
			CF_OBJ_SUBDIR($cf_item,cf_subdir)

			# These dependencies really are for development, not
			# builds, but they are useful in porting, too.
			cf_depend="../include/ncurses_cfg.h"
			if test "$srcdir" = "."; then
				cf_reldir="."
			else
				cf_reldir="\$(srcdir)"
			fi

			if test -f $srcdir/$cf_dir/$cf_dir.priv.h; then
				cf_depend="$cf_depend $cf_reldir/$cf_dir.priv.h"
			elif test -f $srcdir/$cf_dir/curses.priv.h; then
				cf_depend="$cf_depend $cf_reldir/curses.priv.h"
			fi

			for cf_subset in $LIB_SUBSETS
			do
			$AWK -f $srcdir/mk-1st.awk \
				name=$cf_dir \
				traces=$LIB_TRACING \
				MODEL=$CF_ITEM \
				model=$cf_subdir \
				prefix=$cf_prefix \
				suffix=$cf_suffix \
				subset=$cf_subset \
				ShlibVer=$cf_cv_shlib_version \
				DoLinks=$cf_cv_do_symlinks \
				rmSoLocs=$cf_cv_rm_so_locs \
				ldconfig="$LDCONFIG" \
				overwrite=$WITH_OVERWRITE \
				depend="$cf_depend" \
				target="$target" \
				$srcdir/$cf_dir/modules >>$cf_dir/Makefile
			test $cf_dir = ncurses && WITH_OVERWRITE=no
			$AWK -f $srcdir/mk-2nd.awk \
				name=$cf_dir \
				traces=$LIB_TRACING \
				MODEL=$CF_ITEM \
				model=$cf_subdir \
				subset=$cf_subset \
				srcdir=$srcdir \
				echo=$WITH_ECHO \
				$srcdir/$cf_dir/modules >>$cf_dir/Makefile
			done
		done
	fi

	echo '	cd '$cf_dir' && $(MAKE) $(CF_MFLAGS) [$]@' >>Makefile
done

for cf_dir in $SRC_SUBDIRS
do
	if test -f $cf_dir/Makefile ; then
		case "$cf_dir" in
		Ada95) #(vi
			echo 'libs \' >> Makefile
			echo 'install.libs \' >> Makefile
			echo 'uninstall.libs ::' >> Makefile
			echo '	cd '$cf_dir' && $(MAKE) $(CF_MFLAGS) [$]@' >> Makefile
			;;
		esac
	fi

	if test -f $srcdir/$cf_dir/modules; then
		echo >> Makefile
		if test -f $srcdir/$cf_dir/headers; then
cat >> Makefile <<CF_EOF
install.includes \\
uninstall.includes \\
CF_EOF
		fi
if test "$cf_dir" != "c++" ; then
echo 'lint \' >> Makefile
fi
cat >> Makefile <<CF_EOF
libs \\
lintlib \\
install.libs \\
uninstall.libs \\
install.$cf_dir \\
uninstall.$cf_dir ::
	cd $cf_dir && \$(MAKE) \$(CF_MFLAGS) \[$]@
CF_EOF
	elif test -f $srcdir/$cf_dir/headers; then
cat >> Makefile <<CF_EOF

libs \\
install.libs \\
uninstall.libs \\
install.includes \\
uninstall.includes ::
	cd $cf_dir && \$(MAKE) \$(CF_MFLAGS) \[$]@
CF_EOF
fi
done

cat >> Makefile <<CF_EOF

install.data \
uninstall.data ::
	cd misc && \$(MAKE) \$(CF_MFLAGS) \[$]@

install.man \
uninstall.man ::
	cd man && \$(MAKE) \$(CF_MFLAGS) \[$]@

distclean ::
	rm -f config.cache config.log config.status Makefile include/ncurses_cfg.h
	rm -f headers.sh headers.sed
	rm -rf \$(DIRS_TO_MAKE)
CF_EOF

# Special case: tack's manpage lives in its own directory.
if test -d tack ; then
if test -f $srcdir/$tack.h; then
cat >> Makefile <<CF_EOF

install.man \
uninstall.man ::
	cd tack && \$(MAKE) \$(CF_MFLAGS) \[$]@
CF_EOF
fi
fi

dnl If we're installing into a subdirectory of /usr/include, etc., we should
dnl prepend the subdirectory's name to the "#include" paths.  It won't hurt
dnl anything, and will make it more standardized.  It's awkward to decide this
dnl at configuration because of quoting, so we'll simply make all headers
dnl installed via a script that can do the right thing.

rm -f headers.sed headers.sh

dnl ( generating this script makes the makefiles a little tidier :-)
echo creating headers.sh
cat >headers.sh <<CF_EOF
#! /bin/sh
# This shell script is generated by the 'configure' script.  It is invoked in a
# subdirectory of the build tree.  It generates a sed-script in the parent
# directory that is used to adjust includes for header files that reside in a
# subdirectory of /usr/include, etc.
PRG=""
while test \[$]# != 3
do
PRG="\$PRG \[$]1"; shift
done
DST=\[$]1
REF=\[$]2
SRC=\[$]3
TMPSRC=\${TMPDIR-/tmp}/\`basename \$SRC\`\$\$
TMPSED=\${TMPDIR-/tmp}/headers.sed\$\$
echo installing \$SRC in \$DST
CF_EOF
if test $WITH_CURSES_H = yes; then
	cat >>headers.sh <<CF_EOF
case \$DST in
/*/include/*)
	END=\`basename \$DST\`
	for i in \`cat \$REF/../*/headers |fgrep -v "#"\`
	do
		NAME=\`basename \$i\`
		echo "s/<\$NAME>/<\$END\/\$NAME>/" >> \$TMPSED
	done
	;;
*)
	echo "" >> \$TMPSED
	;;
esac
CF_EOF
else
	cat >>headers.sh <<CF_EOF
case \$DST in
/*/include/*)
	END=\`basename \$DST\`
	for i in \`cat \$REF/../*/headers |fgrep -v "#"\`
	do
		NAME=\`basename \$i\`
		if test "\$NAME" = "curses.h"
		then
			echo "s/<curses.h>/<ncurses.h>/" >> \$TMPSED
			NAME=ncurses.h
		fi
		echo "s/<\$NAME>/<\$END\/\$NAME>/" >> \$TMPSED
	done
	;;
*)
	echo "s/<curses.h>/<ncurses.h>/" >> \$TMPSED
	;;
esac
CF_EOF
fi
cat >>headers.sh <<CF_EOF
rm -f \$TMPSRC
sed -f \$TMPSED \$SRC > \$TMPSRC
NAME=\`basename \$SRC\`
CF_EOF
if test $WITH_CURSES_H != yes; then
	cat >>headers.sh <<CF_EOF
test "\$NAME" = "curses.h" && NAME=ncurses.h
CF_EOF
fi
cat >>headers.sh <<CF_EOF
eval \$PRG \$TMPSRC \$DST/\$NAME
rm -f \$TMPSRC \$TMPSED
CF_EOF

chmod 0755 headers.sh

for cf_dir in $SRC_SUBDIRS
do
	if test -f $srcdir/$cf_dir/headers; then
	cat >>$cf_dir/Makefile <<CF_EOF
\$(DESTDIR)\$(includedir) :
	\$(srcdir)/../mkinstalldirs \[$]@

install \\
install.libs \\
install.includes :: \$(AUTO_SRC) \$(DESTDIR)\$(includedir) \\
CF_EOF
		j=""
		for i in `cat $srcdir/$cf_dir/headers |fgrep -v "#"`
		do
			test -n "$j" && echo "		$j \\" >>$cf_dir/Makefile
			j=$i
		done
		echo "		$j" >>$cf_dir/Makefile
		for i in `cat $srcdir/$cf_dir/headers |fgrep -v "#"`
		do
			echo "	@ (cd \$(DESTDIR)\$(includedir) && rm -f `basename $i`) ; ../headers.sh \$(INSTALL_DATA) \$(DESTDIR)\$(includedir) \$(srcdir) $i" >>$cf_dir/Makefile
			test $i = curses.h && test $WITH_CURSES_H = yes && echo "	@ (cd \$(DESTDIR)\$(includedir) && rm -f ncurses.h && \$(LN_S) curses.h ncurses.h)" >>$cf_dir/Makefile
		done

	cat >>$cf_dir/Makefile <<CF_EOF

uninstall \\
uninstall.libs \\
uninstall.includes ::
CF_EOF
		for i in `cat $srcdir/$cf_dir/headers |fgrep -v "#"`
		do
			i=`basename $i`
			echo "	-@ (cd \$(DESTDIR)\$(includedir) && rm -f $i)" >>$cf_dir/Makefile
			test $i = curses.h && echo "	-@ (cd \$(DESTDIR)\$(includedir) && rm -f ncurses.h)" >>$cf_dir/Makefile
		done
	fi
done

])dnl
dnl ---------------------------------------------------------------------------
dnl Compute the library file-suffix from the given model name
dnl $1 = model name
dnl $2 = variable to set
dnl The variable $LIB_SUFFIX, if set, prepends the variable to set.
AC_DEFUN([CF_LIB_SUFFIX],
[
	AC_REQUIRE([CF_SUBST_NCURSES_VERSION])
	case $1 in
	libtool) $2='.la'  ;;
	normal)  $2='.a'   ;;
	debug)   $2='_g.a' ;;
	profile) $2='_p.a' ;;
	shared)
		case $cf_cv_system_name in
		hpux*)	$2='.sl'  ;;
		*)	$2='.so'  ;;
		esac
	esac
	test -n "$LIB_SUFFIX" && $2="${LIB_SUFFIX}[$]{$2}"
])dnl
dnl ---------------------------------------------------------------------------
dnl Compute the string to append to -library from the given model name
dnl $1 = model name
dnl $2 = variable to set
dnl The variable $LIB_SUFFIX, if set, prepends the variable to set.
AC_DEFUN([CF_LIB_TYPE],
[
	case $1 in
	libtool) $2=''   ;;
	normal)  $2=''   ;;
	debug)   $2='_g' ;;
	profile) $2='_p' ;;
	shared)  $2=''   ;;
	esac
	test -n "$LIB_SUFFIX" && $2="${LIB_SUFFIX}[$]{$2}"
])dnl
dnl ---------------------------------------------------------------------------
dnl Some systems have a non-ANSI linker that doesn't pull in modules that have
dnl only data (i.e., no functions), for example NeXT.  On those systems we'll
dnl have to provide wrappers for global tables to ensure they're linked
dnl properly.
AC_DEFUN([CF_LINK_DATAONLY],
[
AC_MSG_CHECKING([if data-only library module links])
AC_CACHE_VAL(cf_cv_link_dataonly,[
	rm -f conftest.a
	cat >conftest.$ac_ext <<EOF
#line __oline__ "configure"
int	testdata[[3]] = { 123, 456, 789 };
EOF
	if AC_TRY_EVAL(ac_compile) ; then
		mv conftest.o data.o && \
		( $AR $AR_OPTS conftest.a data.o ) 2>&5 1>/dev/null
	fi
	rm -f conftest.$ac_ext data.o
	cat >conftest.$ac_ext <<EOF
#line __oline__ "configure"
int	testfunc()
{
#if defined(NeXT)
	exit(1);	/* I'm told this linker is broken */
#else
	extern int testdata[[3]];
	return testdata[[0]] == 123
	   &&  testdata[[1]] == 456
	   &&  testdata[[2]] == 789;
#endif
}
EOF
	if AC_TRY_EVAL(ac_compile); then
		mv conftest.o func.o && \
		( $AR $AR_OPTS conftest.a func.o ) 2>&5 1>/dev/null
	fi
	rm -f conftest.$ac_ext func.o
	( eval $ac_cv_prog_RANLIB conftest.a ) 2>&5 >/dev/null
	cf_saveLIBS="$LIBS"
	LIBS="conftest.a $LIBS"
	AC_TRY_RUN([
	int main()
	{
		extern int testfunc();
		exit (!testfunc());
	}
	],
	[cf_cv_link_dataonly=yes],
	[cf_cv_link_dataonly=no],
	[cf_cv_link_dataonly=unknown])
	LIBS="$cf_saveLIBS"
	])
AC_MSG_RESULT($cf_cv_link_dataonly)
test "$cf_cv_link_dataonly" = no && AC_DEFINE(BROKEN_LINKER)
])dnl
dnl ---------------------------------------------------------------------------
dnl Most Unix systems have both link and symlink, a few don't have symlink.
dnl A few non-Unix systems implement symlink, but not link.
dnl A few non-systems implement neither (or have nonfunctional versions).
AC_DEFUN([CF_LINK_FUNCS],
[
AC_CHECK_FUNCS( \
	remove \
	unlink )

if test "$ac_cv_prog_cc_cross" = yes ; then
	AC_CHECK_FUNCS( \
		link \
		symlink )
else
	AC_CACHE_CHECK(if link/symlink functions work,cf_cv_link_funcs,[
		cf_cv_link_funcs=
		for cf_func in link symlink ; do
			AC_TRY_RUN([
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
int main()
{
	int fail = 0;
	char *src = "config.log";
	char *dst = "conftest.chk";
	struct stat src_sb;
	struct stat dst_sb;

	stat(src, &src_sb);
	fail = ($cf_func("config.log", "conftest.chk") < 0)
	    || (stat(dst, &dst_sb) < 0)
	    || (dst_sb.st_mtime != src_sb.st_mtime);
#ifdef HAVE_UNLINK
	unlink(dst);
#else
	remove(dst);
#endif
	exit (fail);
}
			],[
			cf_cv_link_funcs="$cf_cv_link_funcs $cf_func"
			eval 'ac_cv_func_'$cf_func'=yes'],[
			eval 'ac_cv_func_'$cf_func'=no'],[
			eval 'ac_cv_func_'$cf_func'=error'])
		done
		test -z "$cf_cv_link_funcs" && cf_cv_link_funcs=no
	])
	test "$ac_cv_func_link"    = yes && AC_DEFINE(HAVE_LINK)
	test "$ac_cv_func_symlink" = yes && AC_DEFINE(HAVE_SYMLINK)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Some 'make' programs support $(MAKEFLAGS), some $(MFLAGS), to pass 'make'
dnl options to lower-levels.  It's very useful for "make -n" -- if we have it.
dnl (GNU 'make' does both, something POSIX 'make', which happens to make the
dnl $(MAKEFLAGS) variable incompatible because it adds the assignments :-)
AC_DEFUN([CF_MAKEFLAGS],
[
AC_MSG_CHECKING([for makeflags variable])
AC_CACHE_VAL(cf_cv_makeflags,[
	cf_cv_makeflags=''
	for cf_option in '-$(MAKEFLAGS)' '$(MFLAGS)'
	do
		cat >cf_makeflags.tmp <<CF_EOF
all :
	@ echo '.$cf_option'
CF_EOF
		cf_result=`${MAKE-make} -k -f cf_makeflags.tmp 2>/dev/null`
		case "$cf_result" in
		.*k)
			cf_result=`${MAKE-make} -k -f cf_makeflags.tmp CC=cc 2>/dev/null`
			case "$cf_result" in
			.*CC=*)	cf_cv_makeflags=
				;;
			*)	cf_cv_makeflags=$cf_option
				;;
			esac
			break
			;;
		*)	echo no match "$cf_result"
			;;
		esac
	done
	rm -f cf_makeflags.tmp])
AC_MSG_RESULT($cf_cv_makeflags)
AC_SUBST(cf_cv_makeflags)
])dnl
dnl ---------------------------------------------------------------------------
dnl Generate tags/TAGS targets for makefiles.  Do not generate TAGS if we have
dnl a monocase filesystem.
AC_DEFUN([CF_MAKE_TAGS],[
AC_REQUIRE([CF_MIXEDCASE_FILENAMES])
AC_CHECK_PROG(MAKE_LOWER_TAGS, ctags, yes, no)

if test "$cf_cv_mixedcase" = yes ; then
	AC_CHECK_PROG(MAKE_UPPER_TAGS, etags, yes, no)
else
	MAKE_UPPER_TAGS=no
fi

if test "$MAKE_UPPER_TAGS" = yes ; then
	MAKE_UPPER_TAGS=
else
	MAKE_UPPER_TAGS="#"
fi
AC_SUBST(MAKE_UPPER_TAGS)

if test "$MAKE_LOWER_TAGS" = yes ; then
	MAKE_LOWER_TAGS=
else
	MAKE_LOWER_TAGS="#"
fi
AC_SUBST(MAKE_LOWER_TAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl Option to allow user to override automatic configuration of manpage format.
dnl There are several special cases.
AC_DEFUN([CF_MANPAGE_FORMAT],
[
AC_REQUIRE([CF_PATHSEP])
AC_MSG_CHECKING(format of man-pages)

AC_ARG_WITH(manpage-format,
	[  --with-manpage-format   specify manpage-format: gzip/compress/BSDI/normal and
                          optionally formatted, e.g., gzip,formatted],
	[cf_manpage_form=$withval],
	[cf_manpage_form=unknown])

case ".$cf_manpage_form" in
.gzip|.compress|.BSDI|.normal|.formatted) # (vi
  ;;
.unknown|.) # (vi
  if test -z "$MANPATH" ; then
    MANPATH="/usr/man:/usr/share/man"
  fi
  # look for the 'date' man-page (it's most likely to be installed!)
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}${PATHSEP}"
  cf_manpage_form=unknown
  for cf_dir in $MANPATH; do
    test -z "$cf_dir" && cf_dir=/usr/man
    for cf_name in $cf_dir/*/date.[[01]]* $cf_dir/*/date
    do
       cf_test=`echo $cf_name | sed -e 's/*//'`
       if test "x$cf_test" = "x$cf_name" ; then
	  case "$cf_name" in
	  *.gz) cf_manpage_form=gzip;;
	  *.Z)  cf_manpage_form=compress;;
	  *.0)	cf_manpage_form=BSDI,formatted;;
	  *)    cf_manpage_form=normal;;
	  esac
	  break
       fi
    done
    if test "$cf_manpage_form" != "unknown" ; then
       break
    fi
  done
  IFS="$ac_save_ifs"
  ;;
.*) # (vi
  AC_MSG_WARN(Unexpected manpage-format)
  ;;
esac

AC_MSG_RESULT($cf_manpage_form)
])dnl
dnl ---------------------------------------------------------------------------
dnl The Debian people have their own naming convention for manpages.  This
dnl option lets us override the name of the file containing renaming, or
dnl disable it altogether.
AC_DEFUN([CF_MANPAGE_RENAMES],
[
AC_MSG_CHECKING(for manpage renaming)

AC_ARG_WITH(manpage-renames,
	[  --with-manpage-renames  specify manpage-renaming],
	[cf_manpage_renames=$withval],
	[cf_manpage_renames=yes])

case ".$cf_manpage_renames" in #(vi
.no) #(vi
  ;;
.|.yes)
  # Debian 'man' program?
  if test -f /etc/debian_version ; then
    cf_manpage_renames=`cd $srcdir && pwd`/man/man_db.renames
  else
    cf_manpage_renames=no
  fi
  ;;
esac

if test "$cf_manpage_renames" != no ; then
  if test ! -f $cf_manpage_renames ; then
    AC_MSG_ERROR(not a filename: $cf_manpage_renames)
  fi

  test ! -d man && mkdir man

  # Construct a sed-script to perform renaming within man-pages
  if test -n "$cf_manpage_renames" ; then
    test ! -d man && mkdir man
    $srcdir/man/make_sed.sh $cf_manpage_renames >man/edit_man.sed
  fi
fi

AC_MSG_RESULT($cf_manpage_renames)
])dnl
dnl ---------------------------------------------------------------------------
dnl Some people expect each tool to make all aliases for manpages in the
dnl man-directory.  This accommodates the older, less-capable implementations
dnl of 'man', and is optional.
AC_DEFUN([CF_MANPAGE_SYMLINKS],
[
AC_MSG_CHECKING(for manpage symlinks)

AC_ARG_WITH(manpage-symlinks,
	[  --with-manpage-symlinks specify manpage-symlinks],
	[cf_manpage_symlinks=$withval],
	[cf_manpage_symlinks=yes])

AC_MSG_RESULT($cf_manpage_symlinks)
])dnl
dnl ---------------------------------------------------------------------------
dnl This option causes manpages to be run through tbl(1) to generate tables
dnl correctly.
AC_DEFUN([CF_MANPAGE_TBL],
[
AC_MSG_CHECKING(for manpage tbl)

AC_ARG_WITH(manpage-tbl,
	[  --with-manpage-tbl      specify manpage processing with tbl],
	[cf_manpage_tbl=$withval],
	[cf_manpage_tbl=no])

AC_MSG_RESULT($cf_manpage_tbl)
])dnl
dnl ---------------------------------------------------------------------------
dnl Try to determine if the man-pages on the system are compressed, and if
dnl so, what format is used.  Use this information to construct a script that
dnl will install man-pages.
AC_DEFUN([CF_MAN_PAGES],
[
CF_HELP_MESSAGE(Options to Specify How Manpages are Installed:)
CF_MANPAGE_FORMAT
CF_MANPAGE_RENAMES
CF_MANPAGE_SYMLINKS
CF_MANPAGE_TBL

  if test "$prefix" = "NONE" ; then
     cf_prefix="$ac_default_prefix"
  else
     cf_prefix="$prefix"
  fi

  case "$cf_manpage_form" in # (vi
  *formatted*) # (vi
    cf_subdir='$mandir/cat'
    cf_format=yes
    ;;
  *)
    cf_subdir='$mandir/man'
    cf_format=no
    ;;
  esac

test ! -d man && mkdir man
cat >man/edit_man.sh <<CF_EOF
#! /bin/sh
# this script is generated by the configure-script
prefix="$cf_prefix"
datadir="$datadir"
TERMINFO="$TERMINFO"
MKDIRS="`cd $srcdir && pwd`/mkinstalldirs"
INSTALL="$INSTALL"
INSTALL_DATA="$INSTALL_DATA"
transform="$program_transform_name"

TMP=\${TMPDIR-/tmp}/man\$\$
trap "rm -f \$TMP" 0 1 2 5 15

verb=\[$]1
shift

mandir=\[$]1
shift

srcdir=\[$]1
shift

for i in \[$]* ; do
case \$i in #(vi
*.orig|*.rej) ;; #(vi
*.[[0-9]]*)
	section=\`expr "\$i" : '.*\\.\\([[0-9]]\\)[[xm]]*'\`;
	if test \$verb = installing ; then
	if test ! -d $cf_subdir\${section} ; then
		\$MKDIRS $cf_subdir\$section
	fi
	fi
	aliases=
	source=\`basename \$i\`
	inalias=\$source
	test ! -f \$inalias && inalias="\$srcdir/\$inalias"
	if test ! -f \$inalias ; then
		echo .. skipped \$source
		continue
	fi
CF_EOF
if test "$cf_manpage_symlinks" = yes ; then
cat >>man/edit_man.sh <<CF_EOF
	aliases=\`sed -f \$srcdir/manlinks.sed \$inalias | sort -u\`
CF_EOF
fi

if test "$cf_manpage_renames" = no ; then
cat >>man/edit_man.sh <<CF_EOF
	# perform program transformations for section 1 man pages
	if test \$section = 1 ; then
		target=$cf_subdir\${section}/\`echo \$source|sed "\${transform}"\`
	else
		target=$cf_subdir\${section}/\$source
	fi
CF_EOF
else
cat >>man/edit_man.sh <<CF_EOF
	target=\`grep "^\$source" $cf_manpage_renames | $AWK '{print \[$]2}'\`
	if test -z "\$target" ; then
		echo '? missing rename for '\$source
		target="\$source"
	fi
	target="$cf_subdir\${section}/\${target}"
CF_EOF
fi

	# replace variables in man page
	ifelse($1,,,[
	for cf_name in $1
	do
cat >>man/edit_man.sh <<CF_EOF
	prog_$cf_name=\`echo $cf_name|sed "\${transform}"\`
CF_EOF
	done
	])
cat >>man/edit_man.sh <<CF_EOF
	sed	-e "s,@DATADIR@,\$datadir," \\
		-e "s,@TERMINFO@,\$TERMINFO," \\
CF_EOF
	ifelse($1,,,[
	for cf_name in $1
	do
		cf_NAME=`echo "$cf_name" | sed y%abcdefghijklmnopqrstuvwxyz./-%ABCDEFGHIJKLMNOPQRSTUVWXYZ___%`
cat >>man/edit_man.sh <<CF_EOF
		-e "s,@$cf_NAME@,\$prog_$cf_name," \\
CF_EOF
	done
	])
cat >>man/edit_man.sh <<CF_EOF
		< \$i >\$TMP
CF_EOF
if test $cf_manpage_tbl = yes ; then
cat >>man/edit_man.sh <<CF_EOF
	tbl \$TMP >\$TMP.out
	mv \$TMP.out \$TMP
CF_EOF
fi
if test $with_curses_h != yes ; then
cat >>man/edit_man.sh <<CF_EOF
	sed -e "/\#[    ]*include/s,curses.h,ncurses.h," < \$TMP >\$TMP.out
	mv \$TMP.out \$TMP
CF_EOF
fi
if test $cf_format = yes ; then
cat >>man/edit_man.sh <<CF_EOF
	nroff -man \$TMP >\$TMP.out
	mv \$TMP.out \$TMP
CF_EOF
fi
case "$cf_manpage_form" in #(vi
*compress*) #(vi
cat >>man/edit_man.sh <<CF_EOF
	if test \$verb = installing ; then
	if ( compress -f \$TMP )
	then
		mv \$TMP.Z \$TMP
	fi
	fi
	target="\$target.Z"
CF_EOF
  ;;
*gzip*) #(vi
cat >>man/edit_man.sh <<CF_EOF
	if test \$verb = installing ; then
	if ( gzip -f \$TMP )
	then
		mv \$TMP.gz \$TMP
	fi
	fi
	target="\$target.gz"
CF_EOF
  ;;
*BSDI*)
cat >>man/edit_man.sh <<CF_EOF
	# BSDI installs only .0 suffixes in the cat directories
	target="\`echo \$target|sed -e 's/\.[[1-9]]\+.\?/.0/'\`"
CF_EOF
  ;;
esac
cat >>man/edit_man.sh <<CF_EOF
	suffix=\`basename \$target | sed -e 's/^[[^.]]*//'\`
	if test \$verb = installing ; then
		echo \$verb \$target
		\$INSTALL_DATA \$TMP \$target
		test -n "\$aliases" && (
			cd $cf_subdir\${section} && (
				target=\`basename \$target\`
				for cf_alias in \$aliases
				do
					if test \$section = 1 ; then
						cf_alias=\`echo \$cf_alias|sed "\${transform}"\`
					fi

					if test -f \$cf_alias\${suffix} ; then
						if ( cmp -s \$target \$cf_alias\${suffix} )
						then
							:
						else
							echo .. \$verb alias \$cf_alias\${suffix}
							rm -f \$cf_alias\${suffix}
							$LN_S \$target \$cf_alias\${suffix}
						fi
					else
						echo .. \$verb alias \$cf_alias\${suffix}
						rm -f \$cf_alias\${suffix}
						$LN_S \$target \$cf_alias\${suffix}
					fi
				done
			)
		)
	elif test \$verb = removing ; then
		echo \$verb \$target
		rm -f \$target
		test -n "\$aliases" && (
			cd $cf_subdir\${section} && (
				for cf_alias in \$aliases
				do
					if test \$section = 1 ; then
						cf_alias=\`echo \$cf_alias|sed "\${transform}"\`
					fi

					echo .. \$verb alias \$cf_alias\${suffix}
					rm -f \$cf_alias\${suffix}
				done
			)
		)
	else
#		echo ".hy 0"
		cat \$TMP
	fi
	;;
esac
done
exit 0
CF_EOF
chmod 755 man/edit_man.sh

])dnl
dnl ---------------------------------------------------------------------------
dnl Checks for libraries.  At least one UNIX system, Apple Macintosh
dnl Rhapsody 5.5, does not have -lm.  We cannot use the simpler
dnl AC_CHECK_LIB(m,sin), because that fails for C++.
AC_DEFUN([CF_MATH_LIB],
[
AC_CACHE_CHECK(if -lm needed for math functions,
	cf_cv_need_libm,[
	AC_TRY_LINK([
	#include <stdio.h>
	#include <math.h>
	],
	[double x = rand(); printf("result = %g\n", ]ifelse($2,,sin(x),$2)[)],
	[cf_cv_need_libm=no],
	[cf_cv_need_libm=yes])])
if test "$cf_cv_need_libm" = yes
then
ifelse($1,,[
	LIBS="$LIBS -lm"
],[$1=-lm])
fi
])
dnl ---------------------------------------------------------------------------
dnl Check if the file-system supports mixed-case filenames.  If we're able to
dnl create a lowercase name and see it as uppercase, it doesn't support that.
AC_DEFUN([CF_MIXEDCASE_FILENAMES],
[
AC_CACHE_CHECK(if filesystem supports mixed-case filenames,cf_cv_mixedcase,[
	rm -f conftest CONFTEST
	echo test >conftest
	if test -f CONFTEST ; then
		cf_cv_mixedcase=no
	else
		cf_cv_mixedcase=yes
	fi
	rm -f conftest CONFTEST
])
test "$cf_cv_mixedcase" = yes && AC_DEFINE(MIXEDCASE_FILENAMES)
])dnl
dnl ---------------------------------------------------------------------------
dnl Compute the object-directory name from the given model name
AC_DEFUN([CF_OBJ_SUBDIR],
[
	case $1 in
	libtool) $2='obj_lo'  ;;
	normal)  $2='objects' ;;
	debug)   $2='obj_g' ;;
	profile) $2='obj_p' ;;
	shared)  $2='obj_s' ;;
	esac
])dnl
dnl ---------------------------------------------------------------------------
dnl Provide a value for the $PATH and similar separator
AC_DEFUN([CF_PATHSEP],
[
	case $cf_cv_system_name in
	os2*)	PATHSEP=';'  ;;
	*)	PATHSEP=':'  ;;
	esac
ifelse($1,,,[$1=$PATHSEP])
	AC_SUBST(PATHSEP)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check the argument to see that it looks like a pathname.  Rewrite it if it
dnl begins with one of the prefix/exec_prefix variables, and then again if the
dnl result begins with 'NONE'.  This is necessary to workaround autoconf's
dnl delayed evaluation of those symbols.
AC_DEFUN([CF_PATH_SYNTAX],[
case ".[$]$1" in #(vi
./*) #(vi
  ;;
.[[a-zA-Z]]:[[\\/]]*) #(vi OS/2 EMX
  ;;
.\[$]{*prefix}*) #(vi
  eval $1="[$]$1"
  case ".[$]$1" in #(vi
  .NONE/*)
    $1=`echo [$]$1 | sed -e s@NONE@$ac_default_prefix@`
    ;;
  esac
  ;; #(vi
.NONE/*)
  $1=`echo [$]$1 | sed -e s@NONE@$ac_default_prefix@`
  ;;
*)
  AC_ERROR(expected a pathname, not "[$]$1")
  ;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl Compute $PROG_EXT, used for non-Unix ports, such as OS/2 EMX.
AC_DEFUN([CF_PROG_EXT],
[
AC_REQUIRE([CF_CHECK_CACHE])
PROG_EXT=
case $cf_cv_system_name in
os2*)
    # We make sure -Zexe is not used -- it would interfere with @PROG_EXT@
    CFLAGS="$CFLAGS -Zmt"
    CPPFLAGS="$CPPFLAGS -D__ST_MT_ERRNO__"
    CXXFLAGS="$CXXFLAGS -Zmt"
    LDFLAGS=`echo "$LDFLAGS -Zmt -Zcrtdll" | sed "s/-Zexe//g"`
    PROG_EXT=".exe"
    ;;
cygwin*)
    PROG_EXT=".exe"
    ;;
esac
AC_SUBST(PROG_EXT)
])dnl
dnl ---------------------------------------------------------------------------
dnl Force $INSTALL to be an absolute-path.  Otherwise, edit_man.sh and the
dnl misc/tabset install won't work properly.  Usually this happens only when
dnl using the fallback mkinstalldirs script
AC_DEFUN([CF_PROG_INSTALL],
[AC_PROG_INSTALL
case $INSTALL in
/*)
  ;;
*)
  CF_DIRNAME(cf_dir,$INSTALL)
  test -z "$cf_dir" && cf_dir=.
  INSTALL=`cd $cf_dir && pwd`/`echo $INSTALL | sed -e 's:^.*/::'`
  ;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl Attempt to determine if we've got one of the flavors of regular-expression
dnl code that we can support.
AC_DEFUN([CF_REGEX],
[
AC_MSG_CHECKING([for regular-expression headers])
AC_CACHE_VAL(cf_cv_regex,[
AC_TRY_LINK([#include <sys/types.h>
#include <regex.h>],[
	regex_t *p;
	int x = regcomp(p, "", 0);
	int y = regexec(p, "", 0, 0, 0);
	regfree(p);
	],[cf_cv_regex="regex.h"],[
	AC_TRY_LINK([#include <regexp.h>],[
		char *p = compile("", "", "", 0);
		int x = step("", "");
	],[cf_cv_regex="regexp.h"],[
		cf_save_LIBS="$LIBS"
		LIBS="-lgen $LIBS"
		AC_TRY_LINK([#include <regexpr.h>],[
			char *p = compile("", "", "");
			int x = step("", "");
		],[cf_cv_regex="regexpr.h"],[LIBS="$cf_save_LIBS"])])])
])
AC_MSG_RESULT($cf_cv_regex)
case $cf_cv_regex in
	regex.h)   AC_DEFINE(HAVE_REGEX_H_FUNCS) ;;
	regexp.h)  AC_DEFINE(HAVE_REGEXP_H_FUNCS) ;;
	regexpr.h) AC_DEFINE(HAVE_REGEXPR_H_FUNCS) ;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl Attempt to determine the appropriate CC/LD options for creating a shared
dnl library.
dnl
dnl Note: $(LOCAL_LDFLAGS) is used to link executables that will run within the
dnl build-tree, i.e., by making use of the libraries that are compiled in ../lib
dnl We avoid compiling-in a ../lib path for the shared library since that can
dnl lead to unexpected results at runtime.
dnl $(LOCAL_LDFLAGS2) has the same intention but assumes that the shared libraries
dnl are compiled in ../../lib
dnl
dnl The variable 'cf_cv_do_symlinks' is used to control whether we configure
dnl to install symbolic links to the rel/abi versions of shared libraries.
dnl
dnl The variable 'cf_cv_shlib_version' controls whether we use the rel or abi
dnl version when making symbolic links.
dnl
dnl Some loaders leave 'so_locations' lying around.  It's nice to clean up.
AC_DEFUN([CF_SHARED_OPTS],
[
	AC_REQUIRE([CF_SUBST_NCURSES_VERSION])
 	LOCAL_LDFLAGS=
 	LOCAL_LDFLAGS2=
	LD_SHARED_OPTS=
	INSTALL_LIB="-m 644"

	cf_cv_do_symlinks=no

	AC_MSG_CHECKING(if release/abi version should be used for shared libs)
	AC_ARG_WITH(shlib-version,
	[  --with-shlib-version=X  Specify rel or abi version for shared libs],
	[test -z "$withval" && withval=auto
	case $withval in #(vi
	yes) #(vi
		cf_cv_shlib_version=auto
		;;
	rel|abi|auto|no) #(vi
		cf_cv_shlib_version=$withval
		;;
	*)
		AC_ERROR([option value must be one of: rel, abi, auto or no])
		;;
	esac
	],[cf_cv_shlib_version=auto])
	AC_MSG_RESULT($cf_cv_shlib_version)

	cf_cv_rm_so_locs=no

	# Some less-capable ports of gcc support only -fpic
	CC_SHARED_OPTS=
	if test "$GCC" = yes
	then
		AC_MSG_CHECKING(which $CC option to use)
		cf_save_CFLAGS="$CFLAGS"
		for CC_SHARED_OPTS in -fPIC -fpic ''
		do
			CFLAGS="$cf_save_CFLAGS $CC_SHARED_OPTS"
			AC_TRY_COMPILE([#include <stdio.h>],[int x = 1],[break],[])
		done
		AC_MSG_RESULT($CC_SHARED_OPTS)
		CFLAGS="$cf_save_CFLAGS"
	fi

	case $cf_cv_system_name in
	beos*)
		MK_SHARED_LIB='$(CC) -o $[@] -Xlinker -soname=`basename $[@]` -nostart -e 0'
		;;
	hpux*)
		# (tested with gcc 2.7.2 -- I don't have c89)
		if test "$GCC" = yes; then
			LD_SHARED_OPTS='-Xlinker +b -Xlinker $(libdir)'
		else
			CC_SHARED_OPTS='+Z'
			LD_SHARED_OPTS='-Wl,+b,$(libdir)'
		fi
		MK_SHARED_LIB='$(LD) +b $(libdir) -b -o $[@]'
		# HP-UX shared libraries must be executable, and should be
		# readonly to exploit a quirk in the memory manager.
		INSTALL_LIB="-m 555"
		;;
	irix*)
		if test "$cf_cv_ld_rpath" = yes ; then
			cf_ld_rpath_opt="-Wl,-rpath,"
			EXTRA_LDFLAGS="-Wl,-rpath,\$(libdir) $EXTRA_LDFLAGS"
		fi
		# tested with IRIX 5.2 and 'cc'.
		if test "$GCC" != yes; then
			CC_SHARED_OPTS='-KPIC'
		fi
		MK_SHARED_LIB='$(LD) -shared -rdata_shared -soname `basename $[@]` -o $[@]'
		cf_cv_rm_so_locs=yes
		;;
	linux*|gnu*)
		if test "$DFT_LWR_MODEL" = "shared" ; then
 			LOCAL_LDFLAGS="-Wl,-rpath,`pwd`/lib"
 			LOCAL_LDFLAGS2="$LOCAL_LDFLAGS"
		fi
		if test "$cf_cv_ld_rpath" = yes ; then
			cf_ld_rpath_opt="-Wl,-rpath,"
			EXTRA_LDFLAGS="$LOCAL_LDFLAGS $EXTRA_LDFLAGS"
		fi
		test "$cf_cv_shlib_version" = auto && cf_cv_shlib_version=rel
		MK_SHARED_LIB='$(CC) -shared -Wl,-soname,`basename $[@] .$(REL_VERSION)`.$(ABI_VERSION),-stats,-lc -o $[@]'
		;;
	openbsd2*)
		CC_SHARED_OPTS="$CC_SHARED_OPTS -DPIC"
		MK_SHARED_LIB='$(LD) -Bshareable -soname,`basename $[@].$(ABI_VERSION)` -o $[@]'
		;;
	openbsd*|freebsd*)
		CC_SHARED_OPTS="$CC_SHARED_OPTS -DPIC"
		MK_SHARED_LIB='$(LD) -Bshareable -o $[@]'
		test "$cf_cv_shlib_version" = auto && cf_cv_shlib_version=rel
		;;
	netbsd*)
		CC_SHARED_OPTS="$CC_SHARED_OPTS -DPIC"
		test "$cf_cv_ld_rpath" = yes && cf_ld_rpath_opt="-Wl,-rpath,"
		if test "$DFT_LWR_MODEL" = "shared" && test "$cf_cv_ld_rpath" = yes ; then
 			LOCAL_LDFLAGS="-Wl,-rpath,`pwd`/lib"
 			LOCAL_LDFLAGS2="$LOCAL_LDFLAGS"
			EXTRA_LDFLAGS="-Wl,-rpath,\$(libdir) $EXTRA_LDFLAGS"
			MK_SHARED_LIB='$(CC) -shared -Wl,-soname,`basename $[@] .$(REL_VERSION)`.$(ABI_VERSION) -o $[@]'
			if test "$cf_cv_shlib_version" = auto; then
			if test ! -f /usr/libexec/ld.elf_so; then
				cf_cv_shlib_version=rel
			fi
			fi
		else
			MK_SHARED_LIB='$(LD) -Bshareable -o $[@]'
		fi
		;;
	osf*|mls+*)
		# tested with OSF/1 V3.2 and 'cc'
		# tested with OSF/1 V3.2 and gcc 2.6.3 (but the c++ demo didn't
		# link with shared libs).
 		MK_SHARED_LIB='$(LD) -set_version $(REL_VERSION):$(ABI_VERSION) -expect_unresolved "*" -shared -soname `basename $[@]`'
		case $host_os in
		osf4*)
 			MK_SHARED_LIB="${MK_SHARED_LIB} -msym"
			;;
		esac
		MK_SHARED_LIB="${MK_SHARED_LIB}"' -o $[@]'
		if test "$DFT_LWR_MODEL" = "shared" ; then
 			LOCAL_LDFLAGS="-Wl,-rpath,`pwd`/lib"
 			LOCAL_LDFLAGS2="$LOCAL_LDFLAGS"
		fi
		if test "$cf_cv_ld_rpath" = yes ; then
			cf_ld_rpath_opt="-rpath"
			# EXTRA_LDFLAGS="$LOCAL_LDFLAGS $EXTRA_LDFLAGS"
		fi
		cf_cv_rm_so_locs=yes
		;;
	sco3.2v5*)  # (also uw2* and UW7) hops 13-Apr-98
		# tested with osr5.0.5
		if test "$ac_cv_prog_gcc" != yes; then
			CC_SHARED_OPTS='-belf -KPIC'
		fi
		MK_SHARED_LIB='$(LD) -dy -G -h `basename $[@] .$(REL_VERSION)`.$(ABI_VERSION) -o [$]@'
		if test "$cf_cv_ld_rpath" = yes ; then
			# only way is to set LD_RUN_PATH but no switch for it
			RUN_PATH=$libdir
		fi
		test "$cf_cv_shlib_version" = auto && cf_cv_shlib_version=rel
		LINK_PROGS='LD_RUN_PATH=$(libdir)'
		LINK_TESTS='Pwd=`pwd`;LD_RUN_PATH=`dirname $${Pwd}`/lib'
		;;
	sunos4*)
		# tested with SunOS 4.1.1 and gcc 2.7.0
		if test "$ac_cv_prog_gcc" != yes; then
			CC_SHARED_OPTS='-KPIC'
		fi
		MK_SHARED_LIB='$(LD) -assert pure-text -o $[@]'
		test "$cf_cv_shlib_version" = auto && cf_cv_shlib_version=rel
		;;
	solaris2*)
		# tested with SunOS 5.5.1 (solaris 2.5.1) and gcc 2.7.2
		if test "$ac_cv_prog_gcc" != yes; then
			CC_SHARED_OPTS='-KPIC'
		fi
		MK_SHARED_LIB='$(LD) -dy -G -h `basename $[@] .$(REL_VERSION)`.$(ABI_VERSION) -o $[@]'
		if test "$DFT_LWR_MODEL" = "shared" ; then
			LOCAL_LDFLAGS="-R `pwd`/lib:\$(libdir)"
 			LOCAL_LDFLAGS2="$LOCAL_LDFLAGS"
		fi
		if test "$cf_cv_ld_rpath" = yes ; then
			cf_ld_rpath_opt="-R"
			EXTRA_LDFLAGS="$LOCAL_LDFLAGS $EXTRA_LDFLAGS"
		fi
		test "$cf_cv_shlib_version" = auto && cf_cv_shlib_version=rel
		;;
	sysv5uw7*|unix_sv*)
		# tested with UnixWare 7.1.0 (gcc 2.95.2 and cc)
		if test "$ac_cv_prog_gcc" != yes; then
			CC_SHARED_OPTS='-KPIC'
		fi
		MK_SHARED_LIB='$(LD) -d y -G -o [$]@'
		;;
	*)
		CC_SHARED_OPTS='unknown'
		MK_SHARED_LIB='echo unknown'
		;;
	esac

	# This works if the last tokens in $MK_SHARED_LIB are the -o target.
	case "$cf_cv_shlib_version" in #(vi
	rel|abi)
		case "$MK_SHARED_LIB" in #(vi
		*'-o $[@]')
			test "$cf_cv_do_symlinks" = no && cf_cv_do_symlinks=yes
			;;
		*)
			AC_MSG_WARN(ignored --with-shlib-version)
			;;
		esac
		;;
	esac

	if test -n "$cf_ld_rpath_opt" ; then
		AC_MSG_CHECKING(if we need a space after rpath option)
		cf_save_LIBS="$LIBS"
		LIBS="$LIBS ${cf_ld_rpath_opt}$libdir"
		AC_TRY_LINK(, , cf_rpath_space=no, cf_rpath_space=yes)
		LIBS="$cf_save_LIBS"
		AC_MSG_RESULT($cf_rpath_space)
		test "$cf_rpath_space" = yes && cf_ld_rpath_opt="$cf_ld_rpath_opt "
		MK_SHARED_LIB="$MK_SHARED_LIB $cf_ld_rpath_opt\$(libdir)"
	fi

	AC_SUBST(CC_SHARED_OPTS)
	AC_SUBST(LD_SHARED_OPTS)
	AC_SUBST(MK_SHARED_LIB)
	AC_SUBST(LINK_PROGS)
	AC_SUBST(LINK_TESTS)
	AC_SUBST(EXTRA_LDFLAGS)
	AC_SUBST(LOCAL_LDFLAGS)
	AC_SUBST(LOCAL_LDFLAGS2)
	AC_SUBST(INSTALL_LIB)
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for definitions & structures needed for window size-changing
dnl FIXME: check that this works with "snake" (HP-UX 10.x)
AC_DEFUN([CF_SIZECHANGE],
[
AC_REQUIRE([CF_STRUCT_TERMIOS])
AC_CACHE_CHECK(declaration of size-change, cf_cv_sizechange,[
    cf_cv_sizechange=unknown
    cf_save_CPPFLAGS="$CPPFLAGS"

for cf_opts in "" "NEED_PTEM_H"
do

    CPPFLAGS="$cf_save_CPPFLAGS"
    test -n "$cf_opts" && CPPFLAGS="$CPPFLAGS -D$cf_opts"
    AC_TRY_COMPILE([#include <sys/types.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#else
#ifdef HAVE_TERMIO_H
#include <termio.h>
#endif
#endif
#ifdef NEED_PTEM_H
/* This is a workaround for SCO:  they neglected to define struct winsize in
 * termios.h -- it's only in termio.h and ptem.h
 */
#include        <sys/stream.h>
#include        <sys/ptem.h>
#endif
#if !defined(sun) || !defined(HAVE_TERMIOS_H)
#include <sys/ioctl.h>
#endif
],[
#ifdef TIOCGSIZE
	struct ttysize win;	/* FIXME: what system is this? */
	int y = win.ts_lines;
	int x = win.ts_cols;
#else
#ifdef TIOCGWINSZ
	struct winsize win;
	int y = win.ws_row;
	int x = win.ws_col;
#else
	no TIOCGSIZE or TIOCGWINSZ
#endif /* TIOCGWINSZ */
#endif /* TIOCGSIZE */
	],
	[cf_cv_sizechange=yes],
	[cf_cv_sizechange=no])

	CPPFLAGS="$cf_save_CPPFLAGS"
	if test "$cf_cv_sizechange" = yes ; then
		echo "size-change succeeded ($cf_opts)" >&AC_FD_CC
		test -n "$cf_opts" && cf_cv_sizechange="$cf_opts"
		break
	fi
done
])
if test "$cf_cv_sizechange" != no ; then
	AC_DEFINE(HAVE_SIZECHANGE)
	case $cf_cv_sizechange in #(vi
	NEED*)
		AC_DEFINE_UNQUOTED($cf_cv_sizechange )
		;;
	esac
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl For each parameter, test if the source-directory exists, and if it contains
dnl a 'modules' file.  If so, add to the list $cf_cv_src_modules which we'll
dnl use in CF_LIB_RULES.
dnl
dnl This uses the configured value to make the lists SRC_SUBDIRS and
dnl SUB_MAKEFILES which are used in the makefile-generation scheme.
AC_DEFUN([CF_SRC_MODULES],
[
AC_MSG_CHECKING(for src modules)

# dependencies and linker-arguments for test-programs
TEST_DEPS="${LIB_DIR}/${LIB_PREFIX}${LIB_NAME}${DFT_DEP_SUFFIX} $TEST_DEPS"
if test "$DFT_LWR_MODEL" = "libtool"; then
	TEST_ARGS="${TEST_DEPS}"
else
	TEST_ARGS="-l${LIB_NAME}${DFT_ARG_SUFFIX} $TEST_ARGS"
fi

# dependencies and linker-arguments for utility-programs
PROG_ARGS="$TEST_ARGS"

cf_cv_src_modules=
for cf_dir in $1
do
	if test -f $srcdir/$cf_dir/modules; then

		# We may/may not have tack in the distribution, though the
		# makefile is.
		if test $cf_dir = tack ; then
			if test ! -f $srcdir/${cf_dir}/${cf_dir}.h; then
				continue
			fi
		fi

		if test -z "$cf_cv_src_modules"; then
			cf_cv_src_modules=$cf_dir
		else
			cf_cv_src_modules="$cf_cv_src_modules $cf_dir"
		fi

		# Make the ncurses_cfg.h file record the library interface files as
		# well.  These are header files that are the same name as their
		# directory.  Ncurses is the only library that does not follow
		# that pattern.
		if test $cf_dir = tack ; then
			continue
		elif test -f $srcdir/${cf_dir}/${cf_dir}.h; then
			CF_UPPER(cf_have_include,$cf_dir)
			AC_DEFINE_UNQUOTED(HAVE_${cf_have_include}_H)
			AC_DEFINE_UNQUOTED(HAVE_LIB${cf_have_include})
			TEST_DEPS="${LIB_DIR}/${LIB_PREFIX}${cf_dir}${DFT_DEP_SUFFIX} $TEST_DEPS"
			if test "$DFT_LWR_MODEL" = "libtool"; then
				TEST_ARGS="${TEST_DEPS}"
			else
				TEST_ARGS="-l${cf_dir}${DFT_ARG_SUFFIX} $TEST_ARGS"
			fi
		fi
	fi
done
AC_MSG_RESULT($cf_cv_src_modules)
TEST_ARGS="-L${LIB_DIR} -L\$(libdir) $TEST_ARGS"
AC_SUBST(TEST_DEPS)
AC_SUBST(TEST_ARGS)

PROG_ARGS="-L${LIB_DIR} -L\$(libdir) $PROG_ARGS"
AC_SUBST(PROG_ARGS)

SRC_SUBDIRS="man include"
for cf_dir in $cf_cv_src_modules
do
	SRC_SUBDIRS="$SRC_SUBDIRS $cf_dir"
done
SRC_SUBDIRS="$SRC_SUBDIRS misc test"
test "$cf_with_cxx_binding" != no && SRC_SUBDIRS="$SRC_SUBDIRS c++"

ADA_SUBDIRS=
if test "$cf_cv_prog_gnat_correct" = yes && test -f $srcdir/Ada95/Makefile.in; then
   SRC_SUBDIRS="$SRC_SUBDIRS Ada95"
   ADA_SUBDIRS="gen src samples"
fi

SUB_MAKEFILES=
for cf_dir in $SRC_SUBDIRS
do
	SUB_MAKEFILES="$SUB_MAKEFILES $cf_dir/Makefile"
done

if test -n "$ADA_SUBDIRS"; then
   for cf_dir in $ADA_SUBDIRS
   do
      SUB_MAKEFILES="$SUB_MAKEFILES Ada95/$cf_dir/Makefile"
   done
   AC_SUBST(ADA_SUBDIRS)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Check for -lstdc++, which is GNU's standard C++ library.
AC_DEFUN([CF_STDCPP_LIBRARY],
[
if test -n "$GXX" ; then
case $cf_cv_system_name in #(vi
os2*) #(vi
	cf_stdcpp_libname=stdcpp
	;;
*)
	cf_stdcpp_libname=stdc++
	;;
esac
AC_CACHE_CHECK(for library $cf_stdcpp_libname,cf_cv_libstdcpp,[
	cf_save="$LIBS"
	LIBS="$LIBS -l$cf_stdcpp_libname"
AC_TRY_LINK([
#include <strstream.h>],[
char buf[80];
strstreambuf foo(buf, sizeof(buf))
],
	[cf_cv_libstdcpp=yes],
	[cf_cv_libstdcpp=no])
	LIBS="$cf_save"
])
test "$cf_cv_libstdcpp" = yes && CXXLIBS="$CXXLIBS -l$cf_stdcpp_libname"
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl	Remove "-g" option from the compiler options
AC_DEFUN([CF_STRIP_G_OPT],
[$1=`echo ${$1} | sed -e 's/-g //' -e 's/-g$//'`])dnl
dnl ---------------------------------------------------------------------------
dnl Check if we need _POSIX_SOURCE defined to use struct sigaction.  We'll only
dnl do this if we've found the sigaction function.
dnl
dnl If needed, define SVR4_ACTION.
AC_DEFUN([CF_STRUCT_SIGACTION],[
if test "$ac_cv_func_sigaction" = yes; then
AC_MSG_CHECKING(whether sigaction needs _POSIX_SOURCE)
AC_TRY_COMPILE([
#include <sys/types.h>
#include <signal.h>],
	[struct sigaction act],
	[sigact_bad=no],
	[
AC_TRY_COMPILE([
#define _POSIX_SOURCE
#include <sys/types.h>
#include <signal.h>],
	[struct sigaction act],
	[sigact_bad=yes
	 AC_DEFINE(SVR4_ACTION)],
	 [sigact_bad=unknown])])
AC_MSG_RESULT($sigact_bad)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl Some machines require _POSIX_SOURCE to completely define struct termios.
dnl If so, define SVR4_TERMIO
AC_DEFUN([CF_STRUCT_TERMIOS],[
AC_CHECK_HEADERS( \
termio.h \
termios.h \
unistd.h \
)
if test "$ISC" = yes ; then
	AC_CHECK_HEADERS( sys/termio.h )
fi
if test "$ac_cv_header_termios_h" = yes ; then
	case "$CFLAGS $CPPFLAGS" in
	*-D_POSIX_SOURCE*)
		termios_bad=dunno ;;
	*)	termios_bad=maybe ;;
	esac
	if test "$termios_bad" = maybe ; then
	AC_MSG_CHECKING(whether termios.h needs _POSIX_SOURCE)
	AC_TRY_COMPILE([#include <termios.h>],
		[struct termios foo; int x = foo.c_iflag],
		termios_bad=no, [
		AC_TRY_COMPILE([
#define _POSIX_SOURCE
#include <termios.h>],
			[struct termios foo; int x = foo.c_iflag],
			termios_bad=unknown,
			termios_bad=yes AC_DEFINE(SVR4_TERMIO))
			])
	AC_MSG_RESULT($termios_bad)
	fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl	Shorthand macro for substituting things that the user may override
dnl	with an environment variable.
dnl
dnl	$1 = long/descriptive name
dnl	$2 = environment variable
dnl	$3 = default value
AC_DEFUN([CF_SUBST],
[AC_CACHE_VAL(cf_cv_subst_$2,[
AC_MSG_CHECKING(for $1 (symbol $2))
test -z "[$]$2" && $2=$3
AC_MSG_RESULT([$]$2)
AC_SUBST($2)
cf_cv_subst_$2=[$]$2])
$2=${cf_cv_subst_$2}
])dnl
dnl ---------------------------------------------------------------------------
dnl Get the version-number for use in shared-library naming, etc.
AC_DEFUN([CF_SUBST_NCURSES_VERSION],
[
NCURSES_MAJOR="`egrep '^NCURSES_MAJOR[[ 	]]*=' $srcdir/dist.mk | sed -e 's/^[[^0-9]]*//'`"
NCURSES_MINOR="`egrep '^NCURSES_MINOR[[ 	]]*=' $srcdir/dist.mk | sed -e 's/^[[^0-9]]*//'`"
NCURSES_PATCH="`egrep '^NCURSES_PATCH[[ 	]]*=' $srcdir/dist.mk | sed -e 's/^[[^0-9]]*//'`"
cf_cv_abi_version=${NCURSES_MAJOR}
cf_cv_rel_version=${NCURSES_MAJOR}.${NCURSES_MINOR}
dnl Show the computed version, for logging
AC_MSG_RESULT(Configuring NCURSES $cf_cv_rel_version ABI $cf_cv_abi_version (`date`))
dnl We need these values in the generated headers
AC_SUBST(NCURSES_MAJOR)
AC_SUBST(NCURSES_MINOR)
AC_SUBST(NCURSES_PATCH)
dnl We need these values in the generated makefiles
AC_SUBST(cf_cv_rel_version)
AC_SUBST(cf_cv_abi_version)
AC_SUBST(cf_cv_cc_bool_type)
AC_SUBST(cf_cv_builtin_bool)
AC_SUBST(cf_cv_type_of_bool)dnl
])dnl
dnl ---------------------------------------------------------------------------
dnl Check if we can include <sys/time.h> with <sys/select.h>; this breaks on
dnl older SCO configurations.
AC_DEFUN([CF_SYS_TIME_SELECT],
[
AC_MSG_CHECKING(if sys/time.h works with sys/select.h)
AC_CACHE_VAL(cf_cv_sys_time_select,[
AC_TRY_COMPILE([
#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
],[],[cf_cv_sys_time_select=yes],
     [cf_cv_sys_time_select=no])
     ])
AC_MSG_RESULT($cf_cv_sys_time_select)
test "$cf_cv_sys_time_select" = yes && AC_DEFINE(HAVE_SYS_TIME_SELECT)
])dnl
dnl ---------------------------------------------------------------------------
dnl Determine the type we should use for chtype (and attr_t, which is treated
dnl as the same thing).  We want around 32 bits, so on most machines want a
dnl long, but on newer 64-bit machines, probably want an int.  If we're using
dnl wide characters, we have to have a type compatible with that, as well.
AC_DEFUN([CF_TYPEOF_CHTYPE],
[
AC_REQUIRE([CF_UNSIGNED_LITERALS])
AC_MSG_CHECKING([for type of chtype])
AC_CACHE_VAL(cf_cv_typeof_chtype,[
		AC_TRY_RUN([
#ifdef USE_WIDEC_SUPPORT
#include <stddef.h>	/* we want wchar_t */
#define WANT_BITS 39
#else
#define WANT_BITS 31
#endif
#include <stdio.h>
int main()
{
	FILE *fp = fopen("cf_test.out", "w");
	if (fp != 0) {
		char *result = "long";
#ifdef USE_WIDEC_SUPPORT
		/*
		 * If wchar_t is smaller than a long, it must be an int or a
		 * short.  We prefer not to use a short anyway.
		 */
		if (sizeof(unsigned long) > sizeof(wchar_t))
			result = "int";
#endif
		if (sizeof(unsigned long) > sizeof(unsigned int)) {
			int n;
			unsigned int x;
			for (n = 0; n < WANT_BITS; n++) {
				unsigned int y = (x >> n);
				if (y != 1 || x == 0) {
					x = 0;
					break;
				}
			}
			/*
			 * If x is nonzero, an int is big enough for the bits
			 * that we want.
			 */
			result = (x != 0) ? "int" : "long";
		}
		fputs(result, fp);
		fclose(fp);
	}
	exit(0);
}
		],
		[cf_cv_typeof_chtype=`cat cf_test.out`],
		[cf_cv_typeof_chtype=long],
		[cf_cv_typeof_chtype=long])
		rm -f cf_test.out
	])
AC_MSG_RESULT($cf_cv_typeof_chtype)

AC_SUBST(cf_cv_typeof_chtype)
AC_DEFINE_UNQUOTED(TYPEOF_CHTYPE,$cf_cv_typeof_chtype)

cf_cv_1UL="1"
test "$cf_cv_unsigned_literals" = yes && cf_cv_1UL="${cf_cv_1UL}U"
test "$cf_cv_typeof_chtype"    = long && cf_cv_1UL="${cf_cv_1UL}L"
AC_SUBST(cf_cv_1UL)

])dnl
dnl ---------------------------------------------------------------------------
dnl
AC_DEFUN([CF_TYPE_SIGACTION],
[
AC_MSG_CHECKING([for type sigaction_t])
AC_CACHE_VAL(cf_cv_type_sigaction,[
	AC_TRY_COMPILE([
#include <signal.h>],
		[sigaction_t x],
		[cf_cv_type_sigaction=yes],
		[cf_cv_type_sigaction=no])])
AC_MSG_RESULT($cf_cv_type_sigaction)
test "$cf_cv_type_sigaction" = yes && AC_DEFINE(HAVE_TYPE_SIGACTION)
])dnl
dnl ---------------------------------------------------------------------------
dnl Test if the compiler supports 'U' and 'L' suffixes.  Only old compilers
dnl won't, but they're still there.
AC_DEFUN([CF_UNSIGNED_LITERALS],
[
AC_MSG_CHECKING([if unsigned literals are legal])
AC_CACHE_VAL(cf_cv_unsigned_literals,[
	AC_TRY_COMPILE([],[long x = 1L + 1UL + 1U + 1],
		[cf_cv_unsigned_literals=yes],
		[cf_cv_unsigned_literals=no])
	])
AC_MSG_RESULT($cf_cv_unsigned_literals)
])dnl
dnl ---------------------------------------------------------------------------
dnl Make an uppercase version of a variable
dnl $1=uppercase($2)
AC_DEFUN([CF_UPPER],
[
$1=`echo "$2" | sed y%abcdefghijklmnopqrstuvwxyz./-%ABCDEFGHIJKLMNOPQRSTUVWXYZ___%`
])dnl
dnl ---------------------------------------------------------------------------
dnl Compute the shift-mask that we'll use for wide-character indices.  We use
dnl all but the index portion of chtype for storing attributes.
AC_DEFUN([CF_WIDEC_SHIFT],
[
AC_REQUIRE([CF_TYPEOF_CHTYPE])
AC_MSG_CHECKING([for number of bits in chtype])
AC_CACHE_VAL(cf_cv_shift_limit,[
	AC_TRY_RUN([
#include <stdio.h>
int main()
{
	FILE *fp = fopen("cf_test.out", "w");
	if (fp != 0) {
		int n;
		unsigned TYPEOF_CHTYPE x = 1L;
		for (n = 0; ; n++) {
			unsigned long y = (x >> n);
			if (y != 1 || x == 0)
				break;
			x <<= 1;
		}
		fprintf(fp, "%d", n);
		fclose(fp);
	}
	exit(0);
}
		],
		[cf_cv_shift_limit=`cat cf_test.out`],
		[cf_cv_shift_limit=32],
		[cf_cv_shift_limit=32])
		rm -f cf_test.out
	])
AC_MSG_RESULT($cf_cv_shift_limit)
AC_SUBST(cf_cv_shift_limit)

AC_MSG_CHECKING([for width of character-index])
AC_CACHE_VAL(cf_cv_widec_shift,[
if test ".$with_widec" = ".yes" ; then
	cf_attrs_width=39
	if ( expr $cf_cv_shift_limit \> $cf_attrs_width >/dev/null )
	then
		cf_cv_widec_shift=`expr 16 + $cf_cv_shift_limit - $cf_attrs_width`
	else
		cf_cv_widec_shift=16
	fi
else
	cf_cv_widec_shift=8
fi
])
AC_MSG_RESULT($cf_cv_widec_shift)
AC_SUBST(cf_cv_widec_shift)
])dnl
dnl ---------------------------------------------------------------------------
dnl Wrapper for AC_ARG_WITH to ensure that user supplies a pathname, not just
dnl defaulting to yes/no.
dnl
dnl $1 = option name
dnl $2 = help-text
dnl $3 = environment variable to set
dnl $4 = default value, shown in the help-message, must be a constant
dnl $5 = default value, if it's an expression & cannot be in the help-message
dnl
AC_DEFUN([CF_WITH_PATH],
[AC_ARG_WITH($1,[$2 ](default: ifelse($4,,empty,$4)),,
ifelse($4,,[withval="${$3}"],[withval="${$3-ifelse($5,,$4,$5)}"]))dnl
CF_PATH_SYNTAX(withval)
eval $3="$withval"
AC_SUBST($3)dnl
])dnl
dnl ---------------------------------------------------------------------------
dnl Process an option specifying a list of colon-separated paths.
dnl
dnl $1 = option name
dnl $2 = help-text
dnl $3 = environment variable to set
dnl $4 = default value, shown in the help-message, must be a constant
dnl $5 = default value, if it's an expression & cannot be in the help-message
dnl
AC_DEFUN([CF_WITH_PATHLIST],[
AC_REQUIRE([CF_PATHSEP])
AC_ARG_WITH($1,[$2 ](default: ifelse($4,,empty,$4)),,
ifelse($4,,[withval="${$3}"],[withval="${$3-ifelse($5,,$4,$5)}"]))dnl

IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}${PATHSEP}"
cf_dst_path=
for cf_src_path in $withval
do
  CF_PATH_SYNTAX(cf_src_path)
  test -n "$cf_dst_path" && cf_dst_path="${cf_dst_path}:"
  cf_dst_path="${cf_dst_path}${cf_src_path}"
done
IFS="$ac_save_ifs"

eval $3="$cf_dst_path"
AC_SUBST($3)dnl
])dnl
