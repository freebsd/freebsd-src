#	@(#)Makefile	5.11 (Berkeley) 5/21/91

LOCALTIME=	US/Pacific
TZDIR=/usr/share/zoneinfo

NOOBJ=	oobj

# disktab may be wrong -- hcx9 is a tahoe, but gets its own.
# -rw-r--r--
BINOWN= root
BINGRP= wheel
BIN1=	aliases csh.cshrc csh.login csh.logout dm.conf \
	ftpusers gettytab group hosts host.conf hosts.equiv hosts.lpd \
	inetd.conf motd myname netstart phones \
	printcap protocols rc rc.local remote security services shells \
	syslog.conf ttys etc.${MACHINE}/disktab rpc

# -rw-rw-rw-
BIN2=	motd

# -rwxr-xr-x root.wheel, for the new f***ing cron root.wheel
BIN3=	daily weekly monthly

MTREE=	BSD.root.dist BSD.usr.dist BSD.var.dist
NAMEDB=	localhost.rev named.boot root.cache
PCS=	pcs750.bin
WCS1=	wcs fppwcs poc poc1 poc2 fppoc
WCS2=	fpevent fppwcs fppwcs_dual hdcwcs load_diags start_fpp wcs wcs_dual

# Special top level files for FreeBSD
FREEBSD= CONTRIB.386BSD CONTRIB.FreeBSD COPYRIGHT

all clean cleandir depend etc install lint:

distribution: distrib-dirs
	install -c -o ${BINOWN} -g ${BINGRP} -m 644 ${BIN1} ${DESTDIR}/etc
	install -c -o ${BINOWN} -g ${BINGRP} -m 666 ${BIN2} ${DESTDIR}/etc
	install -c -o root -g wheel -m 755 ${BIN3} ${DESTDIR}/etc
	install -c -o root -g wheel -m 600 crontab ${DESTDIR}/var/cron/tabs/root
	install -c -o root -g wheel -m 600 /dev/null ${DESTDIR}/var/cron/log
	install -c -o root -g wheel -m 600 master.passwd ${DESTDIR}/etc
	#
	# This is broken, it always does /etc. It IGNORES the ${DESTDIR} for 
	# where to write the spwd.db pwd.db files.  
	#(cd ${DESTDIR}/etc; pwd_mkdb -p master.passwd)
	#
	# Work around for above problem.
	install -c -o root -g wheel -m 644 pwd.db ${DESTDIR}/etc
	install -c -o root -g wheel -m 600 spwd.db ${DESTDIR}/etc
	install -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	     MAKEDEV.local etc.${MACHINE}/MAKEDEV ${DESTDIR}/dev
	(cd ${DESTDIR}/dev; sh MAKEDEV all)
	(cd root; \
		install -c -o root -g wheel -m 644 dot.cshrc \
		    ${DESTDIR}/root/.cshrc; \
		install -c -o root -g wheel -m 644 dot.klogin \
		    ${DESTDIR}/root/.klogin; \
		install -c -o root -g wheel -m 644 dot.login \
		    ${DESTDIR}/root/.login; \
		install -c -o root -g wheel -m 644 dot.profile \
		    ${DESTDIR}/root/.profile; \
		rm -f ${DESTDIR}/.cshrc ${DESTDIR}/.profile; \
		ln ${DESTDIR}/root/.cshrc ${DESTDIR}/.cshrc; \
		ln ${DESTDIR}/root/.profile ${DESTDIR}/.profile)
	cd mtree; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${MTREE} \
	    ${DESTDIR}/etc/mtree
	cd namedb; install -c -o ${BINOWN} -g ${BINGRP} -m 644 ${NAMEDB} \
	    ${DESTDIR}/etc/namedb
	/bin/rm -f ${DESTDIR}/etc/localtime
	ln -s ${TZDIR}/${LOCALTIME} ${DESTDIR}/etc/localtime
	install -c -o ${BINOWN} -g operator -m 664 /dev/null \
	    ${DESTDIR}/etc/dumpdates
	install -c -o nobody -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/db/locate.database
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/log/lpd-errs
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/log/maillog
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/log/messages
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/log/wtmp
	install -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/run/utmp
	(cd etc.${MACHINE}; install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    fstab.* ${DESTDIR}/etc)
.if ${MACHINE} == "tahoe"
	(cd etc.tahoe; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${WCS1} \
	    ${DESTDIR}/)
.endif
.if ${MACHINE} == "vax"
	(cd etc.vax; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${PCS} \
	    ${DESTDIR}/)
.endif
.if ${MACHINE} == "i386"
	(cd ../sys/i386/boot; make depend all install cleandir; \
	 cd /usr/mdec; find . | cpio -pdalmuv ${DESTDIR}/usr/mdec)
	(cd ../include; make copies)
	(cd ../usr.sbin/sendmail/src; make install; \
	 cd ../cf/cf; make tcpproto.cf; \
	    install -o root -g wheel -m 644 tcpproto.cf \
	        ${DESTDIR}/etc/sendmail.cf)
	(cd ../; install -c -o root -g wheel -m 444 ${FREEBSD} ${DESTDIR}/)
	(cd ..; make install)
.endif

hcx9-distribution:
	(cd etc.tahoe; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${WCS2} \
	    ${DESTDIR}/)

distrib-dirs:
	mtree -u -f mtree/BSD.root.dist -p ${DESTDIR}/
	mtree -u -f mtree/BSD.var.dist -p ${DESTDIR}/var
	mtree -u -f mtree/BSD.usr.dist -p ${DESTDIR}/usr
	(cd ${DESTDIR}; rm -f ${DESTDIR}/sys; \
	    ln -s usr/src/sys sys )

.include <bsd.prog.mk>
