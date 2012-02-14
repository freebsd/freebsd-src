dnl $Id: aclocal.m4,v 1.1 2000/07/29 13:34:15 lukem Exp $
dnl

dnl
dnl AC_MSG_TRY_COMPILE
dnl
dnl Written by Luke Mewburn <lukem@netbsd.org>
dnl
dnl Usage:
dnl	AC_MSG_TRY_COMPILE(Message, CacheVar, Includes, Code,
dnl			    ActionPass [,ActionFail] )
dnl
dnl effectively does:
dnl	AC_CACHE_CHECK(Message, CacheVar,
dnl		AC_TRY_COMPILE(Includes, Code, CacheVar = yes, CacheVar = no)
dnl		if CacheVar == yes
dnl			AC_MESSAGE_RESULT(yes)
dnl			ActionPass
dnl		else
dnl			AC_MESSAGE_RESULT(no)
dnl			ActionFail
dnl	)
dnl
AC_DEFUN(AC_MSG_TRY_COMPILE, [
	AC_CACHE_CHECK($1, $2, [
		AC_TRY_COMPILE([ $3 ], [ $4; ], [ $2=yes ], [ $2=no ])
	])
	if test "x[$]$2" = "xyes"; then
		$5
	else
		$6
		:
	fi
])

dnl
dnl AC_MSG_TRY_LINK
dnl
dnl Usage:
dnl	AC_MSG_TRY_LINK(Message, CacheVar, Includes, Code,
dnl			    ActionPass [,ActionFail] )
dnl
dnl as AC_MSG_TRY_COMPILE, but uses AC_TRY_LINK instead of AC_TRY_COMPILE
dnl
AC_DEFUN(AC_MSG_TRY_LINK, [
	AC_CACHE_CHECK($1, $2, [
		AC_TRY_LINK([ $3 ], [ $4; ], [ $2=yes ], [ $2=no ])
	])
	if test "x[$]$2" = "xyes"; then
		$5
	else
		$6
		:
	fi
])


dnl
dnl AC_LIBRARY_NET: #Id: net.m4,v 1.5 1997/11/09 21:36:54 jhawk Exp #
dnl
dnl Written by John Hawkinson <jhawk@mit.edu>. This code is in the Public
dnl Domain.
dnl
dnl This test is for network applications that need socket() and
dnl gethostbyname() -ish functions.  Under Solaris, those applications need to
dnl link with "-lsocket -lnsl".  Under IRIX, they should *not* link with
dnl "-lsocket" because libsocket.a breaks a number of things (for instance:
dnl gethostbyname() under IRIX 5.2, and snoop sockets under most versions of
dnl IRIX).
dnl 
dnl Unfortunately, many application developers are not aware of this, and
dnl mistakenly write tests that cause -lsocket to be used under IRIX.  It is
dnl also easy to write tests that cause -lnsl to be used under operating
dnl systems where neither are necessary (or useful), such as SunOS 4.1.4, which
dnl uses -lnsl for TLI.
dnl 
dnl This test exists so that every application developer does not test this in
dnl a different, and subtly broken fashion.
dnl 
dnl It has been argued that this test should be broken up into two seperate
dnl tests, one for the resolver libraries, and one for the libraries necessary
dnl for using Sockets API. Unfortunately, the two are carefully intertwined and
dnl allowing the autoconf user to use them independantly potentially results in
dnl unfortunate ordering dependancies -- as such, such component macros would
dnl have to carefully use indirection and be aware if the other components were
dnl executed. Since other autoconf macros do not go to this trouble, and almost
dnl no applications use sockets without the resolver, this complexity has not
dnl been implemented.
dnl
dnl The check for libresolv is in case you are attempting to link statically
dnl and happen to have a libresolv.a lying around (and no libnsl.a).
dnl
AC_DEFUN(AC_LIBRARY_NET, [
   # Most operating systems have gethostbyname() in the default searched
   # libraries (i.e. libc):
   AC_CHECK_FUNC(gethostbyname, ,
     # Some OSes (eg. Solaris) place it in libnsl:
     AC_CHECK_LIB(nsl, gethostbyname, , 
       # Some strange OSes (SINIX) have it in libsocket:
       AC_CHECK_LIB(socket, gethostbyname, ,
          # Unfortunately libsocket sometimes depends on libnsl.
          # AC_CHECK_LIB's API is essentially broken so the following
          # ugliness is necessary:
          AC_CHECK_LIB(socket, gethostbyname,
             LIBS="-lsocket -lnsl $LIBS",
               AC_CHECK_LIB(resolv, gethostbyname),
             -lnsl)
       )
     )
   )
  AC_CHECK_FUNC(socket, , AC_CHECK_LIB(socket, socket, ,
    AC_CHECK_LIB(socket, socket, LIBS="-lsocket -lnsl $LIBS", , -lnsl)))
  ])


dnl Checks for SOCKS firewall support.
dnl
dnl Written by Matthew R. Green <mrg@eterna.com.au>
dnl
AC_DEFUN(AC_LIBRARY_SOCKS, [
    AC_MSG_CHECKING(whether to support SOCKS)
    AC_ARG_WITH(socks,
    [  --with-socks            Compile with SOCKS firewall traversal support.],
    [
	case "$withval" in
	no)
	    AC_MSG_RESULT(no)
	    ;;
	yes)
	    AC_MSG_RESULT(yes)
	    AC_CHECK_LIB(socks5, SOCKSconnect, [
		socks=5
		LIBS="-lsocks5 $LIBS"], [
	      AC_CHECK_LIB(socks, Rconnect, [
		socks=4
		LIBS="-lsocks $LIBS"], [
		    AC_MSG_ERROR(Could not find socks library.  You must first install socks.) ] ) ] )
	    ;;
	esac
    ],
	AC_MSG_RESULT(no)
    )

    if test "x$socks" = "x"; then
	AC_MSG_CHECKING(whether to support SOCKS5)
	AC_ARG_WITH(socks5,
	[  --with-socks5[=PATH]    Compile with SOCKS5 firewall traversal support.],
	[
	    case "$withval" in
	    no)
		AC_MSG_RESULT(no)
		;;
	    *)
		AC_MSG_RESULT(yes)
		socks=5
		if test "x$withval" = "xyes"; then
		    withval="-lsocks5"
		else
		    if test -d "$withval"; then
			if test -d "$withval/include"; then
			    CFLAGS="$CFLAGS -I$withval/include"
			else
			    CFLAGS="$CFLAGS -I$withval"
			fi
			if test -d "$withval/lib"; then
			    withval="-L$withval/lib -lsocks5"
			else
			    withval="-L$withval -lsocks5"
			fi
		    fi
		fi
		LIBS="$withval $LIBS"
		# If Socks was compiled with Kerberos support, we will need
		# to link against kerberos libraries.  Temporarily append
		# to LIBS.  This is harmless if there is no kerberos support.
		TMPLIBS="$LIBS"
		LIBS="$LIBS $KERBEROS_LIBS"
		AC_TRY_LINK([],
		    [ SOCKSconnect(); ],
		    [],
		    [ AC_MSG_ERROR(Could not find the $withval library.  You must first install socks5.) ])
		LIBS="$TMPLIBS"
		;;
	    esac
	],
	    AC_MSG_RESULT(no)
	)
    fi

    if test "x$socks" = "x"; then
	AC_MSG_CHECKING(whether to support SOCKS4)
	AC_ARG_WITH(socks4,
	[  --with-socks4[=PATH]    Compile with SOCKS4 firewall traversal support.],
	[
	    case "$withval" in
	    no)
		AC_MSG_RESULT(no)
		;;
	    *)
		AC_MSG_RESULT(yes)
		socks=4
		if test "x$withval" = "xyes"; then
		    withval="-lsocks"
		else
		    if test -d "$withval"; then
			withval="-L$withval -lsocks"
		    fi
		fi
		LIBS="$withval $LIBS"
		AC_TRY_LINK([],
		    [ Rconnect(); ],
		    [],
		    [ AC_MSG_ERROR(Could not find the $withval library.  You must first install socks.) ])
		;;
	    esac
	],
	    AC_MSG_RESULT(no)
	)
    fi

    if test "x$socks" = "x4"; then
	AC_DEFINE(SOCKS)
	AC_DEFINE(SOCKS4)
	AC_DEFINE(connect, Rconnect)
	AC_DEFINE(getsockname, Rgetsockname)
	AC_DEFINE(bind, Rbind)
	AC_DEFINE(accept, Raccept)
	AC_DEFINE(listen, Rlisten)
	AC_DEFINE(select, Rselect)
    fi

    if test "x$socks" = "x5"; then
	AC_DEFINE(SOCKS)
	AC_DEFINE(SOCKS5)
	AC_DEFINE(connect,SOCKSconnect)
	AC_DEFINE(getsockname,SOCKSgetsockname)
	AC_DEFINE(getpeername,SOCKSgetpeername)
	AC_DEFINE(bind,SOCKSbind)
	AC_DEFINE(accept,SOCKSaccept)
	AC_DEFINE(listen,SOCKSlisten)
	AC_DEFINE(select,SOCKSselect)
	AC_DEFINE(recvfrom,SOCKSrecvfrom)
	AC_DEFINE(sendto,SOCKSsendto)
	AC_DEFINE(recv,SOCKSrecv)
	AC_DEFINE(send,SOCKSsend)
	AC_DEFINE(read,SOCKSread)
	AC_DEFINE(write,SOCKSwrite)
	AC_DEFINE(rresvport,SOCKSrresvport)
	AC_DEFINE(shutdown,SOCKSshutdown)
	AC_DEFINE(listen,SOCKSlisten)
	AC_DEFINE(close,SOCKSclose)
	AC_DEFINE(dup,SOCKSdup)
	AC_DEFINE(dup2,SOCKSdup2)
	AC_DEFINE(fclose,SOCKSfclose)
	AC_DEFINE(gethostbyname,SOCKSgethostbyname)
    fi
])
