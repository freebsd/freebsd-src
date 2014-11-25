# $FreeBSD$

MAN=

.include <src.opts.mk>

.PATH: ${.CURDIR}/../../../contrib/ntp/util \
       ${.CURDIR}/../../../contrib/ntp/ntpd

PROG=	ntp-keygen
SRCS=	ntp-keygen.c ntp-keygen-opts.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../ \
	-I${.CURDIR}/../../../contrib/ntp/libopts

LIBADD+=	ntp opts

.if ${MK_OPENSSL} != "no"
LIBADD+=	md crypto
.endif

.include <bsd.prog.mk>
