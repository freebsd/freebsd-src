#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.12 1994/09/30 13:34:39 g89r4222 Exp $

SUBDIR=	include

SUBDIR+= des compile_et acl ext_srvtab include kdb kdb_destroy kdb_edit \
	 kdb_init kdb_util kdestroy kerberos kinit klist krb ksrvtgt \
	 kstash man register registerd make_keypair

SDIR= ${.CURDIR}/..

# These are the programs which depend on kerberos
# It's nice to know who they are
kprog:
	cd ${SDIR}/bin/rcp; make cleandir obj ; make -DNOMAN depend all install
	cd ${SDIR}/libexec/rlogind;make cleandir;make -DNOMAN depend all install
	cd ${SDIR}/libexec/rshd; make cleandir; make -DNOMAN depend all install
	cd ${SDIR}/usr.bin/login; make cleandir; make -DNOMAN depend all install
	cd ${SDIR}/usr.bin/passwd;make cleandir; make -DNOMAN depend all install
	cd ${SDIR}/usr.bin/rlogin;make cleandir; make -DNOMAN depend all install
	cd ${SDIR}/usr.bin/rsh; make cleandir; make -DNOMAN depend all install
	cd ${SDIR}/usr.bin/su; make cleandir; make -DNOMAN depend all install
	cd ${SDIR}/libexec/kpasswdd; make cleandir; make depend all install

.include <bsd.subdir.mk>
