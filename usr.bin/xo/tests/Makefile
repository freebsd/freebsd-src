# $FreeBSD$

.include <bsd.own.mk>

LIBXOSRC=	${SRCTOP}/contrib/libxo

.PATH: ${LIBXOSRC}/tests/xo ${LIBXOSRC}/tests/xo/saved

.c.out .o.out:

TESTSDIR=	${TESTSBASE}/usr.bin/xo

BINDIR=		${TESTSDIR}

ATF_TESTS_SH+=	functional_test

FILES+=		xo_01.H.err
FILES+=		xo_01.H.out
FILES+=		xo_01.HIPx.err
FILES+=		xo_01.HIPx.out
FILES+=		xo_01.HP.err
FILES+=		xo_01.HP.out
FILES+=		xo_01.J.err
FILES+=		xo_01.J.out
FILES+=		xo_01.JP.err
FILES+=		xo_01.JP.out
FILES+=		xo_01.T.err
FILES+=		xo_01.T.out
FILES+=		xo_01.X.err
FILES+=		xo_01.X.out
FILES+=		xo_01.XP.err
FILES+=		xo_01.XP.out

SCRIPTS+=	xo_01

LIBADD+=	xo

.include <bsd.test.mk>

.SUFFIXES: .sh
