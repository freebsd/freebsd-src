# $FreeBSD$

.include <src.opts.mk>

SUBDIR= 
.if ${MK_OPENSSH} != "no"
SUBDIR+=sshd
.endif

.if ${MK_TESTS} != "no"
SUBDIR+=tests
.endif

.include <bsd.subdir.mk>
