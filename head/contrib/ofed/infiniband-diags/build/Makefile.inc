# $FreeBSD$

.PATH: ${.CURDIR}/../../src ${.CURDIR}/../../man

BINDIR?= /usr/bin
SRCS+= ibdiag_common.c ibdiag_sa.c
CFLAGS+= -I${SYSROOT:U${DESTDIR}}/${INCLUDEDIR}/infiniband
CFLAGS+= -DHAVE_CONFIG_H=1
CFLAGS+= -I${.CURDIR}/../../
CFLAGS+= -I${.CURDIR}/../../src
LIBADD+= osmcomp ibmad ibumad

