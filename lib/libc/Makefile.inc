# $FreeBSD$
#
# This file contains make rules that are shared by libc and libc_r.
#
# Define (empty) variables so that make doesn't give substitution
# errors if the included makefiles don't change these:
MDSRCS=
MISRCS=
MDASM=
MIASM=
NOASM=

#
# If there is a machine dependent makefile, use it:
#
.if exists(${.CURDIR}/../libc/${MACHINE_ARCH}/Makefile.inc)
.include "${.CURDIR}/../libc/${MACHINE_ARCH}/Makefile.inc"
.endif

.include "${.CURDIR}/../libc/db/Makefile.inc"
.include "${.CURDIR}/../libc/compat-43/Makefile.inc"
.include "${.CURDIR}/../libc/gen/Makefile.inc"
.include "${.CURDIR}/../libc/gmon/Makefile.inc"
.include "${.CURDIR}/../libc/locale/Makefile.inc"
.include "${.CURDIR}/../libc/net/Makefile.inc"
.include "${.CURDIR}/../libc/nls/Makefile.inc"
.if !defined(NO_QUAD)
.include "${.CURDIR}/../libc/quad/Makefile.inc"
.endif
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
.if !defined(NO_HESIOD_LIBC)
CFLAGS+= -DHESIOD
.endif

# If there are no machine dependent sources, append all the
# machine-independent sources:
.if empty(MDSRCS)
SRCS+=	${MISRCS}
.else
# Append machine-dependent sources, then append machine-independent sources
# for which there is no machine-dependent variant.
SRCS+=	${MDSRCS}
.for _src in ${MISRCS}
.if ${MDSRCS:R:M${_src:R}} == ""
SRCS+=	${_src}
.endif
.endfor
.endif
