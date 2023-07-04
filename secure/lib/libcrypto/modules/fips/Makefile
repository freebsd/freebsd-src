# $FreeBSD$

SHLIB_NAME?=	fips.so

SRCS=	fips_entry.c fipsprov.c self_test.c self_test_kats.c

.include <bsd.lib.mk>

.PATH:	${LCRYPTO_SRC}/crypto \
	${LCRYPTO_SRC}/providers/fips
