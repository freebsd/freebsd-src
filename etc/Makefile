#	from: @(#)Makefile	5.11 (Berkeley) 5/21/91
# $FreeBSD$

SUBDIR=	sendmail

BIN1=   amd.map apmd.conf auth.conf \
	crontab csh.cshrc csh.login csh.logout \
	dhclient.conf dm.conf fbtab ftpusers gettytab group \
	hosts hosts.allow host.conf hosts.equiv hosts.lpd \
	inetd.conf login.access login.conf \
	motd modems networks newsyslog.conf \
	pam.conf phones pim6dd.conf pim6sd.conf \
	printcap profile protocols \
	rc rc.atm rc.devfs rc.diskless1 rc.diskless2 rc.firewall rc.isdn \
	rc.network rc.network6 rc.pccard rc.serial rc.shutdown rc.sysctl \
	remote rpc security services shells syslog.conf usbd.conf \
	etc.${MACHINE_ARCH}/disktab \
	etc.${MACHINE_ARCH}/rc.${MACHINE_ARCH} \
	etc.${MACHINE_ARCH}/ttys \
	${.CURDIR}/../gnu/usr.bin/man/manpath/manpath.config \
	${.CURDIR}/../usr.bin/mail/misc/mail.rc \
	${.CURDIR}/../usr.bin/locate/locate/locate.rc

.if exists(${.CURDIR}/../crypto) && !defined(NO_OPENSSH)
SSH=	${.CURDIR}/../crypto/openssh/ssh_config \
	${.CURDIR}/../crypto/openssh/sshd_config
.endif

.if exists(${.CURDIR}/../crypto) && !defined(NO_OPENSSL)
SSL=	${.CURDIR}/../crypto/openssl/apps/openssl.cnf
.endif

# -rwxr-xr-x root.wheel, for the new cron root.wheel
BIN2=	netstart pccard_ether rc.suspend rc.resume

MTREE=	BSD.include.dist BSD.local.dist BSD.root.dist BSD.usr.dist \
	BSD.var.dist BSD.x11.dist
NAMEDB= PROTO.localhost.rev named.conf named.root make-localhost
PPPCNF= ppp.deny ppp.shells.sample
PPPCF2= ppp.conf
ETCMAIL=Makefile README mailer.conf access.sample virtusertable.sample \
	mailertable.sample aliases


# Special top level files for FreeBSD
COPYRIGHT=	COPYRIGHT
FREEBSD=
FREEBSD+=	${COPYRIGHT}

etc:

distribute:
	cd ${.CURDIR} ; ${MAKE} distribution DESTDIR=${DISTDIR}/bin
.if defined(OBJFORMAT)
	echo OBJFORMAT=${OBJFORMAT} > ${DISTDIR}/bin/etc/objformat
.endif
	@echo 

distribution:
	(cd ${.CURDIR}; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 ${BIN1} ${DESTDIR}/etc; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 ${BIN2} ${DESTDIR}/etc; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 defaults/rc.conf ${DESTDIR}/etc/defaults/; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 defaults/make.conf ${DESTDIR}/etc/defaults/; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 defaults/pccard.conf ${DESTDIR}/etc/defaults/; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 defaults/periodic.conf ${DESTDIR}/etc/defaults/; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/log/cron; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 600 \
	    master.passwd ${DESTDIR}/etc; \
	( cd ${.CURDIR}/periodic; ${MAKE} install ); \
	( cd ${.CURDIR}/../gnu/usr.bin/send-pr; ${MAKE} etc-gnats-freefall ); \
	( cd ${.CURDIR}/../gnu/libexec/uucp/sample; ${MAKE} install ); \
	( cd ${.CURDIR}/../share/termcap; ${MAKE} etc-termcap ); \
	( cd ${.CURDIR}/../usr.sbin/rmt; ${MAKE} etc-rmt ); \
	( cd ${.CURDIR}/sendmail; ${MAKE} etc-sendmail.cf ); \
	( cd ${.CURDIR}/isdn; ${MAKE} install ); \
	pwd_mkdb -p -d ${DESTDIR}/etc ${DESTDIR}/etc/master.passwd; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	     MAKEDEV.local MAKEDEV ${DESTDIR}/dev )
.if exists(${.CURDIR}/../crypto) && !defined(NO_OPENSSH)
	(cd ${.CURDIR}; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 ${SSH} \
	    ${DESTDIR}/etc/ssh )
.endif
.if exists(${.CURDIR}/../crypto) && !defined(NO_OPENSSL)
	(cd ${.CURDIR}; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 ${SSL} \
	    ${DESTDIR}/etc/ssl )
.endif
.if !defined(NO_MAKEDEV)
	(cd ${DESTDIR}/dev; sh MAKEDEV all)
.endif
	(cd ${.CURDIR}/root; \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 dot.cshrc \
		    ${DESTDIR}/root/.cshrc; \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 dot.klogin \
		    ${DESTDIR}/root/.klogin; \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 dot.login \
		    ${DESTDIR}/root/.login; \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 dot.profile \
		    ${DESTDIR}/root/.profile; \
		rm -f ${DESTDIR}/.cshrc ${DESTDIR}/.profile; \
		ln ${DESTDIR}/root/.cshrc ${DESTDIR}/.cshrc; \
		ln ${DESTDIR}/root/.profile ${DESTDIR}/.profile)
	cd ${.CURDIR}/mtree; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${MTREE} ${DESTDIR}/etc/mtree
	cd ${.CURDIR}/namedb; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${NAMEDB} ${DESTDIR}/etc/namedb
	cd ${.CURDIR}/ppp; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${PPPCNF} ${DESTDIR}/etc/ppp
	cd ${.CURDIR}/ppp; ${INSTALL} -c -o root -g ${BINGRP} -m 600 \
	    ${PPPCF2} ${DESTDIR}/etc/ppp
	cd ${.CURDIR}/mail; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${ETCMAIL} ${DESTDIR}/etc/mail
	${INSTALL} -c -o ${BINOWN} -g operator -m 664 /dev/null \
	    ${DESTDIR}/etc/dumpdates
	${INSTALL} -c -o nobody -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/db/locate.database
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/lpd-errs
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/maillog
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/lastlog
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/messages
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/log/security
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/log/slip.log
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/log/ppp.log
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/wtmp
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/run/utmp
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 ${.CURDIR}/minfree \
	    ${DESTDIR}/var/crash
	(cd ${.CURDIR}/..; \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FREEBSD} \
		${DESTDIR}/)
.if !defined(NOMAN)
	(cd ${.CURDIR}/../share/man; ${MAKE} makedb; )
.endif

distrib-dirs:
	mtree -deU -f ${.CURDIR}/mtree/BSD.root.dist -p ${DESTDIR}/
	mtree -deU -f ${.CURDIR}/mtree/BSD.var.dist -p ${DESTDIR}/var
	mtree -deU -f ${.CURDIR}/mtree/BSD.usr.dist -p ${DESTDIR}/usr
	mtree -deU -f ${.CURDIR}/mtree/BSD.include.dist \
		-p ${DESTDIR}/usr/include
	mtree -deU -f ${.CURDIR}/mtree/BSD.include.dist \
		-p ${DESTDIR}/usr/libdata/perl/5.6.0/mach
	cd ${DESTDIR}/; rm -f ${DESTDIR}/sys; ln -s usr/src/sys sys
	cd ${DESTDIR}/usr/share/locale; \
	set - `cat ${.CURDIR}/locale.alias`; \
	while [ $$# -gt 0 ] ; \
	do \
		rm -rf "$$1"; \
		ln -s "$$2" "$$1"; \
		shift; shift; \
	done
	cd ${DESTDIR}/usr/share/nls; \
	set - `cat ${.CURDIR}/locale.alias`; \
	while [ $$# -gt 0 ] ; \
	do \
		rm -rf "$$1"; \
		ln -s "$$2" "$$1"; \
		shift; shift; \
	done; \
	rm -rf POSIX; \
	ln -s C POSIX

etc-examples:
	(cd ${.CURDIR}; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${BIN1} ${BIN2} \
	    ${DESTDIR}/usr/share/examples/etc; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 defaults/rc.conf \
	    ${DESTDIR}/usr/share/examples/etc/defaults; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 defaults/make.conf \
	    ${DESTDIR}/usr/share/examples/etc/defaults; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 defaults/pccard.conf \
	    ${DESTDIR}/usr/share/examples/etc/defaults; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 defaults/periodic.conf \
	    ${DESTDIR}/usr/share/examples/etc/defaults)

.include <bsd.prog.mk>
