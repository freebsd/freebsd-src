#	$FreeBSD$

.include <src.opts.mk>

PROG=	pkill

LIBADD=	kvm jail

LINKS=	${BINDIR}/pkill ${BINDIR}/pgrep
MLINKS=	pkill.1 pgrep.1

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
