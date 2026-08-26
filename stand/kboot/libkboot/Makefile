.include <bsd.init.mk>

LIB=	kboot
WARNS?= 4

CERT_DIR?=	${SRCTOP}/secure/caroot/trusted
TRUST_ANCHORS=	trust_anchors.inc
BRSSL_TA?=	brssl_ta

${TRUST_ANCHORS}: ${CERT_DIR}/*.pem
	${BRSSL_TA} -q ${CERT_DIR}/*.pem > ${.TARGET}

DPSRCS+=		${TRUST_ANCHORS}
CLEANFILES+=		${TRUST_ANCHORS}

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/arch/${MACHINE_ARCH}
CFLAGS+=-I${LDRSRC}

SRCS=	crt1.c
SRCS+=	dfk.c
SRCS+=	host_syscall.S
SRCS+=	host_syscalls.c
SRCS+=	seg.c
SRCS+=	termios.c
SRCS+=	util.c

.if ${MACHINE_ARCH} != "powerpc64"
SRCS+=	efi.c
.endif

.sinclude "${.CURDIR}/arch/${MACHINE_ARCH}/Makefile.inc"

.include <bsd.lib.mk>
