#	from: @(#)Makefile	5.11 (Berkeley) 5/21/91
#	$Id: Makefile,v 1.117.4.9 1996/06/05 21:20:22 nate Exp $

# -rw-r--r--
BINOWN= root
BINGRP= wheel
BIN1=   aliases amd.map csh.cshrc csh.login csh.logout dm.conf \
	ftpusers gettytab group hosts host.conf hosts.equiv hosts.lpd \
	inetd.conf login.access motd modems netstart networks \
	pccard.conf.sample phones printcap profile protocols \
	rc rc.firewall rc.local rc.pccard rc.serial \
	etc.${MACHINE}/rc.${MACHINE} \
	remote security services sysconfig shells \
	syslog.conf ttys etc.${MACHINE}/disktab rpc make.conf \
	${.CURDIR}/../gnu/usr.bin/man/manpath/manpath.config \
	${.CURDIR}/../usr.bin/mail/misc/mail.rc

# -rw-rw-rw-
BIN2=	motd

# -rwxr-xr-x root.wheel, for the new cron root.wheel
BIN3=	daily weekly monthly pccard_ether

MTREE=	BSD.local.dist BSD.root.dist BSD.usr.dist BSD.var.dist
NAMEDB=	PROTO.localhost.rev named.boot named.root make-localhost
PPPCNF= ppp.conf.filter.sample ppp.conf.iij ppp.conf.sample \
	ppp.linkup.sample ppp.secret.sample

# Special top level files for FreeBSD
COPYRIGHT=	COPYRIGHT
FREEBSD=
FREEBSD+=	${COPYRIGHT}

# Compatibility stuff, remove those links
LATIN1LINKS = \
	da_DK de_AT de_CH de_DE en_AU en_CA en_GB en_US es_ES fi_FI \
	fr_BE fr_CA fr_CH fr_FR is_IS it_CH it_IT nl_BE nl_NL no_NO \
	pt_PT sv_SE

all depend etc install lint:

distribute:
	cd ${.CURDIR} ; ${MAKE} distribution DESTDIR=${DISTDIR}/bin

distribution:
	(cd ${.CURDIR}; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 ${BIN1} ${DESTDIR}/etc; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 666 ${BIN2} ${DESTDIR}/etc; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 ${BIN3} ${DESTDIR}/etc; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 crontab ${DESTDIR}/etc; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/cron/log; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 600 \
	    master.passwd ${DESTDIR}/etc; \
	pwd_mkdb -p -d ${DESTDIR}/etc ${DESTDIR}/etc/master.passwd; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	     MAKEDEV.local etc.${MACHINE}/MAKEDEV ${DESTDIR}/dev )
	(cd ${DESTDIR}/dev; sh MAKEDEV all) ;
	(cd ${.CURDIR}/root; \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 dot.fvwmrc \
		    ${DESTDIR}/root/.fvwmrc; \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 dot.Xdefaults \
		    ${DESTDIR}/root/.Xdefaults; \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 dot.xsession \
		    ${DESTDIR}/root/.xsession; \
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
	${INSTALL} -c -o ${BINOWN} -g operator -m 664 /dev/null \
	    ${DESTDIR}/etc/dumpdates
	${INSTALL} -c -o nobody -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/db/locate.database
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/log/lpd-errs
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/log/maillog
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/log/lastlog
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/log/messages
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/log/slip.log
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/log/wtmp
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/run/utmp
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 ${.CURDIR}/minfree \
	    ${DESTDIR}/var/crash
	(cd ${.CURDIR}/../usr.sbin/sendmail/src; \
	    ${MAKE} obj; \
	    ${MAKE} all; \
	    ${MAKE} install; \
	 cd ../cf/cf; \
	    ${MAKE} obj; \
	    ${MAKE} freebsd.cf; \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 obj/freebsd.cf \
		${DESTDIR}/etc/sendmail.cf)
	(cd ${.CURDIR}/..; \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FREEBSD} \
		${DESTDIR}/)
	(cd ${.CURDIR}/../share/man; ${MAKE} makedb; )

distrib-dirs:
	mtree -deU -f ${.CURDIR}/mtree/BSD.root.dist -p ${DESTDIR}/
	mtree -deU -f ${.CURDIR}/mtree/BSD.var.dist -p ${DESTDIR}/var
# Compatibility stuff, remove obsoleted links, if exists
	if [ -d	${DESTDIR}/usr/share/locale ] ;	\
	then \
		cd ${DESTDIR}/usr/share/locale;	\
		for l in ${LATIN1LINKS}	; do \
			if [ -h $$l.ISO_8859-1 ]; then \
				rm $$l.ISO_8859-1; \
			fi ; \
		done; \
	fi
	mtree -deU -f ${.CURDIR}/mtree/BSD.usr.dist -p ${DESTDIR}/usr
	mtree -deU -f ${.CURDIR}/mtree/BSD.include.dist \
		-p ${DESTDIR}/usr/include
.if defined(MAKE_LOCAL)
	mtree -deU -f ${.CURDIR}/mtree/BSD.local.dist -p ${DESTDIR}/usr/local
.endif
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
.if defined(MAKE_LOCAL)
	cd ${DESTDIR}/usr/local/share/nls; \
	set - `cat ${.CURDIR}/locale.alias`; \
	while [ $$# -gt 0 ] ; \
	do \
		rm -rf "$$1"; \
		ln -s "$$2" "$$1"; \
		shift; shift; \
	done; \
	rm -rf POSIX; \
	ln -s C POSIX
.endif

.include <bsd.prog.mk>
