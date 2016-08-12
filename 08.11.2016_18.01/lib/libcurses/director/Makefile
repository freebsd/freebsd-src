#	$NetBSD: Makefile,v 1.8 2014/01/19 22:09:34 apb Exp $
#
.include <bsd.own.mk>

ATFFILE=	no
TESTSDIR=       ${TESTSBASE}/lib/libcurses
TESTS_C+=	director

PROG=	director
NOMAN=	"true"
#MAN=	director.7
SRCS=	testlang_parse.y testlang_conf.l director.c

YHEADER=	1
CPPFLAGS+=	-I${.CURDIR} -I.
DPADD+=		${LIBL}
LDADD+=		-ll -lutil -lc

COPTS.testlang_parse.c += -Wno-uninitialized
CWARNFLAGS.clang+=	-Wno-format -Wno-conversion

.include <bsd.test.mk>
