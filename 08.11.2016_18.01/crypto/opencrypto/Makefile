# $NetBSD: Makefile,v 1.2 2014/01/17 22:33:02 pgoyette Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/crypto/opencrypto

TESTS_SH+=	t_opencrypto

BINDIR=		${TESTSDIR}
MKMAN=		no

PROGS+=		h_aesctr1
PROGS+=		h_aesctr2
PROGS+=		h_arc4
PROGS+=		h_camellia
PROGS+=		h_cbcdes
PROGS+=		h_comp
PROGS+=		h_comp_zlib
PROGS+=		h_comp_zlib_rnd
PROGS+=		h_gcm
PROGS+=		h_md5
PROGS+=		h_md5hmac
PROGS+=		h_null
PROGS+=		h_sha1hmac
PROGS+=		h_xcbcmac

LDADD.h_comp_zlib+=	-lz
LDADD.h_comp_zlib_rnd+=	-lz

.include <bsd.test.mk>

