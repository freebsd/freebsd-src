DIAGPATH=	${.CURDIR}/../../management/infiniband-diags
BINDIR?=	/usr/bin
CFLAGS+=	-I${.CURDIR}/../../include/infiniband
CFLAGS+=	-I${.CURDIR}/../../management/opensm/include/
