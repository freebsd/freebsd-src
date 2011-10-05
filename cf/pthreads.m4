Dnl $Id$

AC_DEFUN([KRB_PTHREADS], [
AC_MSG_CHECKING(if compiling threadsafe libraries)

AC_ARG_ENABLE(pthread-support,
	AS_HELP_STRING([--enable-pthread-support],
			[if you want thread safe libraries]),
	[],[enable_pthread_support=maybe])

case "$host" in 
*-*-solaris2*)
	native_pthread_support=yes
	if test "$GCC" = yes; then
		PTHREAD_CFLAGS=-pthreads
		PTHREAD_LIBADD=-pthreads
	else
		PTHREAD_CFLAGS=-mt
		PTHREAD_LDADD=-mt
		PTHREAD_LIBADD=-mt
	fi
	;;
*-*-netbsd[[12]]*)
	native_pthread_support="if running netbsd 1.6T or newer"
	dnl heim_threads.h knows this
	PTHREAD_LIBADD="-lpthread"
	;;
*-*-netbsd[[3456789]]*)
	native_pthread_support="netbsd 3 uses explict pthread"
	dnl heim_threads.h knows this
	PTHREAD_LIBADD="-lpthread"
	;;
*-*-freebsd[[56789]]*)
	native_pthread_support=yes
	PTHREAD_LIBADD="-pthread"
	;;
*-*-openbsd*)
	native_pthread_support=yes
	PTHREAD_CFLAGS=-pthread
	PTHREAD_LIBADD=-pthread
	;;
*-*-linux* | *-*-linux-gnu)
	case `uname -r` in
	2.*|3.*)
		native_pthread_support=yes
		PTHREAD_CFLAGS=-pthread
		PTHREAD_LIBADD=-pthread
		;;
	esac
	;;
*-*-kfreebsd*-gnu*)
	native_pthread_support=yes
	PTHREAD_CFLAGS=-pthread
	PTHREAD_LIBADD=-pthread
	;;
*-*-aix*)
	dnl AIX is disabled since we don't handle the utmp/utmpx
        dnl problems that aix causes when compiling with pthread support
	native_pthread_support=no
	;;
mips-sgi-irix6.[[5-9]])  # maybe works for earlier versions too
	native_pthread_support=yes
	PTHREAD_LIBADD="-lpthread"
	;;
*-*-darwin*)
	native_pthread_support=yes
	;;
*)
	native_pthread_support=no
	;;
esac

if test "$enable_pthread_support" = maybe ; then
	enable_pthread_support="$native_pthread_support"
fi
	
if test "$enable_pthread_support" != no; then
    AC_DEFINE(ENABLE_PTHREAD_SUPPORT, 1,
	[Define if you want have a thread safe libraries])
    dnl This sucks, but libtool doesn't save the depenecy on -pthread
    dnl for libraries.
    LIBS="$PTHREAD_LIBADD $LIBS"
else
  PTHREAD_CFLAGS=""
  PTHREAD_LIBADD=""
fi

AC_SUBST(PTHREAD_CFLAGS)
AC_SUBST(PTHREAD_LDADD)
AC_SUBST(PTHREAD_LIBADD)

AC_MSG_RESULT($enable_pthread_support)
])
