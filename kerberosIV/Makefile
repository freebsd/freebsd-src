#	$Id: Makefile,v 1.5 1997/10/02 22:59:48 jkh Exp $

SUBDIR=	include lib libexec usr.bin usr.sbin

SDIR= ${.CURDIR}/..

CODAI=	${MAKE} ${MFLAGS} cleandir; \
	${MAKE} ${MFLAGS} obj; \
	${MAKE} ${MFLAGS} depend all install

CODAD=	${MAKE} ${MFLAGS} MAKE_KERBEROS4=yes cleandir; \
	${MAKE} ${MFLAGS} MAKE_KERBEROS4=yes obj; \
	${MAKE} ${MFLAGS} MAKE_KERBEROS4=yes depend all distribute

# These are the programs which depend on kerberos
kprog:
	cd ${SDIR}/bin/rcp; ${CODAI}
	cd ${SDIR}/libexec/rlogind; ${CODAI}
	cd ${SDIR}/libexec/rshd; ${CODAI}
	cd ${SDIR}/libexec/telnetd; ${CODAI}
	cd ${SDIR}/usr.bin/login; ${CODAI}
	cd ${SDIR}/usr.bin/passwd; ${CODAI}
	cd ${SDIR}/usr.bin/rlogin; ${CODAI}
	cd ${SDIR}/usr.bin/rsh; ${CODAI}
	cd ${SDIR}/usr.bin/su; ${CODAI}
	cd ${SDIR}/usr.bin/telnet; ${CODAI}
	cd ${SDIR}/sbin/dump; ${CODAI}
	cd ${SDIR}/sbin/restore; ${CODAI}

bootstrap: cleandir obj depend all install kprog

help-distribute:	distribute
	cd ${SDIR}/bin/rcp; ${CODAD}
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
