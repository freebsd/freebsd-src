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

.if defined(LIBCHERI)
.if ${MK_LIBCHERI_ALL} == "no"
SUBDIR:=
.else
.if ${MK_LIBCHERI_BROKEN} == "no"
# >90 minutes to compile wb_block.c
SUBDIR:=	${SUBDIR:Nlibcrypto}
.endif
.endif
.endif

.include <bsd.subdir.mk>
