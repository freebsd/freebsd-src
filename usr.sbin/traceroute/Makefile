# $FreeBSD$

TRACEROUTE_DISTDIR?= ${.CURDIR}/../../contrib/traceroute
.PATH: ${TRACEROUTE_DISTDIR}

PROG=	traceroute
MAN=	traceroute.8
SRCS=	version.c traceroute.c
BINOWN=	root
BINMODE=4555
CLEANFILES=	version.c

CFLAGS+= -DHAVE_SYS_SELECT_H=1 -DHAVE_SYS_SYSCTL_H=1 \
	 -DHAVE_SETLINEBUF=1 -DHAVE_RAW_OPTIONS=1 \
	 -DSTDC_HEADERS=1
.ifndef (NOIPSEC)
CFLAGS+= -DIPSEC
.endif
# RTT Jitter on the internet these days means printing 3 decimal places on
# > 1000ms times is plain useless.  Uncomment this to enable variable precision
# reporting, ie: print a variable precision from 0.001ms through 1000ms
# CFLAGS+= -DSANE_PRECISION

.ifndef (NOIPSEC)
DPADD=	${LIBIPSEC}
LDADD=	-lipsec
.endif

.if ${MACHINE_ARCH} == "alpha"
# gcc builtin memcpy causes unaligned access
CFLAGS+= -fno-builtin
.endif

CFLAGS+= -I${TRACEROUTE_DISTDIR}/lbl

version.c: ${TRACEROUTE_DISTDIR}/VERSION
	@rm -f ${.TARGET}
	head -1 ${TRACEROUTE_DISTDIR}/VERSION | \
	    sed -e 's/.*/char version[] = "&";/' \
	    > ${.TARGET}

.include <bsd.prog.mk>
