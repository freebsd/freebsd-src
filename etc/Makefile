#	from: @(#)Makefile	5.11 (Berkeley) 5/21/91
#	$Id: Makefile,v 1.162 1997/10/20 00:35:17 jmb Exp $

# -rw-r--r--
BINOWN= root
BINGRP= wheel
BIN1=   aliases amd.map crontab csh.cshrc csh.login csh.logout dm.conf \
	ftpusers gettytab group hosts host.conf hosts.equiv hosts.lpd \
	inetd.conf login.conf login.access motd modems networks \
	newsyslog.conf phones pccard.conf.sample printcap profile protocols \
	rc rc.conf rc.firewall rc.local rc.network rc.pccard rc.serial \
	rc.shutdown etc.${MACHINE}/rc.${MACHINE} \
	remote security services shells \
	syslog.conf ttys etc.${MACHINE}/disktab rpc make.conf \
	${.CURDIR}/../gnu/usr.bin/man/manpath/manpath.config \
	${.CURDIR}/../usr.bin/mail/misc/mail.rc \
	${.CURDIR}/../usr.bin/locate/locate/locate.rc

# -rw-rw-rw-
BIN2=	motd

# -rwxr-xr-x root.wheel, for the new cron root.wheel
BIN3=	netstart pccard_ether

MTREE=	BSD.include.dist BSD.local.dist BSD.root.dist BSD.usr.dist \
	BSD.var.dist BSD.x11.dist
NAMEDB=	PROTO.localhost.rev named.boot named.root make-localhost
PPPCNF= ppp.conf.sample ppp.linkup.sample ppp.linkdown.sample \
	ppp.secret.sample ppp.deny ppp.shells.sample
NOSPAM=	Makefile README sendmail.cf.additions


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
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/cron/log; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 600 \
	    master.passwd ${DESTDIR}/etc; \
	( cd ${.CURDIR}/periodic; ${MAKE} install );\
	( cd ${.CURDIR}/../gnu/usr.bin/send-pr; ${MAKE} etc-gnats-freefall );\
	( cd ${.CURDIR}/../gnu/libexec/uucp/sample; ${MAKE} install ); \
	( cd ${.CURDIR}/../share/termcap; ${MAKE} etc-termcap ); \
	( cd ${.CURDIR}/../usr.sbin/rmt; ${MAKE} etc-rmt ); \
	( cd ${.CURDIR}/../usr.sbin/sendmail/cf/cf; ${MAKE} etc-sendmail.cf ); \
	pwd_mkdb -p -d ${DESTDIR}/etc ${DESTDIR}/etc/master.passwd; \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	     MAKEDEV.local etc.${MACHINE}/MAKEDEV ${DESTDIR}/dev )
	(cd ${DESTDIR}/dev; sh MAKEDEV all) ;
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
	cd ${.CURDIR}/mail; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${NOSPAM} ${DESTDIR}/etc/mail
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
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 600 /dev/null \
	    ${DESTDIR}/var/log/ppp.log
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/log/wtmp
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/run/utmp
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 ${.CURDIR}/minfree \
	    ${DESTDIR}/var/crash
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

.include <bsd.prog.mk>
