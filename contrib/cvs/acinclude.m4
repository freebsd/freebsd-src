/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

AC_DEFUN([ACX_WITH_GSSAPI],[
#
# Use --with-gssapi[=DIR] to enable GSSAPI support.
#
# defaults to enabled with DIR in default list below
#
# Search for /SUNHEA/ and read the comments about this default below.
#
AC_ARG_WITH(
  [gssapi],
  AC_HELP_STRING(
    [--with-gssapi],
    [GSSAPI directory (default autoselects)]), ,
  [with_gssapi=yes])dnl

dnl
dnl FIXME - cache withval and obliterate later cache values when options change
dnl
#
# Try to locate a GSSAPI installation if no location was specified, assuming
# GSSAPI was enabled (the default).
# 
if test -n "$acx_gssapi_cv_gssapi"; then
  # Granted, this is a slightly ugly way to print this info, but the
  # AC_CHECK_HEADER used in the search for a GSSAPI installation makes using
  # AC_CACHE_CHECK worse
  AC_MSG_CHECKING([for GSSAPI])
else :; fi
AC_CACHE_VAL([acx_gssapi_cv_gssapi], [
if test x$with_gssapi = xyes; then
  # --with but no location specified
  # assume a gssapi.h or gssapi/gssapi.h locates our install.
  #
  # This isn't always strictly true.  For instance Solaris 7's SUNHEA (header)
  # package installs gssapi.h whether or not the necessary libraries are
  # installed.  I'm still not sure whether to consider this a bug.  The long
  # way around is to not consider GSSPAI installed unless gss_import_name is
  # found, but that brings up a lot of other hassles, like continuing to let
  # gcc & ld generate the error messages when the user uses --with-gssapi=dir
  # as a debugging aid.  The short way around is to disable GSSAPI by default,
  # but I think Sun users have been faced with this for awhile and I haven't
  # heard many complaints.
  acx_gssapi_save_CPPFLAGS=$CPPFLAGS
  for acx_gssapi_cv_gssapi in yes /usr/kerberos /usr/cygnus/kerbnet no; do
    if test x$acx_gssapi_cv_gssapi = xno; then
      break
    fi
    if test x$acx_gssapi_cv_gssapi = xyes; then
      AC_MSG_CHECKING([for GSSAPI])
      AC_MSG_RESULT([])
    else
      CPPFLAGS="$acx_gssapi_save_CPPFLAGS -I$acx_gssapi_cv_gssapi/include"
      AC_MSG_CHECKING([for GSSAPI in $acx_gssapi_cv_gssapi])
      AC_MSG_RESULT([])
    fi
    unset ac_cv_header_gssapi_h
    unset ac_cv_header_gssapi_gssapi_h
    unset ac_cv_header_krb5_h
    AC_CHECK_HEADERS([gssapi.h gssapi/gssapi.h krb5.h])
    if (test "$ac_cv_header_gssapi_h" = yes ||
	  test "$ac_cv_header_gssapi_gssapi_h" = yes) &&
	test "$ac_cv_header_krb5_h" = yes; then
      break
    fi
  done
  CPPFLAGS=$acx_gssapi_save_CPPFLAGS
else
  acx_gssapi_cv_gssapi=$with_gssapi
fi
AC_MSG_CHECKING([for GSSAPI])
])dnl
AC_MSG_RESULT([$acx_gssapi_cv_gssapi])

#
# Set up GSSAPI includes for later use.  We don't bother to check for
# $acx_gssapi_cv_gssapi=no here since that will be caught later.
#
if test x$acx_gssapi_cv_gssapi = yes; then
  # no special includes necessary
  GSSAPI_INCLUDES=""
else
  # GSSAPI at $acx_gssapi_cv_gssapi (could be 'no')
  GSSAPI_INCLUDES=" -I$acx_gssapi_cv_gssapi/include"
fi

#
# Get the rest of the information CVS needs to compile with GSSAPI support
#
if test x$acx_gssapi_cv_gssapi != xno; then
  # define HAVE_GSSAPI and set up the includes
  AC_DEFINE([HAVE_GSSAPI], ,
[Define if you have GSSAPI with Kerberos version 5 available.])
  includeopt=$includeopt$GSSAPI_INCLUDES

  # locate any other headers
  acx_gssapi_save_CPPFLAGS=$CPPFLAGS
  CPPFLAGS=$CPPFLAGS$GSSAPI_INCLUDES
  dnl We don't use HAVE_KRB5_H anywhere, but including it here might make it
  dnl easier to spot errors by reading configure output
  AC_CHECK_HEADERS([gssapi.h gssapi/gssapi.h gssapi/gssapi_generic.h krb5.h])
  # And look through them for GSS_C_NT_HOSTBASED_SERVICE or its alternatives
  AC_CACHE_CHECK(
    [for GSS_C_NT_HOSTBASED_SERVICE],
    [acx_gssapi_cv_gss_c_nt_hostbased_service],
  [
    acx_gssapi_cv_gss_c_nt_hostbased_service=no
    if test "$ac_cv_header_gssapi_h" = "yes"; then
      AC_EGREP_HEADER(
	[GSS_C_NT_HOSTBASED_SERVICE], [gssapi.h],
	[acx_gssapi_cv_gss_c_nt_hostbased_service=yes],
      [
	AC_EGREP_HEADER(
	  [gss_nt_service_name], [gssapi.h],
	  [acx_gssapi_cv_gss_c_nt_hostbased_service=gss_nt_service_name])
      ])
    fi
    if test $acx_gssapi_cv_gss_c_nt_hostbased_service = no &&
       test "$ac_cv_header_gssapi_gssapi_h" = "yes"; then
      AC_EGREP_HEADER(
	[GSS_C_NT_HOSTBASED_SERVICE], [gssapi/gssapi.h],
	[acx_gssapi_cv_gss_c_nt_hostbased_service=yes],
      [
	AC_EGREP_HEADER([gss_nt_service_name], [gssapi/gssapi.h],
	  [acx_gssapi_cv_gss_c_nt_hostbased_service=gss_nt_service_name])
      ])
    else :; fi
    if test $acx_gssapi_cv_gss_c_nt_hostbased_service = no &&
       test "$ac_cv_header_gssapi_gssapi_generic_h" = "yes"; then
      AC_EGREP_HEADER(
	[GSS_C_NT_HOSTBASED_SERVICE], [gssapi/gssapi_generic.h],
	[acx_gssapi_cv_gss_c_nt_hostbased_service=yes],
      [
	AC_EGREP_HEADER(
	  [gss_nt_service_name], [gssapi/gssapi_generic.h],
	  [acx_gssapi_cv_gss_c_nt_hostbased_service=gss_nt_service_name])
      ])
    else :; fi
  ])
  if test $acx_gssapi_cv_gss_c_nt_hostbased_service != yes &&
     test $acx_gssapi_cv_gss_c_nt_hostbased_service != no; then
    # don't define for yes since that means it already means something and
    # don't define for no since we'd rather the compiler catch the error
    # It's debatable whether we'd prefer that the compiler catch the error
    #  - it seems our estranged developer is more likely to be familiar with
    #	 the intricacies of the compiler than with those of autoconf, but by
    #	 the same token, maybe we'd rather alert them to the fact that most
    #	 of the support they need to fix the problem is installed if they can
    #	 simply locate the appropriate symbol.
    AC_DEFINE_UNQUOTED(
      [GSS_C_NT_HOSTBASED_SERVICE],
      [$acx_gssapi_cv_gss_c_nt_hostbased_service],
[Define to an alternative value if GSS_C_NT_HOSTBASED_SERVICE isn't defined
in the gssapi.h header file.  MIT Kerberos 1.2.1 requires this.  Only relevant
when using GSSAPI.])
  else :; fi

  CPPFLAGS=$acx_gssapi_save_CPPFLAGS

  # Expect the libs to be installed parallel to the headers
  #
  # We could try once with and once without, but I'm not sure it's worth the
  # trouble.
  if test x$acx_gssapi_cv_gssapi != xyes; then
    if test -z "$LIBS"; then
      LIBS="-L$acx_gssapi_cv_gssapi/lib"
    else
      LIBS="-L$acx_gssapi_cv_gssapi/lib $LIBS"
    fi
  else :; fi

  dnl What happens if we want to enable, say, krb5 and some other GSSAPI
  dnl authentication method at the same time?
  #
  # Some of the order below is particular due to library dependencies
  #

  #
  # des			Heimdal K 0.3d, but Heimdal seems to be set up such
  #			that it could have been installed from elsewhere.
  #
  AC_SEARCH_LIBS([des_set_odd_parity], [des])

  #
  # com_err		Heimdal K 0.3d
  #
  # com_err		MIT K5 v1.2.2-beta1
  #
  AC_SEARCH_LIBS([com_err], [com_err])

  #
  # asn1		Heimdal K 0.3d		-lcom_err
  #
  AC_SEARCH_LIBS([initialize_asn1_error_table_r], [asn1])

  #
  # resolv		required, but not installed by Heimdal K 0.3d
  #
  # resolv		MIT K5 1.2.2-beta1
  # 			Linux 2.2.17
  #
  AC_SEARCH_LIBS([__dn_expand], [resolv])

  #
  # roken		Heimdal K 0.3d		-lresolv
  #
  AC_SEARCH_LIBS([roken_gethostbyaddr], [roken])

  #
  # k5crypto		MIT K5 v1.2.2-beta1
  #
  AC_SEARCH_LIBS([valid_enctype], [k5crypto])

  #
  # gen			? ? ?			Needed on Irix 5.3 with some
  #			Irix 5.3		version of Kerberos.  I'm not
  #						sure which since Irix didn't
  #						get any testing this time
  #						around.  Original comment:
  #
  # This is necessary on Irix 5.3, in order to link against libkrb5 --
  # there, an_to_ln.o refers to things defined only in -lgen.
  #
  AC_SEARCH_LIBS([compile], [gen])

  #
  # krb5		? ? ?			-lgen -l???
  #			Irix 5.3
  #
  # krb5		MIT K5 v1.1.1
  #
  # krb5		MIT K5 v1.2.2-beta1	-lcrypto -lcom_err
  # 			Linux 2.2.17
  #
  # krb5		MIT K5 v1.2.2-beta1	-lcrypto -lcom_err -lresolv
  #
  # krb5		Heimdal K 0.3d		-lasn1 -lroken -ldes
  #
  AC_SEARCH_LIBS([krb5_free_context], [krb5])

  #
  # gssapi_krb5		Only lib needed with MIT K5 v1.2.1, so find it first in
  #			order to prefer MIT Kerberos.  If both MIT & Heimdal
  #			Kerberos are installed and in the path, this will leave
  #			some of the libraries above in LIBS unnecessarily, but
  #			noone would ever do that, right?
  #
  # gssapi_krb5		MIT K5 v1.2.2-beta1	-lkrb5
  #
  # gssapi		Heimdal K 0.3d		-lkrb5
  #
  AC_SEARCH_LIBS([gss_import_name], [gssapi_krb5 gssapi])
fi
])dnl
