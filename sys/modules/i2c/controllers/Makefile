# $FreeBSD$

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm amdsmb ichsmb intpm ismt nfsmb viapm lpbb pcf
.endif

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
SUBDIR += imcsmb
.endif

.include <bsd.subdir.mk>
