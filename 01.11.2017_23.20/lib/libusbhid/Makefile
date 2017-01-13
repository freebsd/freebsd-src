#	$NetBSD: Makefile,v 1.1 2016/01/01 21:38:54 jakllsch Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libusbhid

DPADD+=		${LIBUSBHID}
LDADD+=		-lusbhid

TESTS_C+=	t_usbhid

FILES=          test_usb_hid_usages
FILESDIR=       ${TESTSDIR}

.include <bsd.test.mk>
