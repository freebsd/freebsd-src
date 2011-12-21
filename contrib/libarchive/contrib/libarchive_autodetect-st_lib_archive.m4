dnl
dnl @synopsis ST_LIB_ARCHIVE([ENABLED-DEFAULT])
dnl
dnl This macro figures out what's necessary to link a program against an
dnl instance of the BSD libarchive package by Tim Kientzle.
dnl 
dnl See http://people.freebsd.org/~kientzle/libarchive/ for more info.
dnl
dnl It exports and substitutes the variables LIBARCHIVE_LIBS, LIBARCHIVE_LDFLAGS,
dnl and LIBARCHIVE_CPPFLAGS to appropriate values for the identified instance of
dnl libarchive.  The values are AC_SUBST'd, so a user could, for example, simply
dnl include @LIBARCHIVE_CPPFLAGS@ in the definition of AM_CPPFLAGS in a Makefile.am.
dnl
dnl ENABLED-DEFAULT is either "yes" or "no" and determines whether the default value
dnl is --with-libarchive or --without-libarchive.  It is not possible to specify a
dnl default directory.  More simply, any reasonable choice for a default should just
dnl go into the auto-detect list.
dnl
dnl The macro defines the symbol HAVE_LIBARCHIVE if the library is found. You
dnl should use autoheader to include a definition for this symbol in a config.h
dnl file. Sample usage in a C/C++ source is as follows:
dnl
dnl   #ifdef HAVE_LIBARCHIVE
dnl   #include <archive.h>
dnl   #endif /* HAVE_LIBARCHIVE */
dnl
dnl @category InstalledPackages
dnl @author Andre Stechert <andre@splunk.com>
dnl @version 2006-04-20
dnl @license GPLWithACException

AC_DEFUN([ST_LIB_ARCHIVE],
[
#
# Handle input from the configurer and blend with the requirements from the maintainer.
# We go through the trouble of creating a second set of variables other than the with_foo
# variables in order to be sure that error/corner cases have been cleaned up.
#
# After this statement, three trusted variable are defined.
#
# st_lib_archive_ENABLED will be either "yes" or "no".  its value determines whether
# or not we bother with the rest of the checks and whether or not we export a
# bunch of variables.
#
# st_lib_archive_LOCATION will be either "auto" or "defined".  if it is "auto", then
# we try a bunch of standard locations.  if it is "defined", then we just try the value
# provided in st_lib_archive_DIR.
#
# st_lib_archive_DIR will contain the string provided by the user, provided that it's
# actually a directory.
#
AC_MSG_CHECKING([if libarchive is wanted])
AC_ARG_WITH([libarchive],
	AS_HELP_STRING([--with-libarchive=DIR], [libarchive installation directory]),
	[if test "x$with_libarchive" = "xno" ; then
		st_lib_archive_ENABLED=no
	elif test "x$with_libarchive" = "xyes" ; then
		st_lib_archive_ENABLED=yes
		st_lib_archive_LOCATION=auto
	else
		st_lib_archive_ENABLED=yes
		st_lib_archive_LOCATION=defined
		if test -d "$with_libarchive" ; then
			st_lib_archive_DIR="$with_libarchive"
		else
			AC_MSG_ERROR([$with_libarchive is not a directory])
		fi
	fi],
	[if test "x$1" = "xno" ; then
		st_lib_archive_ENABLED=no
	elif test "x$1" = "xyes" ; then
		st_lib_archive_ENABLED=yes
	else
		st_lib_archive_ENABLED=yes
	fi])

if test "$st_lib_archive_ENABLED" = "yes" ; then
	AC_MSG_RESULT([yes])
#
# After this statement, one trusted variable is defined.
#
# st_lib_archive_LIB will be either "lib" or "lib64", depending on whether the configurer
# specified 32, 64.  The default is "lib".
#
	AC_MSG_CHECKING([whether to use lib or lib64])
	AC_ARG_WITH([libarchive-bits],
		AS_HELP_STRING([--with-libarchive-bits=32/64], [if 64, look in /lib64 on hybrid systems]),
		[if test "x$with_libarchive_bits" = "x32" ; then
			st_lib_archive_LIB=lib
		elif test "x$with_libarchive_bits" = "x64" ; then
			st_lib_archive_LIB=lib64
		else
			AC_MSG_ERROR([the argument must be either 32 or 64])
		fi],
		[st_lib_archive_LIB=lib])
	AC_MSG_RESULT($st_lib_archive_LIB)
#
# Save the environment before verifying libarchive availability
#
	st_lib_archive_SAVECPPFLAGS="$CPPFLAGS"
	st_lib_archive_SAVELDFLAGS="$LDFLAGS"
	AC_LANG_SAVE
	AC_LANG_C

	if test "x$st_lib_archive_LOCATION" = "xdefined" ; then
		CPPFLAGS="-I$st_lib_archive_DIR/include $st_lib_archive_SAVECPPFLAGS"
		LDFLAGS="-L$st_lib_archive_DIR/$st_lib_archive_LIB $st_lib_archive_SAVELDFLAGS"
		AC_CHECK_LIB(archive, archive_read_new, [st_lib_archive_found_lib=yes], [st_lib_archive_found_lib=no])
		AC_CHECK_HEADER(archive.h, [st_lib_archive_found_hdr=yes], [st_lib_archive_found_hdr=no])
		if test "x$st_lib_archive_found_lib" = "xyes" && test "x$st_lib_archive_found_hdr" = "xyes"; then
			LIBARCHIVE_CPPFLAGS="-I$dir/include"
			LIBARCHIVE_LDFLAGS="-L$dir/$st_lib_archive_LIB"
		else
			AC_MSG_ERROR([could not find libarchive in the requested location])
		fi
	else
		#
		# These are the common install directories for Linux, FreeBSD, Solaris, and Mac.
		#
		for dir in /usr /usr/local /usr/sfw /opt/csw /opt/local /sw
		do
			if test -d "$dir" ; then
				CPPFLAGS="-I$dir/include $st_lib_archive_SAVECPPFLAGS"
				LDFLAGS="-L$dir/$st_lib_archive_LIB $st_lib_archive_SAVELDFLAGS"
				AC_CHECK_LIB(archive, archive_read_new, [st_lib_archive_found_lib=yes], [st_lib_archive_found_lib=no])
				AC_CHECK_HEADER(archive.h, [st_lib_archive_found_hdr=yes], [st_lib_archive_found_hdr=no])
				if test "x$st_lib_archive_found_lib" = "xyes" && test "x$st_lib_archive_found_hdr" = "xyes"; then
					LIBARCHIVE_CPPFLAGS="-I$dir/include"
					LIBARCHIVE_LDFLAGS="-L$dir/$st_lib_archive_LIB"
					break
				fi
			fi
		done
	fi

	if test "x$st_lib_archive_found_hdr" = "xyes" && test "x$st_lib_archive_found_lib" = "xyes" ; then
		LIBARCHIVE_LIBS="-larchive"
		AC_DEFINE([HAVE_LIBARCHIVE], [1], [Defined to 1 if libarchive is available for use.])
		AC_SUBST(LIBARCHIVE_LIBS)
		AC_SUBST(LIBARCHIVE_CPPFLAGS)
		AC_SUBST(LIBARCHIVE_LDFLAGS)
	fi

#
# Restore the environment now that we're done.
#
	AC_LANG_RESTORE
	CPPFLAGS="$st_lib_archive_SAVECPPFLAGS"
	LDFLAGS="$st_lib_archive_SAVELDFLAGS"
else
	AC_MSG_RESULT([no])
fi
AM_CONDITIONAL(LIBARCHIVE, test "x$st_lib_archive_found_lib" = "xyes" && test "x$st_lib_archive_found_hdr" = "xyes")
])
