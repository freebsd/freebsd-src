# $FreeBSD$

PROG=		objformat
NOMAN=		not yet
CFLAGS+=	-DMAIN

.if ${BINFORMAT} == elf
CFLAGS+=	-DFREEBSD_ELF
.else
CFLAGS+=	-DFREEBSD_AOUT
.endif

LINKS+=	${BINDIR}/objformat ${BINDIR}/ar
LINKS+=	${BINDIR}/objformat ${BINDIR}/as
LINKS+=	${BINDIR}/objformat ${BINDIR}/ld
LINKS+=	${BINDIR}/objformat ${BINDIR}/nm
LINKS+=	${BINDIR}/objformat ${BINDIR}/ranlib
LINKS+=	${BINDIR}/objformat ${BINDIR}/size
LINKS+=	${BINDIR}/objformat ${BINDIR}/strings
LINKS+=	${BINDIR}/objformat ${BINDIR}/strip

.include <bsd.prog.mk>
