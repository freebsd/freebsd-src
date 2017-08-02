# $FreeBSD$

.PATH: ${.CURDIR}/../../src ${.CURDIR}/../../man

BINDIR?= /usr/bin
SRCS+= ibdiag_common.c ibdiag_sa.c
CFLAGS+= -I${INCLUDEDIR}/infiniband
CFLAGS+= -DHAVE_CONFIG_H=1
CFLAGS+= -I${.CURDIR}/../../
CFLAGS+= -I${.CURDIR}/../../src
.if defined(TESTBUILD)
LDFLAGS+= -losmcomp -libmad -libumad
.else
LIBADD+= osmcomp ibmad ibumad
.endif

