#	$Id$
#
# This file contains make rules that are shared by libc and libc_r.
#
.include "${.CURDIR}/../libc/db/Makefile.inc"
.include "${.CURDIR}/../libc/compat-43/Makefile.inc"
.include "${.CURDIR}/../libc/gen/Makefile.inc"
.include "${.CURDIR}/../libc/gmon/Makefile.inc"
.include "${.CURDIR}/../libc/locale/Makefile.inc"
.include "${.CURDIR}/../libc/net/Makefile.inc"
.include "${.CURDIR}/../libc/nls/Makefile.inc"
.include "${.CURDIR}/../libc/quad/Makefile.inc"
.include "${.CURDIR}/../libc/regex/Makefile.inc"
.include "${.CURDIR}/../libc/stdio/Makefile.inc"
.include "${.CURDIR}/../libc/stdlib/Makefile.inc"
.include "${.CURDIR}/../libc/stdtime/Makefile.inc"
.include "${.CURDIR}/../libc/string/Makefile.inc"
.include "${.CURDIR}/../libc/sys/Makefile.inc"
.include "${.CURDIR}/../libc/rpc/Makefile.inc"
.include "${.CURDIR}/../libc/xdr/Makefile.inc"
.if !defined(NO_YP_LIBC)
CFLAGS+= -DYP
.include "${.CURDIR}/../libc/yp/Makefile.inc"
.endif
.include "${.CURDIR}/../libc/${MACHINE}/sys/Makefile.inc"
