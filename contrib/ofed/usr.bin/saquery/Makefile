# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   saquery
SRCS=   saquery.c ibdiag_common.c
LDADD=	-libumad -libcommon -libmad -losmcomp -losmvendor -lopensm
CFLAGS+= -I${DIAGPATH}/include -I/usr/include/infiniband
CFLAGS+= -DOSM_VENDOR_INTF_OPENIB -DVENDOR_RMPP_SUPPORT -DDUAL_SIDED_RMPP
CFLAGS+= -pthread
MAN=	saquery.8

WARNS?= 1

.include <bsd.prog.mk>
