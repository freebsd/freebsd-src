.if exists(${.OBJDIR}/../lib)
LIBDESTDIR=	${.OBJDIR}/../lib
.else
LIBDESTDIR=	${.CURDIR}/../lib
.endif

LDDESTDIR=	-L${LIBDESTDIR}
LIBCVS=		${LIBDESTDIR}/libcvs.a
