# $FreeBSD$

SUBDIR=	lib libexec usr.bin usr.sbin

SDIR= ${.CURDIR}/../secure

bootstrap: cleandir obj depend all install kprog

help-distribute:	distribute

CODAI=	${MAKE} ${MFLAGS} MAKE_KERBEROS5=yes cleandir && \
	${MAKE} ${MFLAGS} MAKE_KERBEROS5=yes obj && \
	${MAKE} ${MFLAGS} MAKE_KERBEROS5=yes depend all install

CODAD=	${MAKE} ${MFLAGS} MAKE_KERBEROS5=yes cleandir && \
	${MAKE} ${MFLAGS} MAKE_KERBEROS5=yes obj && \
	${MAKE} ${MFLAGS} MAKE_KERBEROS5=yes depend all distribute

kprog:
	cd ${SDIR}/usr.bin/ssh; ${CODAI}
	cd ${SDIR}/usr.sbin/sshd; ${CODAI}

help-distribute:	distribute
	cd ${SDIR}/usr.bin/ssh; ${CODAD}
	cd ${SDIR}/usr.sbin/sshd; ${CODAD}

.include <bsd.subdir.mk>
