.if !defined(CVSDIR)

CVSDIR=		$(.CURDIR)/../../../../contrib/cvs

.if exists(${.OBJDIR}/../lib)
LIBDESTDIR=	${.OBJDIR}/../lib
.else
LIBDESTDIR=	${.CURDIR}/../lib
.endif

LIBCVS=		${LIBDESTDIR}/libcvs.a

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.endif
