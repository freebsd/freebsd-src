dnl
dnl $Id: check-netinet-ip-and-tcp.m4,v 1.2 1999/05/14 13:15:40 assar Exp $
dnl

dnl extra magic check for netinet/{ip.h,tcp.h} because on irix 6.5.3
dnl you have to include standards.h before including these files

AC_DEFUN(CHECK_NETINET_IP_AND_TCP,
[
AC_CHECK_HEADERS(standards.h)
for i in netinet/ip.h netinet/tcp.h; do

cv=`echo "$i" | sed 'y%./+-%__p_%'`

AC_MSG_CHECKING([for $i])
AC_CACHE_VAL([ac_cv_header_$cv],
[AC_TRY_CPP([\
#ifdef HAVE_STANDARDS_H
#include <standards.h>
#endif
#include <$i>
],
eval "ac_cv_header_$cv=yes",
eval "ac_cv_header_$cv=no")])
AC_MSG_RESULT(`eval echo \\$ac_cv_header_$cv`)
changequote(, )dnl
if test `eval echo \\$ac_cv_header_$cv` = yes; then
  ac_tr_hdr=HAVE_`echo $i | sed 'y%abcdefghijklmnopqrstuvwxyz./-%ABCDEFGHIJKLMNOPQRSTUVWXYZ___%'`
changequote([, ])dnl
  AC_DEFINE_UNQUOTED($ac_tr_hdr, 1)
fi
done
dnl autoheader tricks *sigh*
: << END
@@@headers="$headers netinet/ip.h netinet/tcp.h"@@@
END

])
