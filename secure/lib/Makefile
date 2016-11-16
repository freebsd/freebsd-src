# $FreeBSD$

.include <src.opts.mk>

SUBDIR= 
.if ${MK_OPENSSL} != "no"
SUBDIR+=libcrypto libssl
.if ${MK_OPENSSH} != "no"
SUBDIR+=libssh
.endif
.endif

.if ${MK_TESTS} != "no"
SUBDIR+=tests
.endif

.include <bsd.subdir.mk>
