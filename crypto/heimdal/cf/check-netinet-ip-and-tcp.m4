dnl
dnl $Id: check-netinet-ip-and-tcp.m4,v 1.3.12.1 2004/04/01 07:27:33 joda Exp $
dnl

dnl extra magic check for netinet/{ip.h,tcp.h} because on irix 6.5.3
dnl you have to include standards.h before including these files

AC_DEFUN([CHECK_NETINET_IP_AND_TCP],
[
AC_CHECK_HEADERS(standards.h)
for i in netinet/ip.h netinet/tcp.h; do

cv=`echo "$i" | sed 'y%./+-%__p_%'`

AC_CACHE_CHECK([for $i],ac_cv_header_$cv,
[AC_TRY_CPP([\
#ifdef HAVE_STANDARDS_H
#include <standards.h>
#endif
#include <$i>
],
eval "ac_cv_header_$cv=yes",
eval "ac_cv_header_$cv=no")])
ac_res=`eval echo \\$ac_cv_header_$cv`
if test "$ac_res" = yes; then
	ac_tr_hdr=HAVE_`echo $i | sed 'y%abcdefghijklmnopqrstuvwxyz./-%ABCDEFGHIJKLMNOPQRSTUVWXYZ___%'`
	AC_DEFINE_UNQUOTED($ac_tr_hdr, 1)
fi
done
if false;then
	AC_CHECK_HEADERS(netinet/ip.h netinet/tcp.h)
fi
])
