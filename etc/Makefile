#	from: @(#)Makefile	5.11 (Berkeley) 5/21/91
# $FreeBSD$

.if !defined(NO_SENDMAIL)
SUBDIR=	sendmail
.endif

BIN1=   amd.map apmd.conf auth.conf \
	crontab csh.cshrc csh.login csh.logout \
	dhclient.conf dm.conf fbtab ftpusers gettytab group \
	hosts hosts.allow host.conf hosts.equiv hosts.lpd \
	inetd.conf login.access login.conf \
	motd modems networks newsyslog.conf \
	pam.conf phones printcap profile protocols \
	rc rc.atm rc.diskless1 rc.diskless2 rc.firewall rc.firewall6 \
	rc.network rc.network6 rc.pccard rc.sendmail rc.serial rc.shutdown \
	rc.syscons rc.sysctl remote rpc services shells sysctl.conf \
	syslog.conf usbd.conf \
	etc.${MACHINE_ARCH}/disktab \
	etc.${MACHINE_ARCH}/rc.${MACHINE_ARCH} \
	etc.${MACHINE_ARCH}/ttys \
	${.CURDIR}/../gnu/usr.bin/man/manpath/manpath.config \
	${.CURDIR}/../usr.bin/mail/misc/mail.rc \
	${.CURDIR}/../usr.bin/locate/locate/locate.rc

.if !defined(NO_I4B)
BIN1+=	rc.isdn
.endif

.if exists(${.CURDIR}/../crypto) && !defined(NO_OPENSSH)
SSH=	${.CURDIR}/../crypto/openssh/ssh_config \
	${.CURDIR}/../crypto/openssh/sshd_config \
	moduli
.endif

.if exists(${.CURDIR}/../crypto) && !defined(NO_OPENSSL)
SSL=	${.CURDIR}/../crypto/openssl/apps/openssl.cnf
.endif

# -rwxr-xr-x root:wheel, for the new cron root:wheel
BIN2=	netstart pccard_ether rc.suspend rc.resume

DEFAULTS= rc.conf make.conf pccard.conf periodic.conf

MTREE=	BSD.include.dist BSD.local.dist BSD.root.dist BSD.usr.dist \
	BSD.var.dist BSD.x11.dist BSD.x11-4.dist
.if !defined(NO_SENDMAIL)
MTREE+=	BSD.sendmail.dist
.endif
NAMEDB= PROTO.localhost.rev PROTO.localhost-v6.rev named.conf named.root \
	make-localhost
PPPCNF= ppp.conf
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
	cd ${.CURDIR}; \
	    ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 \
		${BIN1} ${DESTDIR}/etc; \
	    ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 755 \
		${BIN2} ${DESTDIR}/etc; \
	    ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 600 \
		master.passwd nsmb.conf opieaccess ${DESTDIR}/etc; \
	cd ${.CURDIR}/defaults; ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${DEFAULTS} ${DESTDIR}/etc/defaults;
	cd ${.CURDIR}/periodic; ${MAKE} install
	cd ${.CURDIR}/../gnu/usr.bin/send-pr; ${MAKE} etc-gnats-freefall
	cd ${.CURDIR}/../share/termcap; ${MAKE} etc-termcap
	cd ${.CURDIR}/../usr.sbin/rmt; ${MAKE} etc-rmt
	pwd_mkdb -p -d ${DESTDIR}/etc ${DESTDIR}/etc/master.passwd
.if !defined(NO_I4B)
	( cd ${.CURDIR}/isdn; ${MAKE} install )
.endif
.if !defined(NO_SENDMAIL)
	( cd ${.CURDIR}/sendmail; ${MAKE} distribution );
.endif
.if !defined(NOUUCP)
	( cd ${.CURDIR}/../gnu/libexec/uucp/sample; ${MAKE} install )
.endif
.if exists(${.CURDIR}/../crypto) && !defined(NO_OPENSSH)
	(cd ${.CURDIR}; ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 ${SSH} \
	    ${DESTDIR}/etc/ssh )
.endif
.if exists(${.CURDIR}/../crypto) && !defined(NO_OPENSSL)
	(cd ${.CURDIR}; ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 ${SSL} \
	    ${DESTDIR}/etc/ssl )
.endif
.if !defined(NO_MAKEDEV)
	( cd ${.CURDIR}; ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 555 \
	    MAKEDEV.local MAKEDEV ${DESTDIR}/dev )
.if !defined(NO_MAKEDEV_RUN)
	(cd ${DESTDIR}/dev; sh MAKEDEV all)
.endif
.endif
	(cd ${.CURDIR}/root; \
		${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 dot.cshrc \
		    ${DESTDIR}/root/.cshrc; \
		${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 dot.klogin \
		    ${DESTDIR}/root/.klogin; \
		${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 dot.login \
		    ${DESTDIR}/root/.login; \
		${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 dot.profile \
		    ${DESTDIR}/root/.profile; \
		rm -f ${DESTDIR}/.cshrc ${DESTDIR}/.profile; \
		ln ${DESTDIR}/root/.cshrc ${DESTDIR}/.cshrc; \
		ln ${DESTDIR}/root/.profile ${DESTDIR}/.profile)
	cd ${.CURDIR}/mtree; ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${MTREE} ${DESTDIR}/etc/mtree
	cd ${.CURDIR}/namedb; ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${NAMEDB} ${DESTDIR}/etc/namedb
	cd ${.CURDIR}/ppp; ${INSTALL} ${COPY} -o root -g ${BINGRP} -m 600 \
	    ${PPPCNF} ${DESTDIR}/etc/ppp
	cd ${.CURDIR}/mail; ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${ETCMAIL} ${DESTDIR}/etc/mail
	@if [ -d ${DESTDIR}/etc/mail -a -f ${DESTDIR}/etc/mail/aliases -a \
	    ! -f ${DESTDIR}/etc/aliases ]; then \
		set -x; \
		ln -s mail/aliases ${DESTDIR}/etc/aliases; \
	fi
	${INSTALL} ${COPY} -o ${BINOWN} -g operator -m 664 /dev/null \
	    ${DESTDIR}/etc/dumpdates
	${INSTALL} ${COPY} -o nobody -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/db/locate.database
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/log/auth.log
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/log/cron
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/lpd-errs
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 640 /dev/null \
	    ${DESTDIR}/var/log/maillog
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/lastlog
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/messages
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/log/security
	${INSTALL} ${COPY} -o ${BINOWN} -g network -m 640 /dev/null \
	    ${DESTDIR}/var/log/slip.log
	${INSTALL} ${COPY} -o ${BINOWN} -g network -m 640 /dev/null \
	    ${DESTDIR}/var/log/ppp.log
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/wtmp
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/run/utmp
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 ${.CURDIR}/minfree \
	    ${DESTDIR}/var/crash
	(cd ${.CURDIR}/..; \
	    ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${FREEBSD} \
		${DESTDIR}/)
.if !defined(NOMAN)
	(cd ${.CURDIR}/../share/man; ${MAKE} makedb; )
.endif

distrib-dirs:
	-set - `grep "^[a-zA-Z]" ${.CURDIR}/locale.deprecated`; \
	while [ $$# -gt 0 ] ; \
	do \
		for dir in /usr/share/locale \
			   /usr/share/nls \
			   /usr/local/share/nls; \
		do \
			test -d ${DESTDIR}/$${dir} && cd ${DESTDIR}/$${dir}; \
			test -L "$$2" && rm -rf "$$2"; \
			test \! -L "$$1" && test -d "$$1" && mv "$$1" "$$2"; \
		done; \
		shift; shift; \
	done
	mtree -deU -f ${.CURDIR}/mtree/BSD.root.dist -p ${DESTDIR}/
	mtree -deU -f ${.CURDIR}/mtree/BSD.var.dist -p ${DESTDIR}/var
	mtree -deU -f ${.CURDIR}/mtree/BSD.usr.dist -p ${DESTDIR}/usr
	mtree -deU -f ${.CURDIR}/mtree/BSD.include.dist \
		-p ${DESTDIR}/usr/include
	mtree -deU -f ${.CURDIR}/mtree/BSD.include.dist \
		-p ${DESTDIR}/usr/libdata/perl/5.00503/mach
.if !defined(NO_SENDMAIL)
	mtree -deU -f ${.CURDIR}/mtree/BSD.sendmail.dist -p ${DESTDIR}/
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
	cd ${DESTDIR}/usr/share/locale; \
	set - `grep "^[a-zA-Z]" ${.CURDIR}/locale.alias`; \
	while [ $$# -gt 0 ] ; \
	do \
		rm -rf "$$1"; \
		ln -s "$$2" "$$1"; \
		shift; shift; \
	done
	cd ${DESTDIR}/usr/share/nls; \
	set - `grep "^[a-zA-Z]" ${.CURDIR}/nls.alias`; \
	while [ $$# -gt 0 ] ; \
	do \
		rm -rf "$$1"; \
		ln -s "$$2" "$$1"; \
		shift; shift; \
	done

etc-examples:
	cd ${.CURDIR}; ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${BIN1} ${BIN2} nsmb.conf opieaccess \
	    ${DESTDIR}/usr/share/examples/etc
	cd ${.CURDIR}/defaults; ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${DEFAULTS} ${DESTDIR}/usr/share/examples/etc/defaults

.include <bsd.prog.mk>
