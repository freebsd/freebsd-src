#	@(#)Makefile	8.2 (Berkeley) 3/17/94
# $FreeBSD$

.PATH: ${.CURDIR}/../../sys/geom

PROG=	bsdlabel
SRCS=	bsdlabel.c geom_bsd_enc.c
MAN+=	bsdlabel.8

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
LINKS=	${BINDIR}/bsdlabel ${BINDIR}/disklabel
MLINKS=	bsdlabel.8 disklabel.8
.endif

DPADD=	${LIBGEOM} ${LIBBSDXML} ${LIBSBUF}
LDADD=	-lgeom -lbsdxml -lsbuf

.include <bsd.prog.mk>
