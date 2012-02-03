# $FreeBSD$
#

.PATH:  ${.CURDIR}/../../../i386/bios

KMOD=	smapi
SRCS=	smapi.c smapi_bios.S \
	bus_if.h device_if.h
WERROR=
.if ${CC:T:Mclang} == "clang"
# XXX: clang integrated-as doesn't grok 16-bit assembly yet
CFLAGS+=	${.IMPSRC:T:Msmapi_bios.S:C/^.+$/-no-integrated-as/}
.endif

.include <bsd.kmod.mk>
