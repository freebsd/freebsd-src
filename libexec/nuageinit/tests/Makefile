PACKAGE=	tests
.PATH:		${SRCTOP}/usr.sbin/pw/tests

BINDIR=		${TESTSDIR}

PROGS=		crypt
LIBADD=		crypt

ATF_TESTS_SH=	nuage utils nuageinit

${PACKAGE}FILES+=	addgroup.lua
${PACKAGE}FILES+=	addsshkey.lua
${PACKAGE}FILES+=	adduser.lua
${PACKAGE}FILES+=	adduser_passwd.lua
${PACKAGE}FILES+=	dirname.lua
${PACKAGE}FILES+=	err.lua
${PACKAGE}FILES+=	sethostname.lua
${PACKAGE}FILES+=	settimezone.lua
${PACKAGE}FILES+=	warn.lua
${PACKAGE}FILES+=	addfile.lua

.include <bsd.test.mk>
