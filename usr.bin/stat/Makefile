.include <src.opts.mk>

PROG=	stat

LINKS=	${BINDIR}/stat ${BINDIR}/readlink \
	${BINDIR}/stat ${BINDIR}/mountpoint
MLINKS=	stat.1 readlink.1 stat.1 mountpoint.1

HAS_TESTS=
SUBDIR.${MK_TESTS}+=	tests

.include <bsd.prog.mk>
