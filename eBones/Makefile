#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.2 1994/10/06 00:32:42 wollman Exp $

SUBDIR=	include

SUBDIR+= des compile_et acl ext_srvtab include kdb kdb_destroy kdb_edit \
	 kdb_init kdb_util kdestroy kerberos kinit klist krb ksrvtgt \
	 kstash man
#
# MISSING AND PROBABLY BOGUS: register registerd make_keypair
#

SDIR= ${.CURDIR}/..

CODAI?=	${MAKE} ${MFLAGS} cleandir; \
	${MAKE} ${MFLAGS} obj; ${MAKE} ${MFLAGS} depend all install

# These are the programs which depend on kerberos
# It's nice to know who they are
kprog:
	cd ${SDIR}/bin/rcp; ${CODAI}
	cd ${SDIR}/libexec/rlogind; ${CODAI}
	cd ${SDIR}/libexec/rshd; ${CODAI}
	cd ${SDIR}/usr.bin/login; ${CODAI}
	cd ${SDIR}/usr.bin/passwd; ${CODAI}
	cd ${SDIR}/usr.bin/rlogin; ${CODAI}
	cd ${SDIR}/usr.bin/rsh; ${CODAI}
	cd ${SDIR}/usr.bin/su; ${CODAI}
	cd ${SDIR}/libexec/kpasswdd; ${CODAI}

bootstrap:
	cd include; ${MAKE} ${MFLAGS} install
	${MAKE} ${MFLAGS} cleandir
	${MAKE} ${MFLAGS} obj
	@echo removing old 1.1.5 Kerberos libraries
	rm -f /usr/lib/libkrb.so.4.0
	rm -f /usr/lib/libdes.so.4.0
	rm -f /usr/lib/libkdb.so.4.0
	${MAKE} ${MFLAGS} depend all install kprog

.include <bsd.subdir.mk>
