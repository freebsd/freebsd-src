dnl
dnl $Id: aclocal.m4,v 1.38 1997/05/18 18:47:30 assar Exp $
dnl

dnl
dnl General tests
dnl

dnl
dnl Look for function in any of the specified libraries
dnl

dnl AC_FIND_FUNC_NO_LIBS(func, libraries, includes, arguments)
AC_DEFUN(AC_FIND_FUNC_NO_LIBS, [

AC_MSG_CHECKING([for $1])
AC_CACHE_VAL(ac_cv_funclib_$1,
[
if eval "test \"\$ac_cv_func_$1\" != yes" ; then
	ac_save_LIBS="$LIBS"
	for ac_lib in "" $2; do
		if test -n "$ac_lib"; then 
			ac_lib="-l$ac_lib"
			LIBS="$ac_lib $ac_save_LIBS"
		fi
		AC_TRY_LINK([$3],[$1($4)],eval "if test -n \"$ac_lib\";then ac_cv_funclib_$1=$ac_lib; else ac_cv_funclib_$1=yes; fi";break)
	done
	eval "ac_cv_funclib_$1=\${ac_cv_funclib_$1-no}"
	LIBS="$ac_save_LIBS"
fi
])

eval "ac_res=\$ac_cv_funclib_$1"

# autoheader tricks *sigh*
: << END
@@@funcs="$funcs $1"@@@
@@@libs="$libs $2"@@@
END

changequote(, )dnl
eval "ac_tr_func=HAVE_`echo $1 | tr '[a-z]' '[A-Z]'`"
eval "ac_tr_lib=HAVE_LIB`echo $ac_res | sed -e 's/-l//' | tr '[a-z]' '[A-Z]'`"
eval "LIB_$1=$ac_res"
changequote([, ])dnl

case "$ac_res" in
	yes)
	eval "ac_cv_func_$1=yes"
	eval "LIB_$1="
	AC_DEFINE_UNQUOTED($ac_tr_func)
	AC_MSG_RESULT([yes])
	;;
	no)
	eval "ac_cv_func_$1=no"
	eval "LIB_$1="
	AC_MSG_RESULT([no])
	;;
	*)
	eval "ac_cv_func_$1=yes"
	eval "ac_cv_lib_`echo "$ac_res" | sed 's/-l//'`=yes"
	AC_DEFINE_UNQUOTED($ac_tr_func)
	AC_DEFINE_UNQUOTED($ac_tr_lib)
	AC_MSG_RESULT([yes, in $ac_res])
	;;
esac
AC_SUBST(LIB_$1)
])

dnl AC_FIND_FUNC(func, libraries, includes, arguments)
AC_DEFUN(AC_FIND_FUNC, [
AC_FIND_FUNC_NO_LIBS([$1], [$2], [$3], [$4])
if test -n "$LIB_$1"; then
	LIBS="$LIB_$1 $LIBS"
fi
])

dnl
dnl Same as AC _REPLACE_FUNCS, just define HAVE_func if found in normal
dnl libraries 

AC_DEFUN(AC_BROKEN,
[for ac_func in $1
do
AC_CHECK_FUNC($ac_func, [
changequote(, )dnl
ac_tr_func=HAVE_`echo $ac_func | tr '[a-z]' '[A-Z]'`
changequote([, ])dnl
AC_DEFINE_UNQUOTED($ac_tr_func)],[LIBOBJS="$LIBOBJS ${ac_func}.o"])
# autoheader tricks *sigh*
: << END
@@@funcs="$funcs $1"@@@
END
done
AC_SUBST(LIBOBJS)dnl
])

dnl
dnl Mix between AC_FIND_FUNC and AC_BROKEN
dnl

AC_DEFUN(AC_FIND_IF_NOT_BROKEN,
[AC_FIND_FUNC([$1], [$2], [$3], [$4])
if eval "test \"$ac_cv_func_$1\" != yes"; then
LIBOBJS="$LIBOBJS $1.o"
fi
AC_SUBST(LIBOBJS)dnl
])

dnl
dnl
dnl

dnl AC_TEST_PACKAGE(package,header,lib,linkline)
AC_DEFUN(AC_TEST_PACKAGE,
[
AC_MSG_CHECKING(for $1)
AC_ARG_WITH($1,
[  --with-$1=dir                use $1 in dir],
[if test "$with_$1" = "no"; then
  with_$1=
fi]
)
AC_ARG_WITH($1-lib,
[  --with-$1-lib=dir            use $1-lib in dir],
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-$1-lib])
elif test "X$with_$1" = "X"; then
  with_$1=yes
fi]
)
AC_ARG_WITH($1-include,
[  --with-$1-include=dir        use $1-include in dir],
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-$1-include])
elif test "X$with_$1" = "X"; then
  with_$1=yes
fi]
)

define([foo], translit($1, [a-z], [A-Z]))
: << END
@@@syms="$syms foo"@@@
END

if test -n "$with_$1"; then
  AC_DEFINE([foo])
  if test "$with_$1" != "yes"; then
    $1_dir=$with_$1
  fi
dnl Try to find include
  if test -n "$with_$1_include"; then
    trydir=$with_$1_include
  elif test "$with_$1" != "yes"; then
    trydir="$with_$1 $with_$1/include"
  else
    trydir=
  fi
  found=
  for i in $trydir ""; do
    if test -n "$i"; then
      if test -f $i/$2; then
        found=yes; res=$i; break
      fi
    else
      AC_TRY_CPP([#include <$2>], [found=yes; res=$i; break])
    fi
  done
  if test -n "$found"; then
    $1_include=$res
  else
    AC_MSG_ERROR(Cannot find $2)
  fi
dnl Try to find lib
  if test -n "$with_$1_lib"; then
    trydir=$with_$1_lib
  elif test "$with_$1" != "yes"; then
    trydir="$with_$1 $with_$1/lib"
  else
    trydir=
  fi
  found=
  for i in $trydir ""; do
    if test -n "$i"; then
      if test -f $i/$3; then
        found=yes; res=$i; break
      fi
    else
      old_LIBS=$LIBS
      LIBS="$4 $LIBS"
      AC_TRY_LINK([], [], [found=yes; res=$i; LIBS=$old_LIBS; break])
      LIBS=$old_LIBS
    fi
  done
  if test -n "$found"; then
    $1_lib=$res
  else
    AC_MSG_ERROR(Cannot find $3)
  fi
  AC_MSG_RESULT([headers $$1_include, libraries $$1_lib])
  AC_DEFINE_UNQUOTED(foo)
  if test -n "$$1_include"; then
    foo[INCLUDE]="-I$$1_include"
  fi
  AC_SUBST(foo[INCLUDE])
  if test -n "$$1_lib"; then
    foo[LIB]="-L$$1_lib"
  fi
  foo[LIB]="$foo[LIB] $4"
  AC_SUBST(foo[LIB])
else
  AC_MSG_RESULT(no)
fi
undefine([foo])
])

dnl
dnl Check if we need the declaration of a variable
dnl

dnl AC_HAVE_DECLARATION(includes, variable)
AC_DEFUN(AC_CHECK_DECLARATION, [
AC_MSG_CHECKING([if $2 is properly declared])
AC_CACHE_VAL(ac_cv_var_$2_declaration, [
AC_TRY_COMPILE([$1
extern struct { int foo; } $2;],
[$2.foo = 1;],
eval "ac_cv_var_$2_declaration=no",
eval "ac_cv_var_$2_declaration=yes")
])

ac_tr_var=[HAVE_]translit($2, [a-z], [A-Z])[_DECLARATION]

define([foo], [HAVE_]translit($2, [a-z], [A-Z])[_DECLARATION])
: << END
@@@syms="$syms foo"@@@
END
undefine([foo])

AC_MSG_RESULT($ac_cv_var_$2_declaration)
if eval "test \"\$ac_cv_var_$2_declaration\" = yes"; then
	AC_DEFINE_UNQUOTED($ac_tr_var)
fi
])

dnl
dnl
dnl

dnl AC_CHECK_VAR(includes, variable)
AC_DEFUN(AC_CHECK_VAR, [
AC_MSG_CHECKING(for $2)
AC_CACHE_VAL(ac_cv_var_$2, [
AC_TRY_LINK([extern int $2;
int foo() { return $2; }],
	    [foo()],
	    ac_cv_var_$2=yes, ac_cv_var_$2=no)
])
eval "ac_tr_var=[HAVE_]translit($2,[a-z],[A-Z])"

define([foo], [HAVE_]translit($2, [a-z], [A-Z]))
: << END
@@@syms="$syms foo"@@@
END
undefine([foo])

AC_MSG_RESULT(`eval echo \\$ac_cv_var_$2`)
if test `eval echo \\$ac_cv_var_$2` = yes; then
	AC_DEFINE_UNQUOTED($ac_tr_var)
	AC_CHECK_DECLARATION([$1],[$2])
fi
])

dnl
dnl Check if we need the prototype for a function
dnl

dnl AC_NEED_PROTO(includes, function)

AC_DEFUN(AC_NEED_PROTO, [
AC_CACHE_CHECK([if $2 needs a prototype], ac_cv_func_$2_noproto,
AC_TRY_COMPILE([$1],
[struct foo { int foo; } xx;
extern int $2 (struct foo*);
$2(&xx);
],
eval "ac_cv_func_$2_noproto=yes",
eval "ac_cv_func_$2_noproto=no"))
define([foo], [NEED_]translit($2, [a-z], [A-Z])[_PROTO])
if test "$ac_cv_func_$2_noproto" = yes; then
	AC_DEFINE(foo)
fi
: << END
@@@syms="$syms foo"@@@
END
undefine([foo])
])

dnl AC_MSG_RESULT($ac_cv_func_$3_proto)
dnl if eval "test \"\$ac_cv_func_$3_proto\" = yes"; then
dnl 	AC_DEFINE_UNQUOTED($ac_tr_func)
dnl fi
dnl ])
dnl 
dnl AC_DEFUN(AC_NEED_PROTO, [
dnl AC_MSG_CHECKING([if $3 needs a proto])
dnl AC_CACHE_VAL(ac_cv_func_$3_proto, [
dnl AC_TRY_COMPILE([$1],
dnl [$2],
dnl eval "ac_cv_func_$3_proto=no",
dnl eval "ac_cv_func_$3_proto=yes")
dnl ])
dnl changequote(, )dnl
dnl eval "ac_tr_func=NEED_`echo $3 | tr '[a-z]' '[A-Z]'`_PROTO"
dnl changequote([, ])dnl
dnl 
dnl define([foo], [NEED_]translit($3, [a-z], [A-Z])[_PROTO])
dnl : << END
dnl @@@syms="$syms foo"@@@
dnl END
dnl undefine([foo])
dnl 
dnl AC_MSG_RESULT($ac_cv_func_$3_proto)
dnl if eval "test \"\$ac_cv_func_$3_proto\" = yes"; then
dnl 	AC_DEFINE_UNQUOTED($ac_tr_func)
dnl fi
dnl ])

AC_DEFUN(AC_GROK_TYPE, [
AC_CACHE_VAL(ac_cv_type_$1, 
AC_TRY_COMPILE([
#include "confdefs.h"
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h>
#endif
#ifdef HAVE_BIND_BITYPES_H
#include <bind/bitypes.h>
#endif
#ifdef HAVE_NETINET_IN6_MACHTYPES_H
#include <netinet/in6_machtypes.h>
#endif
],
$i x;
,
eval ac_cv_type_$1=yes,
eval ac_cv_type_$1=no))])


AC_DEFUN(AC_GROK_TYPES, [
for i in $1; do
	AC_MSG_CHECKING(for $i)
	AC_GROK_TYPE($i)
	eval ac_res=\$ac_cv_type_$i
	if test "$ac_res" = yes; then
		type=HAVE_`echo $i | tr '[a-z]' '[A-Z]'`
		AC_DEFINE_UNQUOTED($type)
	fi
	AC_MSG_RESULT($ac_res)
done
])

dnl
dnl Specific tests
dnl

dnl
dnl We prefer byacc or yacc because they do not use `alloca'
dnl

AC_DEFUN(AC_KRB_PROG_YACC,
[AC_CHECK_PROGS(YACC, byacc yacc 'bison -y')])

dnl
dnl Also look for EMXOMF for OS/2
dnl

AC_DEFUN(AC_KRB_PROG_RANLIB,
[AC_CHECK_PROGS(RANLIB, ranlib EMXOMF, :)])

dnl
dnl Better test for ln -s, ln or cp
dnl

AC_DEFUN(AC_KRB_PROG_LN_S,
[AC_MSG_CHECKING(for ln -s or something else)
AC_CACHE_VAL(ac_cv_prog_LN_S,
[rm -f conftestdata
if ln -s X conftestdata 2>/dev/null
then
  rm -f conftestdata
  ac_cv_prog_LN_S="ln -s"
else
  touch conftestdata1
  if ln conftestdata1 conftestdata2; then
    rm -f conftestdata*
    ac_cv_prog_LN_S=ln
  else
    ac_cv_prog_LN_S=cp
  fi
fi])dnl
LN_S="$ac_cv_prog_LN_S"
AC_MSG_RESULT($ac_cv_prog_LN_S)
AC_SUBST(LN_S)dnl
])

dnl
dnl NEXTSTEP is not posix compliant by default,
dnl you need a switch -posix to the compiler
dnl

AC_DEFUN(AC_KRB_SYS_NEXTSTEP, [
AC_MSG_CHECKING(for NEXTSTEP)
AC_CACHE_VAL(krb_cv_sys_nextstep,
AC_EGREP_CPP(yes, 
[#ifdef NeXT
	yes
#endif 
], krb_cv_sys_nextstep=yes, krb_cv_sys_nextstep=no) )
if test "$krb_cv_sys_nextstep" = "yes"; then
  CFLAGS="$CFLAGS -posix"
  LIBS="$LIBS -posix"
fi
AC_MSG_RESULT($krb_cv_sys_nextstep)
])

dnl
dnl AIX have a very different syscall convention
dnl
AC_DEFUN(AC_KRB_SYS_AIX, [
AC_MSG_CHECKING(for AIX)
AC_CACHE_VAL(krb_cv_sys_aix,
AC_EGREP_CPP(yes, 
[#ifdef _AIX
	yes
#endif 
], krb_cv_sys_aix=yes, krb_cv_sys_aix=no) )
AC_MSG_RESULT($krb_cv_sys_aix)
])

dnl
dnl test for broken getcwd in (SunOS braindamage)
dnl

AC_DEFUN(AC_KRB_FUNC_GETCWD_BROKEN, [
if test "$ac_cv_func_getcwd" = yes; then
AC_MSG_CHECKING(if getcwd is broken)
AC_CACHE_VAL(ac_cv_func_getcwd_broken, [
ac_cv_func_getcwd_broken=no

AC_TRY_RUN([
#include <errno.h>
char *getcwd(char*, int);

void *popen(char *cmd, char *mode)
{
	errno = ENOTTY;
	return 0;
}

int main()
{
	char *ret;
	ret = getcwd(0, 1024);
	if(ret == 0 && errno == ENOTTY)
		return 0;
	return 1;
}
], ac_cv_func_getcwd_broken=yes,:,:)
])
if test "$ac_cv_func_getcwd_broken" = yes; then
	AC_DEFINE(BROKEN_GETCWD, 1)dnl
	LIBOBJS="$LIBOBJS getcwd.o"
	AC_SUBST(LIBOBJS)dnl
	AC_MSG_RESULT($ac_cv_func_getcwd_broken)
else
	AC_MSG_RESULT([seems ok])
fi
fi
])


AC_DEFUN(AC_HAVE_PRAGMA_WEAK, [
if test "${with_shared}" = "yes"; then
AC_MSG_CHECKING(for pragma weak)
AC_CACHE_VAL(ac_have_pragma_weak, [
ac_have_pragma_weak=no
cat > conftest_foo.$ac_ext <<'EOF'
[#]line __oline__ "configure"
#include "confdefs.h"
#pragma weak foo = _foo
int _foo = 17;
EOF
cat > conftest_bar.$ac_ext <<'EOF'
[#]line __oline__ "configure"
#include "confdefs.h"
extern int foo;

int t() {
  return foo;
}

int main() {
  return t();
}
EOF
if AC_TRY_EVAL('CC -o conftest $CFLAGS $CPPFLAGS $LDFLAGS conftest_foo.$ac_ext conftest_bar.$ac_ext 1>&AC_FD_CC'); then
ac_have_pragma_weak=yes
fi
rm -rf conftest*
])
if test "$ac_have_pragma_weak" = "yes"; then
	AC_DEFINE(HAVE_PRAGMA_WEAK, 1)dnl
fi
AC_MSG_RESULT($ac_have_pragma_weak)
fi
])

dnl
dnl Search for struct winsize
dnl

AC_DEFUN(AC_KRB_STRUCT_WINSIZE, [
AC_MSG_CHECKING(for struct winsize)
AC_CACHE_VAL(ac_cv_struct_winsize, [
ac_cv_struct_winsize=no
for i in sys/termios.h sys/ioctl.h; do
AC_EGREP_HEADER(
changequote(, )dnl
struct[ 	]*winsize,dnl
changequote([,])dnl
$i, ac_cv_struct_winsize=yes; break)dnl
done
])
if test "$ac_cv_struct_winsize" = "yes"; then
  AC_DEFINE(HAVE_STRUCT_WINSIZE, 1)dnl
fi
AC_MSG_RESULT($ac_cv_struct_winsize)
AC_EGREP_HEADER(ws_xpixel, termios.h, AC_DEFINE(HAVE_WS_XPIXEL))
AC_EGREP_HEADER(ws_ypixel, termios.h, AC_DEFINE(HAVE_WS_YPIXEL))
])
