dnl $Id: grok-type.m4,v 1.4 1999/11/29 11:16:48 joda Exp $
dnl
AC_DEFUN(AC_GROK_TYPE, [
AC_CACHE_VAL(ac_cv_type_$1, 
AC_TRY_COMPILE([
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
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
		type=HAVE_[]upcase($i)
		AC_DEFINE_UNQUOTED($type)
	fi
	AC_MSG_RESULT($ac_res)
done
])
