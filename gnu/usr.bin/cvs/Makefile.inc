.if exists(${.CURDIR}/../lib/obj)
LIBDESTDIR=	${.CURDIR}/../lib/obj
.else
LIBDESTDIR=	${.CURDIR}/../lib
.endif

LDDESTDIR=	-L${LIBDESTDIR}
LIBCVS=		${LIBDESTDIR}/libcvs.a
