# $FreeBSD: src/usr.sbin/traceroute/Makefile,v 1.11.2.1 2000/07/03 06:41:38 ps Exp $

PROG=	traceroute
MAN8=	traceroute.8
BINMODE=4555
CFLAGS+=-DHAVE_SYS_SELECT_H=1 -DHAVE_SETLINEBUF=1 -DHAVE_RAW_OPTIONS=1 \
	-DSTDC_HEADERS=1
.ifndef (NOIPSEC)
CFLAGS+=-DIPSEC
.endif
# RTT Jitter on the internet these days means printing 3 decimal places on
# > 1000ms times is plain useless.  Uncomment this to enable variable precision
# reporting, ie: print a variable precision from 0.001ms through 1000ms
# CFLAGS+=-DSANE_PRECISION

SRCS=	version.c traceroute.c
CLEANFILES+=	version.c
.ifndef (NOIPSEC)
DPADD=	${LIBIPSEC}
LDADD=	-lipsec
.endif

TRACEROUTE_DISTDIR?= ${.CURDIR}/../../contrib/traceroute

.if ${MACHINE_ARCH} == "alpha"
# gcc builtin memcpy causes unaligned access
CFLAGS+=	-fno-builtin
.endif

CFLAGS+=	-I${TRACEROUTE_DISTDIR}/lbl
.PATH:	${TRACEROUTE_DISTDIR}

version.c: ${TRACEROUTE_DISTDIR}/VERSION
	@rm -f ${.TARGET}
	sed -e 's/.*/char version[] = "&";/' ${TRACEROUTE_DISTDIR}/VERSION > ${.TARGET}

.include <bsd.prog.mk>
