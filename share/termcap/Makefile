
# reorder gives an editor command for most common terminals
# (in reverse order from n'th to 1'st most commonly used)
# to move them to the front of termcap
#
MAN=	termcap.5

PACKAGE=	runtime
FILES=		termcap termcap.db
FILESDIR=	${BINDIR}/misc
CLEANFILES+=	termcap.db

.include <bsd.endian.mk>
.include <src.tools.mk>

termcap.db: termcap
	${CAP_MKDB_CMD} ${CAP_MKDB_ENDIAN} -f ${.TARGET:R} ${.ALLSRC}

etc-termcap:
	${INSTALL_SYMLINK} -T "package=runtime" \
	    ${BINDIR}/misc/termcap ${DESTDIR}/etc/termcap

.include <bsd.prog.mk>
