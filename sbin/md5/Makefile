#	$Id: Makefile,v 1.3 1994/05/21 21:30:07 jkh Exp $

PROG=		md5
MAN1=		md5.1
SRCS=		md5c.c mddriver.c
CFLAGS+=	-DMD=5
CLEANFILES=	test.rfc

# For security reasons, this has to be static.
LDFLAGS=	-static

all:	${PROG} test

test: md5 test.rfc
	-./md5 -x | diff - test.rfc > diffs 2>&1
	@-if test -s diffs ; then \
		echo '*** MD5 TEST FAILED'; cat diffs; \
	else \
		echo '    MD5 Test Passed'; \
	fi
	rm -f diffs

# test.rfc is taken from Appendix 5 of RFC 1321.
test.rfc:
	@echo 'MD5 test suite:' > test.rfc
	@echo 'MD5 ("") = d41d8cd98f00b204e9800998ecf8427e' >> test.rfc
	@echo 'MD5 ("a") = 0cc175b9c0f1b6a831c399e269772661' >> test.rfc
	@echo 'MD5 ("abc") = 900150983cd24fb0d6963f7d28e17f72' >> test.rfc
	@echo 'MD5 ("message digest") = f96b697d7cb7938d525a2f31aaf161d0' >> test.rfc
	@echo 'MD5 ("abcdefghijklmnopqrstuvwxyz") = c3fcd3d76192e4007dfb496cca67e13b' >> test.rfc
	@echo 'MD5 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") = d174ab98d277d9f5a5611c2c9f419d9f' >> test.rfc
	@echo 'MD5 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") = 57edf4a22be3c955ac49da2e2107b67a' >> test.rfc

.include <bsd.prog.mk>
