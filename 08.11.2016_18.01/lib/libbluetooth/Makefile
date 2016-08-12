#	$NetBSD: Makefile,v 1.1 2011/04/07 06:21:57 plunky Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libbluetooth

DPADD+=		${LIBBLUETOOTH}
LDADD+=		-lbluetooth

TESTS_C+=	t_bluetooth
TESTS_C+=	t_sdp_data
TESTS_C+=	t_sdp_get
TESTS_C+=	t_sdp_match
TESTS_C+=	t_sdp_put
TESTS_C+=	t_sdp_set

.include <bsd.test.mk>
