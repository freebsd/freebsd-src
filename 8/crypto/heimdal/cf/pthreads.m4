dnl $Id: pthreads.m4 20295 2007-04-11 11:08:08Z lha $

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
		PTHREADS_CFLAGS=-pthreads
		PTHREADS_LIBS=-pthreads
	else
		PTHREADS_CFLAGS=-mt
		PTHREADS_LIBS=-mt
	fi
	;;
*-*-netbsd*)
	native_pthread_support="if running netbsd 1.6T or newer"
	dnl heim_threads.h knows this
	PTHREADS_LIBS=""
	;;
*-*-freebsd5*)
	native_pthread_support=yes
	;;
*-*-linux* | *-*-linux-gnu)
	case `uname -r` in
	2.*)
		native_pthread_support=yes
		PTHREADS_CFLAGS=-pthread
		PTHREADS_LIBS=-pthread
		;;
	esac
	;;
*-*-aix*)
	dnl AIX is disabled since we don't handle the utmp/utmpx
        dnl problems that aix causes when compiling with pthread support
	native_pthread_support=no
	;;
mips-sgi-irix6.[[5-9]])  # maybe works for earlier versions too
	native_pthread_support=yes
	PTHREADS_LIBS="-lpthread"
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
    LIBS="$PTHREADS_LIBS $LIBS"
else
  PTHREADS_CFLAGS=""
  PTHREADS_LIBS=""
fi

AC_SUBST(PTHREADS_CFLAGS)
AC_SUBST(PTHREADS_LIBS)

AC_MSG_RESULT($enable_pthread_support)
])
