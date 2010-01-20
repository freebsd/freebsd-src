#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD$

# reorder gives an editor command for most common terminals
# (in reverse order from n'th to 1'st most commonly used)
# to move them to the front of termcap
#
MAN=	termcap.5

FILES=		termcap termcap.db
FILESDIR=	${BINDIR}/misc
CLEANFILES+=	termcap termcap.db

termcap: reorder termcap.src
	TERM=dumb TERMCAP=dumb: ex - ${.CURDIR}/termcap.src < ${.CURDIR}/reorder

.include <bsd.endian.mk>
.if ${TARGET_ENDIANNESS} == "1234"
CAP_MKDB_ENDIAN= -l
.elif ${TARGET_ENDIANNESS} == "4321"
CAP_MKDB_ENDIAN= -b
.else
CAP_MKDB_ENDIAN=
.endif

termcap.db: termcap
	cap_mkdb ${CAP_MKDB_ENDIAN} termcap

etc-termcap:
	ln -fs ${BINDIR}/misc/termcap ${DESTDIR}/etc/termcap

.include <bsd.prog.mk>
