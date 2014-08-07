# $FreeBSD$

.include <src.opts.mk>

PROG=		mdconfig
MAN=		mdconfig.8

DPADD=	${LIBUTIL} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} 
LDADD=	-lutil -lgeom -lbsdxml -lsbuf

.if ${MK_TESTS} != "no"
SUBDIR+=    tests
.endif

.include <bsd.prog.mk>
