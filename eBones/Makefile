#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.22 1996/10/01 14:07:34 jkh Exp $

SUBDIR=	include lib libexec usr.bin usr.sbin man

SDIR= ${.CURDIR}/..

CODAI=	${MAKE} ${MFLAGS} cleandir; \
	${MAKE} ${MFLAGS} obj; \
	${MAKE} ${MFLAGS} depend all install

CODAD=	${MAKE} ${MFLAGS} MAKE_EBONES=yes cleandir; \
	${MAKE} ${MFLAGS} MAKE_EBONES=yes obj; \
	${MAKE} ${MFLAGS} MAKE_EBONES=yes depend all distribute

# These are the programs which depend on kerberos
kprog:
	cd ${SDIR}/bin/rcp; ${CODAI}
	cd ${SDIR}/libexec/rlogind; ${CODAI}
	cd ${SDIR}/libexec/rshd; ${CODAI}
	cd ${SDIR}/sbin/mount_nfs; ${CODAI}
	cd ${SDIR}/sbin/nfsd; ${CODAI}
	cd ${SDIR}/usr.bin/login; ${CODAI}
	cd ${SDIR}/usr.bin/passwd; ${CODAI}
	cd ${SDIR}/usr.bin/rlogin; ${CODAI}
	cd ${SDIR}/usr.bin/rsh; ${CODAI}
	cd ${SDIR}/usr.bin/su; ${CODAI}

bootstrap:
	( cd ${.CURDIR}/include; ${MAKE} ${MFLAGS} install )
	( cd ${.CURDIR}/lib; ${MAKE} ${MFLAGS} depend all install )
	( cd ${.CURDIR}; \
	  ${MAKE} ${MFLAGS} cleandir; \
	  ${MAKE} ${MFLAGS} obj; \
	  ${MAKE} ${MFLAGS} depend all install kprog )

help-distribute:	distribute
	cd ${SDIR}/bin/rcp; ${CODAD}
	cd ${SDIR}/libexec/rlogind; ${CODAD}
	cd ${SDIR}/libexec/rshd; ${CODAD}
	cd ${SDIR}/sbin/mount_nfs; ${CODAD}
	cd ${SDIR}/sbin/nfsd; ${CODAD}
	cd ${SDIR}/usr.bin/login; ${CODAD}
	cd ${SDIR}/usr.bin/passwd; ${CODAD}
	cd ${SDIR}/usr.bin/rlogin; ${CODAD}
	cd ${SDIR}/usr.bin/rsh; ${CODAD}
	cd ${SDIR}/usr.bin/su; ${CODAD}

.include <bsd.subdir.mk>
