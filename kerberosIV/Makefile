# $FreeBSD$

MAINTAINER=	markm@FreeBSD.org

SUBDIR=	include lib libexec usr.bin usr.sbin

SDIR= ${.CURDIR}/..

CODAI=	${MAKE} ${MFLAGS} MAKE_KERBEROS4=yes cleandir && \
	${MAKE} ${MFLAGS} MAKE_KERBEROS4=yes obj && \
	${MAKE} ${MFLAGS} MAKE_KERBEROS4=yes depend all install

CODAIN=	${MAKE} ${MFLAGS} cleandir && \
	${MAKE} ${MFLAGS} obj && \
	${MAKE} ${MFLAGS} depend all install

CODAD=	${MAKE} ${MFLAGS} MAKE_KERBEROS4=yes cleandir && \
	${MAKE} ${MFLAGS} MAKE_KERBEROS4=yes obj && \
	${MAKE} ${MFLAGS} MAKE_KERBEROS4=yes depend all distribute

# These are the programs which depend on kerberos
kprog:
	cd ${SDIR}/bin/rcp; ${CODAI}
	cd ${SDIR}/gnu/usr.bin/cvs; ${CODAI}
	cd ${SDIR}/libexec/ftpd; ${CODAI}
	cd ${SDIR}/libexec/rlogind; ${CODAI}
	cd ${SDIR}/libexec/rshd; ${CODAI}
	cd ${SDIR}/usr.bin/login; ${CODAI}
	cd ${SDIR}/usr.bin/passwd; ${CODAI}
	cd ${SDIR}/usr.bin/rlogin; ${CODAI}
	cd ${SDIR}/usr.bin/rsh; ${CODAI}
	cd ${SDIR}/usr.bin/su; ${CODAI}
	cd ${SDIR}/sbin/dump; ${CODAI}
	cd ${SDIR}/sbin/restore; ${CODAI}

# These are the programs which depend on kerberos
# This target us used to rebuild these programs WITHOUT kerberos
dekerberise:
	cd ${SDIR}/bin/rcp; ${CODAIN}
	cd ${SDIR}/gnu/usr.bin/cvs; ${CODAIN}
	cd ${SDIR}/libexec/ftpd; ${CODAIN}
	cd ${SDIR}/libexec/rlogind; ${CODAIN}
	cd ${SDIR}/libexec/rshd; ${CODAIN}
	cd ${SDIR}/usr.bin/login; ${CODAIN}
	cd ${SDIR}/usr.bin/passwd; ${CODAIN}
	cd ${SDIR}/usr.bin/rlogin; ${CODAIN}
	cd ${SDIR}/usr.bin/rsh; ${CODAIN}
	cd ${SDIR}/usr.bin/su; ${CODAIN}
	cd ${SDIR}/sbin/dump; ${CODAIN}
	cd ${SDIR}/sbin/restore; ${CODAIN}

bootstrap: cleandir obj depend all install kprog

help-distribute:	distribute
	cd ${SDIR}/bin/rcp; ${CODAD}
	cd ${SDIR}/gnu/usr.bin/cvs; ${CODAD}
	cd ${SDIR}/libexec/ftpd; ${CODAD}
	cd ${SDIR}/libexec/rlogind; ${CODAD}
	cd ${SDIR}/libexec/rshd; ${CODAD}
	cd ${SDIR}/usr.bin/login; ${CODAD}
	cd ${SDIR}/usr.bin/passwd; ${CODAD}
	cd ${SDIR}/usr.bin/rlogin; ${CODAD}
	cd ${SDIR}/usr.bin/rsh; ${CODAD}
	cd ${SDIR}/usr.bin/su; ${CODAD}
	cd ${SDIR}/sbin/dump; ${CODAD}
	cd ${SDIR}/sbin/restore; ${CODAD}

.include <bsd.subdir.mk>
