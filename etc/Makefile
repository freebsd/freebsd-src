#	from: @(#)Makefile	5.11 (Berkeley) 5/21/91
# $FreeBSD$

.if !defined(NO_SENDMAIL)
SUBDIR=	sendmail
.endif

BIN1=	amd.map apmd.conf auth.conf \
	crontab csh.cshrc csh.login csh.logout devd.conf devfs.conf \
	dhclient.conf disktab fbtab ftpusers gettytab group \
	hosts hosts.allow hosts.equiv hosts.lpd \
	inetd.conf login.access login.conf \
	mac.conf motd netconfig network.subr networks newsyslog.conf \
	pf.conf pf.os phones profile protocols \
	rc rc.bsdextended rc.firewall rc.firewall6 rc.sendmail rc.shutdown \
	rc.subr remote rpc services \
	shells sysctl.conf syslog.conf usbd.conf \
	etc.${MACHINE_ARCH}/ttys \
	${.CURDIR}/../gnu/usr.bin/man/manpath/manpath.config \
	${.CURDIR}/../usr.bin/mail/misc/mail.rc \
	${.CURDIR}/../usr.bin/locate/locate/locate.rc
.if !defined(NO_LPR)
BIN1+=	printcap
.endif

.if !defined(NO_CRYPT) && !defined(NO_OPENSSL)
.if !defined(NO_OPENSSH)
SSH=	${.CURDIR}/../crypto/openssh/ssh_config \
	${.CURDIR}/../crypto/openssh/sshd_config \
	${.CURDIR}/../crypto/openssh/moduli
.endif
SSL=	${.CURDIR}/../crypto/openssl/apps/openssl.cnf
.endif

# -rwxr-xr-x root:wheel, for the new cron root:wheel
BIN2=	netstart pccard_ether rc.suspend rc.resume

MTREE=	BSD.include.dist BSD.local.dist BSD.root.dist BSD.usr.dist \
	BSD.var.dist BSD.x11.dist BSD.x11-4.dist
.if !defined(NO_SENDMAIL)
MTREE+=	BSD.sendmail.dist
.endif
.if !defined(NO_BIND)
MTREE+=	BIND.chroot.dist
.if defined(WITH_BIND_LIBS)
MTREE+=	BIND.include.dist
.endif
.endif

.if !defined(NO_BIND_ETC) && !defined(NO_BIND)
NAMEDB=	PROTO.localhost.rev PROTO.localhost-v6.rev named.conf named.root \
	make-localhost
.endif

PPPCNF=	ppp.conf

.if defined(NO_SENDMAIL)
ETCMAIL=mailer.conf aliases
.else
ETCMAIL=Makefile README mailer.conf access.sample virtusertable.sample \
	mailertable.sample aliases
.endif

# Special top level files for FreeBSD
FREEBSD=COPYRIGHT

afterinstall:
.if !defined(NO_MAN)
	cd ${.CURDIR}/../share/man; ${MAKE} makedb
.endif

distribute:
	cd ${.CURDIR} ; ${MAKE} install DESTDIR=${DISTDIR}/${DISTRIBUTION}
	cd ${.CURDIR} ; ${MAKE} distribution DESTDIR=${DISTDIR}/${DISTRIBUTION}

.include <bsd.endian.mk>
.if ${TARGET_ENDIANNESS} == "1234"
CAP_MKDB_ENDIAN= -l
PWD_MKDB_ENDIAN= -L
.elif ${TARGET_ENDIANNESS} == "4321"
CAP_MKDB_ENDIAN= -b
PWD_MKDB_ENDIAN= -B
.else
CAP_MKDB_ENDIAN=
.endif

distribution:
	cd ${.CURDIR}; \
	    ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
		${BIN1} ${DESTDIR}/etc; \
	    cap_mkdb ${CAP_MKDB_ENDIAN} ${DESTDIR}/etc/login.conf; \
	    ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 755 \
		${BIN2} ${DESTDIR}/etc; \
	    ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 600 \
		master.passwd nsmb.conf opieaccess ${DESTDIR}/etc; \
	    pwd_mkdb ${PWD_MKDB_ENDIAN} -p -d ${DESTDIR}/etc \
		${DESTDIR}/etc/master.passwd
	cd ${.CURDIR}/bluetooth; ${MAKE} install
	cd ${.CURDIR}/defaults; ${MAKE} install
	cd ${.CURDIR}/periodic; ${MAKE} install
	cd ${.CURDIR}/rc.d; ${MAKE} install
	cd ${.CURDIR}/../gnu/usr.bin/send-pr; ${MAKE} etc-gnats-freefall
	cd ${.CURDIR}/../share/termcap; ${MAKE} etc-termcap
	cd ${.CURDIR}/../usr.sbin/rmt; ${MAKE} etc-rmt
	cd ${.CURDIR}/pam.d; ${MAKE} install
.if !defined(NO_I4B)
	cd ${.CURDIR}/isdn; ${MAKE} install
.endif
.if !defined(NO_SENDMAIL)
	cd ${.CURDIR}/sendmail; ${MAKE} distribution
.endif
.if !defined(NO_OPENSSL)
.if !defined(NO_OPENSSH)
	cd ${.CURDIR}; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${SSH} ${DESTDIR}/etc/ssh
.endif
	cd ${.CURDIR}; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${SSL} ${DESTDIR}/etc/ssl
.endif
.if !defined(NO_KERBEROS)
	cd ${.CURDIR}/root; \
	    ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
		dot.k5login ${DESTDIR}/root/.k5login;
.endif
	cd ${.CURDIR}/root; \
	    ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
		dot.cshrc ${DESTDIR}/root/.cshrc; \
	    ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
		dot.login ${DESTDIR}/root/.login; \
	    ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
		dot.profile ${DESTDIR}/root/.profile; \
	    rm -f ${DESTDIR}/.cshrc ${DESTDIR}/.profile; \
	    ln ${DESTDIR}/root/.cshrc ${DESTDIR}/.cshrc; \
	    ln ${DESTDIR}/root/.profile ${DESTDIR}/.profile
	cd ${.CURDIR}/mtree; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${MTREE} ${DESTDIR}/etc/mtree
.if !defined(NO_BIND)
.if !defined(NO_BIND_ETC)
	cd ${.CURDIR}/namedb; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${NAMEDB} ${DESTDIR}/var/named/etc/namedb
.endif
.if !defined(NO_BIND_MTREE)
	@if [ ! -e ${DESTDIR}/etc/namedb ]; then \
		set -x; \
		ln -s ../var/named/etc/namedb ${DESTDIR}/etc/namedb; \
	fi
.endif
.endif
	cd ${.CURDIR}/ppp; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 600 \
	    ${PPPCNF} ${DESTDIR}/etc/ppp
	cd ${.CURDIR}/mail; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${ETCMAIL} ${DESTDIR}/etc/mail
	@if [ -d ${DESTDIR}/etc/mail -a -f ${DESTDIR}/etc/mail/aliases -a \
	      ! -f ${DESTDIR}/etc/aliases ]; then \
		set -x; \
		ln -s mail/aliases ${DESTDIR}/etc/aliases; \
	fi
	${INSTALL} -o ${BINOWN} -g operator -m 664 /dev/null \
	    ${DESTDIR}/etc/dumpdates
	${INSTALL} -o nobody -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/db/locate.database
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/log/auth.log
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/log/cron
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/log/debug.log
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/log/xferlog
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/lpd-errs
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 640 /dev/null \
	    ${DESTDIR}/var/log/maillog
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/lastlog
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/messages
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/log/security
	${INSTALL} -o ${BINOWN} -g network -m 640 /dev/null \
	    ${DESTDIR}/var/log/slip.log
	${INSTALL} -o ${BINOWN} -g network -m 640 /dev/null \
	    ${DESTDIR}/var/log/ppp.log
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/wtmp
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/run/utmp
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 ${.CURDIR}/minfree \
	    ${DESTDIR}/var/crash
	cd ${.CURDIR}/..; ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 444 \
		${FREEBSD} ${DESTDIR}/
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/../sys/${MACHINE}/conf/GENERIC.hints \
	    ${DESTDIR}/boot/device.hints

distrib-dirs:
	mtree -eU ${MTREE_FOLLOWS_SYMLINKS} -f ${.CURDIR}/mtree/BSD.root.dist -p ${DESTDIR}/
	mtree -eU ${MTREE_FOLLOWS_SYMLINKS} -f ${.CURDIR}/mtree/BSD.var.dist -p ${DESTDIR}/var
	mtree -eU ${MTREE_FOLLOWS_SYMLINKS} -f ${.CURDIR}/mtree/BSD.usr.dist -p ${DESTDIR}/usr
	mtree -eU ${MTREE_FOLLOWS_SYMLINKS} -f ${.CURDIR}/mtree/BSD.include.dist \
		-p ${DESTDIR}/usr/include
.if !defined(NO_BIND)
.if defined(WITH_BIND_LIBS)
	mtree -deU ${MTREE_FOLLOWS_SYMLINKS} -f ${.CURDIR}/mtree/BIND.include.dist \
	    -p ${DESTDIR}/usr/include
.endif
.if !defined(NO_BIND_MTREE)
	mtree -deU ${MTREE_FOLLOWS_SYMLINKS} -f ${.CURDIR}/mtree/BIND.chroot.dist \
	    -p ${DESTDIR}/var/named
.endif
.endif
.if !defined(NO_SENDMAIL)
	mtree -deU ${MTREE_FOLLOWS_SYMLINKS} -f ${.CURDIR}/mtree/BSD.sendmail.dist -p ${DESTDIR}/
.endif
	cd ${DESTDIR}/; rm -f ${DESTDIR}/sys; ln -s usr/src/sys sys
	cd ${DESTDIR}/usr/share/man/en.ISO8859-1; ln -sf ../man* .
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
	cd ${.CURDIR}/defaults; ${MAKE} install \
	    DESTDIR=${DESTDIR}/usr/share/examples

.include <bsd.prog.mk>
