#	$Id: $

PROG=	traceroute
MAN8=	traceroute.8
BINOWN=	root
BINMODE=4555
CFLAGS+=-DHAVE_SYS_SELECT_H=1 -DHAVE_SETLINEBUF=1 -DHAVE_RAW_OPTIONS=1 \
	-DSTDC_HEADERS=1 
# RTT Jitter on the internet these days means printing 3 decimal places on
# > 1000ms times is plain useless.  Uncomment this to enable variable precision
# reporting, ie: print a variable precision from 0.001ms through 1000ms
# CFLAGS+=-DSANE_PRECISION

SRCS=	version.c traceroute.c
CLEANFILES+=	version.c

TRACEROUTE_DISTDIR?= ${.CURDIR}/../../contrib/traceroute
CFLAGS+=	-I${TRACEROUTE_DISTDIR}/lbl
.PATH:	${TRACEROUTE_DISTDIR}

version.c: ${TRACEROUTE_DISTDIR}/VERSION
	@rm -f $@
	sed -e 's/.*/char version[] = "&";/' ${TRACEROUTE_DISTDIR}/VERSION > $@

.include <bsd.prog.mk>
