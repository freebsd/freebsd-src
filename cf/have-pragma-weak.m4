dnl $Id$
dnl
AC_DEFUN([AC_HAVE_PRAGMA_WEAK], [
if test "${enable_shared}" = "yes"; then
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

int t(void) {
  return foo;
}

int main(int argc, char **argv) {
  return t();
}
EOF
if AC_TRY_EVAL('CC -o conftest $CFLAGS $CPPFLAGS $LDFLAGS conftest_foo.$ac_ext conftest_bar.$ac_ext 1>&AC_FD_CC'); then
ac_have_pragma_weak=yes
fi
rm -rf conftest*
])
if test "$ac_have_pragma_weak" = "yes"; then
	AC_DEFINE(HAVE_PRAGMA_WEAK, 1, [Define this if your compiler supports \`#pragma weak.'])dnl
fi
AC_MSG_RESULT($ac_have_pragma_weak)
fi
])
