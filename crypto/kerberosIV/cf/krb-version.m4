dnl $Id: krb-version.m4,v 1.1 1997/12/14 15:59:03 joda Exp $
dnl
dnl
dnl output a C header-file with some version strings
dnl
AC_DEFUN(AC_KRB_VERSION,[
dnl AC_OUTPUT_COMMANDS([
cat > include/newversion.h.in <<FOOBAR
char *${PACKAGE}_long_version = "@(#)\$Version: $PACKAGE-$VERSION by @USER@ on @HOST@ ($host) @DATE@ \$";
char *${PACKAGE}_version = "$PACKAGE-$VERSION";
FOOBAR

if test -f include/version.h && cmp -s include/newversion.h.in include/version.h.in; then
	echo "include/version.h is unchanged"
	rm -f include/newversion.h.in
else
 	echo "creating include/version.h"
 	User=${USER-${LOGNAME}}
 	Host=`(hostname || uname -n) 2>/dev/null | sed 1q`
 	Date=`date`
	mv -f include/newversion.h.in include/version.h.in
	sed -e "s/@USER@/$User/" -e "s/@HOST@/$Host/" -e "s/@DATE@/$Date/" include/version.h.in > include/version.h
fi
dnl ],host=$host PACKAGE=$PACKAGE VERSION=$VERSION)
])
