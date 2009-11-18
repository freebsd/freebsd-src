#	from: @(#)Makefile	5.11 (Berkeley) 5/21/91
# $FreeBSD$

.include <bsd.own.mk>

.if ${MK_SENDMAIL} != "no"
SUBDIR=	sendmail
.endif

BIN1=	auth.conf \
	crontab devd.conf devfs.conf \
	ddb.conf dhclient.conf disktab fbtab \
	ftpusers gettytab group \
	hosts hosts.allow hosts.equiv \
	inetd.conf libalias.conf login.access login.conf mac.conf motd \
	netconfig network.subr networks newsyslog.conf nsswitch.conf \
	phones profile protocols \
	rc rc.bsdextended rc.firewall rc.firewall6 rc.initdiskless \
	rc.sendmail rc.shutdown \
	rc.subr remote rpc services shells \
	sysctl.conf syslog.conf

.if exists(${.CURDIR}/etc.${MACHINE}/ttys)
BIN1+=	etc.${MACHINE}/ttys
.else
BIN1+=	etc.${MACHINE_ARCH}/ttys
.endif

OPENBSMDIR=			${.CURDIR}/../contrib/openbsm
BSM_ETC_OPEN_FILES=		${OPENBSMDIR}/etc/audit_class \
				${OPENBSMDIR}/etc/audit_event
BSM_ETC_RESTRICTED_FILES=	${OPENBSMDIR}/etc/audit_control \
				${OPENBSMDIR}/etc/audit_user
BSM_ETC_EXEC_FILES=		${OPENBSMDIR}/etc/audit_warn
BSM_ETC_DIR=			${DESTDIR}/etc/security

# NB: keep these sorted by MK_* knobs

.if ${MK_AMD} != "no"
BIN1+= amd.map
.endif

.if ${MK_APM} != "no"
BIN1+= apmd.conf
.endif

.if ${MK_BSNMP} != "no"
BIN1+= snmpd.config
.endif

.if ${MK_FREEBSD_UPDATE} != "no"
BIN1+= freebsd-update.conf
.endif

.if ${MK_LOCATE} != "no"
BIN1+=	${.CURDIR}/../usr.bin/locate/locate/locate.rc
.endif

.if ${MK_LPR} != "no"
BIN1+=	hosts.lpd printcap
.endif

.if ${MK_MAIL} != "no"
BIN1+=	${.CURDIR}/../usr.bin/mail/misc/mail.rc
.endif

.if ${MK_MAN} != "no"
BIN1+=	${.CURDIR}/../gnu/usr.bin/man/manpath/manpath.config
.endif

.if ${MK_NTP} != "no"
BIN1+=	ntp.conf
.endif

.if ${MK_OPENSSH} != "no"
SSH=	${.CURDIR}/../crypto/openssh/ssh_config \
	${.CURDIR}/../crypto/openssh/sshd_config \
	${.CURDIR}/../crypto/openssh/moduli
.endif
.if ${MK_OPENSSL} != "no"
SSL=	${.CURDIR}/../crypto/openssl/apps/openssl.cnf
.endif

.if ${MK_NS_CACHING} != "no"
BIN1+= nscd.conf
.endif

.if ${MK_PORTSNAP} != "no"
BIN1+= portsnap.conf
.endif

.if ${MK_PF} != "no"
BIN1+= pf.os
.endif

.if ${MK_TCSH} != "no"
BIN1+= csh.cshrc csh.login csh.logout
.endif

.if ${MK_WIRELESS} != "no"
BIN1+= regdomain.xml
.endif

# -rwxr-xr-x root:wheel, for the new cron root:wheel
BIN2=	netstart pccard_ether rc.suspend rc.resume

MTREE=	BSD.include.dist BSD.root.dist BSD.usr.dist BSD.var.dist
.if ${MK_SENDMAIL} != "no"
MTREE+=	BSD.sendmail.dist
.endif
.if ${MK_BIND} != "no"
MTREE+=	BIND.chroot.dist
.if ${MK_BIND_LIBS} != "no"
MTREE+=	BIND.include.dist
.endif
.endif

PPPCNF=	ppp.conf

.if ${MK_SENDMAIL} == "no"
ETCMAIL=mailer.conf aliases
.else
ETCMAIL=Makefile README mailer.conf access.sample virtusertable.sample \
	mailertable.sample aliases
.endif

# Special top level files for FreeBSD
FREEBSD=COPYRIGHT

afterinstall:
.if ${MK_MAN} != "no"
	${_+_}cd ${.CURDIR}/../share/man; ${MAKE} makedb
.endif

distribute:
	${_+_}cd ${.CURDIR} ; ${MAKE} install DESTDIR=${DISTDIR}/${DISTRIBUTION}
	${_+_}cd ${.CURDIR} ; ${MAKE} distribution DESTDIR=${DISTDIR}/${DISTRIBUTION}

.include <bsd.endian.mk>
.if ${TARGET_ENDIANNESS} == "1234"
CAP_MKDB_ENDIAN?= -l
PWD_MKDB_ENDIAN?= -L
.elif ${TARGET_ENDIANNESS} == "4321"
CAP_MKDB_ENDIAN?= -b
PWD_MKDB_ENDIAN?= -B
.else
CAP_MKDB_ENDIAN?=
PWD_MKDB_ENDIAN?=
.endif

distribution:
.if !defined(DESTDIR)
	@echo "set DESTDIR before running \"make ${.TARGET}\""
	@false
.endif
	cd ${.CURDIR}; \
	    ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
		${BIN1} ${DESTDIR}/etc; \
	    cap_mkdb ${CAP_MKDB_ENDIAN} ${DESTDIR}/etc/login.conf; \
	    ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 755 \
		${BIN2} ${DESTDIR}/etc; \
	    ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 600 \
		master.passwd nsmb.conf opieaccess ${DESTDIR}/etc;
.if ${MK_TCSH} == "no"
	sed -i "" -e 's;/bin/csh;/bin/sh;' ${DESTDIR}/etc/master.passwd
.endif
	pwd_mkdb ${PWD_MKDB_ENDIAN} -i -p -d ${DESTDIR}/etc \
	    ${DESTDIR}/etc/master.passwd
.if ${MK_BLUETOOTH} != "no"
	${_+_}cd ${.CURDIR}/bluetooth; ${MAKE} install
.endif
	${_+_}cd ${.CURDIR}/defaults; ${MAKE} install
	${_+_}cd ${.CURDIR}/devd; ${MAKE} install
	${_+_}cd ${.CURDIR}/gss; ${MAKE} install
	${_+_}cd ${.CURDIR}/periodic; ${MAKE} install
	${_+_}cd ${.CURDIR}/rc.d; ${MAKE} install
	${_+_}cd ${.CURDIR}/../gnu/usr.bin/send-pr; ${MAKE} etc-gnats-freefall
	${_+_}cd ${.CURDIR}/../share/termcap; ${MAKE} etc-termcap
	${_+_}cd ${.CURDIR}/../usr.sbin/rmt; ${MAKE} etc-rmt
	${_+_}cd ${.CURDIR}/pam.d; ${MAKE} install
	cd ${.CURDIR}; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 0444 \
	    ${BSM_ETC_OPEN_FILES} ${BSM_ETC_DIR}
	cd ${.CURDIR}; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 0600 \
	    ${BSM_ETC_RESTRICTED_FILES} ${BSM_ETC_DIR}
	cd ${.CURDIR}; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 0500 \
	    ${BSM_ETC_EXEC_FILES} ${BSM_ETC_DIR}
.if ${MK_BIND_MTREE} != "no"
	@if [ ! -e ${DESTDIR}/etc/namedb ]; then \
		set -x; \
		ln -s ../var/named/etc/namedb ${DESTDIR}/etc/namedb; \
	fi
.endif
.if ${MK_BIND_ETC} != "no"
	${_+_}cd ${.CURDIR}/namedb; ${MAKE} install
.endif
.if ${MK_SENDMAIL} != "no"
	${_+_}cd ${.CURDIR}/sendmail; ${MAKE} distribution
.endif
.if ${MK_OPENSSH} != "no"
	cd ${.CURDIR}; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${SSH} ${DESTDIR}/etc/ssh
.endif
.if ${MK_OPENSSL} != "no"
	cd ${.CURDIR}; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${SSL} ${DESTDIR}/etc/ssl
.endif
.if ${MK_KERBEROS} != "no"
	cd ${.CURDIR}/root; \
	    ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
		dot.k5login ${DESTDIR}/root/.k5login;
.endif
	cd ${.CURDIR}/root; \
	    ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
		dot.profile ${DESTDIR}/root/.profile; \
	    rm -f ${DESTDIR}/.profile; \
	    ln ${DESTDIR}/root/.profile ${DESTDIR}/.profile
.if ${MK_TCSH} != "no"
	cd ${.CURDIR}/root; \
	    ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
		dot.cshrc ${DESTDIR}/root/.cshrc; \
	    ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
		dot.login ${DESTDIR}/root/.login; \
	    rm -f ${DESTDIR}/.cshrc; \
	    ln ${DESTDIR}/root/.cshrc ${DESTDIR}/.cshrc
.endif
	cd ${.CURDIR}/mtree; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${MTREE} ${DESTDIR}/etc/mtree
.if ${MK_PPP} != "no"
	cd ${.CURDIR}/ppp; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 600 \
	    ${PPPCNF} ${DESTDIR}/etc/ppp
.endif
.if ${MK_MAIL} != "no"
	cd ${.CURDIR}/mail; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${ETCMAIL} ${DESTDIR}/etc/mail
	@if [ -d ${DESTDIR}/etc/mail -a -f ${DESTDIR}/etc/mail/aliases -a \
	      ! -f ${DESTDIR}/etc/aliases ]; then \
		set -x; \
		ln -s mail/aliases ${DESTDIR}/etc/aliases; \
	fi
.endif
	${INSTALL} -o ${BINOWN} -g operator -m 664 /dev/null \
	    ${DESTDIR}/etc/dumpdates
	${INSTALL} -o nobody -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/db/locate.database
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 ${.CURDIR}/minfree \
	    ${DESTDIR}/var/crash
	cd ${.CURDIR}/..; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 444 \
		${FREEBSD} ${DESTDIR}/
.if exists(${.CURDIR}/../sys/${MACHINE}/conf/GENERIC.hints)
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/../sys/${MACHINE}/conf/GENERIC.hints \
	    ${DESTDIR}/boot/device.hints
.endif

distrib-dirs:
	mtree -eU ${MTREE_FOLLOWS_SYMLINKS} -f ${.CURDIR}/mtree/BSD.root.dist -p ${DESTDIR}/
	mtree -eU ${MTREE_FOLLOWS_SYMLINKS} -f ${.CURDIR}/mtree/BSD.var.dist -p ${DESTDIR}/var
	mtree -eU ${MTREE_FOLLOWS_SYMLINKS} -f ${.CURDIR}/mtree/BSD.usr.dist -p ${DESTDIR}/usr
	mtree -eU ${MTREE_FOLLOWS_SYMLINKS} -f ${.CURDIR}/mtree/BSD.include.dist \
		-p ${DESTDIR}/usr/include
.if ${MK_BIND_LIBS} != "no"
	mtree -deU ${MTREE_FOLLOWS_SYMLINKS} -f ${.CURDIR}/mtree/BIND.include.dist \
	    -p ${DESTDIR}/usr/include
.endif
.if ${MK_BIND_MTREE} != "no"
	mtree -deU ${MTREE_FOLLOWS_SYMLINKS} -f ${.CURDIR}/mtree/BIND.chroot.dist \
	    -p ${DESTDIR}/var/named
.endif
.if ${MK_SENDMAIL} != "no"
	mtree -deU ${MTREE_FOLLOWS_SYMLINKS} -f ${.CURDIR}/mtree/BSD.sendmail.dist -p ${DESTDIR}/
.endif
	cd ${DESTDIR}/; rm -f ${DESTDIR}/sys; ln -s usr/src/sys sys
	cd ${DESTDIR}/usr/share/man/en.ISO8859-1; ln -sf ../man* .
	cd ${DESTDIR}/usr/share/man/en.UTF-8; ln -sf ../man* .
	cd ${DESTDIR}/usr/share/man; \
	set - `grep "^[a-zA-Z]" ${.CURDIR}/man.alias`; \
	while [ $$# -gt 0 ] ; \
	do \
		rm -rf "$$1"; \
		ln -s "$$2" "$$1"; \
		shift; shift; \
	done
	cd ${DESTDIR}/usr/share/openssl/man; \
	set - `grep "^[a-zA-Z]" ${.CURDIR}/man.alias`; \
	while [ $$# -gt 0 ] ; \
	do \
		rm -rf "$$1"; \
		ln -s "$$2" "$$1"; \
		shift; shift; \
	done
	cd ${DESTDIR}/usr/share/openssl/man/en.ISO8859-1; ln -sf ../man* .
	cd ${DESTDIR}/usr/share/nls; \
	set - `grep "^[a-zA-Z]" ${.CURDIR}/nls.alias`; \
	while [ $$# -gt 0 ] ; \
	do \
		rm -rf "$$1"; \
		ln -s "$$2" "$$1"; \
		shift; shift; \
	done

etc-examples:
	cd ${.CURDIR}; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${BIN1} ${BIN2} nsmb.conf opieaccess \
	    ${DESTDIR}/usr/share/examples/etc
	${_+_}cd ${.CURDIR}/defaults; ${MAKE} install \
	    DESTDIR=${DESTDIR}/usr/share/examples

.include <bsd.prog.mk>
