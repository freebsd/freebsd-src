#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.7 1995/01/20 02:49:46 wollman Exp $

SUBDIR=	include des acl kdb krb libkadm

SUBDIR+= ext_srvtab kdb_destroy kdb_edit \
	 kdb_init kdb_util kdestroy kerberos kinit klist ksrvtgt \
	 kstash kadmin kadmind man
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

help-distribute:	distribute
	cd ${SDIR}/bin/rcp; ${MAKE} ${MFLAGS} distribute
	cd ${SDIR}/libexec/rlogind; ${MAKE} ${MFLAGS} distribute
	cd ${SDIR}/libexec/rshd; ${MAKE} ${MFLAGS} distribute
	cd ${SDIR}/usr.bin/login; ${MAKE} ${MFLAGS} distribute
	cd ${SDIR}/usr.bin/passwd; ${MAKE} ${MFLAGS} distribute
	cd ${SDIR}/usr.bin/rlogin; ${MAKE} ${MFLAGS} distribute
	cd ${SDIR}/usr.bin/rsh; ${MAKE} ${MFLAGS} distribute
	cd ${SDIR}/usr.bin/su; ${MAKE} ${MFLAGS} distribute
	cd ${SDIR}/libexec/kpasswdd; ${MAKE} ${MFLAGS} distribute


.include <bsd.subdir.mk>

