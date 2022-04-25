PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/net/if_ovpn

ATF_TESTS_SH+=	if_ovpn

${PACKAGE}FILES+= \
	ca.crt \
	client.crt \
	client.key \
	dh.pem \
	server.crt \
	server.key \
	user.pass \
	utils.subr

.include <bsd.test.mk>
