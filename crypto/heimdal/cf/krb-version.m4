dnl $Id: krb-version.m4,v 1.3 2002/08/20 15:49:58 joda Exp $
dnl
dnl
dnl output a C header-file with some version strings
dnl

AC_DEFUN(AC_KRB_VERSION,[
cat > include/newversion.h.in <<FOOBAR
const char *${PACKAGE_TARNAME}_long_version = "@(#)\$Version: $PACKAGE_STRING by @USER@ on @HOST@ ($host) @DATE@ \$";
const char *${PACKAGE_TARNAME}_version = "$PACKAGE_STRING";
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
])
