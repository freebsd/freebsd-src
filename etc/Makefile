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
COPYRIGHT= COPYRIGHT
FREEBSD= CONTRIB.386BSD CONTRIB.FreeBSD ${COPYRIGHT}
#
# Floppy drive name and files for building FreeBSD Floppies
FLOPPY?=	fd0
MOUNT=		/mnt
#
MDEC=		usr/mdec/bootfd usr/mdec/fdboot
MDEC+=		usr/mdec/bootsd usr/mdec/sdboot
MDEC+=		usr/mdec/bootwd usr/mdec/wdboot
#
KC_DIRS=	dev mnt
KC_FILES=	${COPYRIGHT}
KC_FILES+=	bin/[ bin/cp bin/echo bin/sh bin/test
KC_FILES+=	sbin/fsck sbin/halt sbin/init sbin/mount sbin/umount
#
INST1_DIRS=	dev mnt
INST1_FILES=	${COPYRIGHT}
INST1_FILES+=	bin/[ bin/cat bin/df bin/expr bin/ls bin/mkdir
INST1_FILES+=	bin/sh bin/sync bin/test
INST1_FILES+=	dev/MAKEDEV dev/MAKEDEV.local
INST1_FILES+=	etc/disktab etc/group etc/master.passwd etc/passwd
INST1_FILES+=	etc/pwd.db etc/spwd.db
INST1_FILES+=	sbin/disklabel sbin/halt sbin/init sbin/mount sbin/umount
INST1_FILES+=	sbin/newfs sbin/reboot
INST1_FILES+=	usr/bin/cpio
INST1_FILES+=	${MDEC}
INST1_FILES+=	usr/sbin/bad144

INST2_FILES=	${COPYRIGHT}
INST2_FILES+=	usr/bin/gunzip usr/bin/gzcat usr/bin/gzip usr/bin/zcat
INST2_CPIO=	bin/chmod bin/cp bin/dd bin/mv bin/pwd bin/rm bin/stty
INST2_CPIO+=	etc/protocols etc/services
INST2_CPIO+=	sbin/ifconfig sbin/fsck sbin/mknod
#INST2_CPIO+=	sbin/mount_pcfs sbin/mount_isofs
INST2_CPIO+=	sbin/route sbin/shutdown sbin/slattach
INST2_CPIO+=	tmp
INST2_CPIO+=	usr/bin/awk usr/bin/chgrp usr/bin/ftp
INST2_CPIO+=	usr/bin/more usr/bin/tar usr/bin/tip usr/bin/zcat
INST2_CPIO+=	usr/bin/elvis usr/bin/ex usr/bin/vi usr/bin/view
#INST2_CPIO+=	usr/local/bin/mread usr/local/bin/rz
INST2_CPIO+=	usr/sbin/update usr/sbin/chown
INST2_CPIO+=	var

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
	install -c -o root -g wheel -m 644 passwd ${DESTDIR}/etc
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
	    ${DESTDIR}/var/log/lastlog
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
	(cd ../sys/i386/boot; \
	    make depend all install cleandir)
	(cd /; \
	    ls ${MDEC} | cpio -pdalmuv ${DESTDIR}/)
	(cd ../usr.sbin/sendmail/src; \
	    make install; \
	 cd ../cf/cf; \
	    make tcpproto.cf; \
	    install -o root -g wheel -m 644 tcpproto.cf \
	        ${DESTDIR}/etc/sendmail.cf)
	(cd ../; \
	    install -c -o root -g wheel -m 444 ${FREEBSD} ${DESTDIR}/)
	(cd ..; make install)
	# the above make install clobers the /etc/localtime link
	/bin/rm -f ${DESTDIR}/etc/localtime
	ln -s ${TZDIR}/${LOCALTIME} ${DESTDIR}/etc/localtime
	/bin/rm -f ${DESTDIR}/etc/termcap
	ln -s /usr/share/misc/termcap ${DESTDIR}/etc/termcap
	(cd ../sys/compile; rm -rf GENERICISA)
	(cd ../sys/i386/conf; config GENERICISA)
	(cd ../sys/compile/GENERICISA; make depend; make all; \
	    install -c -o root -g wheel -m 755 386bsd \
	        ${DESTDIR}/386bsd.GENERICISA)
	(cd ../include; make copies)
.endif

hcx9-distribution:
	(cd etc.tahoe; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${WCS2} \
	    ${DESTDIR}/)

kc-ah-floppy:
	disklabel -w -r ${FLOPPY} floppy5 /usr/mdec/fdboot /usr/mdec/bootfd
	newfs -b 4096 -c 80 -f 512 -i 16384 -m 0 -o space r${FLOPPY}a floppy5
	mount /dev/${FLOPPY}a ${MOUNT}
	(cd ${DESTDIR}/; \
	    ls -d ${KC_DIRS} | cpio -pdalmuv ${MOUNT})
	(cd ${MOUNT}/dev; \
	    sh ${DESTDIR}/dev/MAKEDEV std; \
	    rm -rf fd; \
	    sh ${DESTDIR}/dev/MAKEDEV fd0 wd0 sd0)
	(cd ${DESTDIR}/; \
	    ls ${KC_FILES} | cpio -pdalmuv ${MOUNT})
	install -c -o root -g wheel -m 755 etc.i386/kc.profile \
	    ${MOUNT}/.profile
	(cd ../sys/compile/GENERICISA; \
	    install -c -o root -g wheel -m 755 386bsd ${MOUNT}/)
	df -ik ${MOUNT}
	umount /dev/${FLOPPY}a
	fsck /dev/r${FLOPPY}a

inst1-floppy:
	disklabel -w -r ${FLOPPY} floppy5 /usr/mdec/fdboot /usr/mdec/bootfd
	newfs -b 4096 -c 80 -f 512 -i 10240 -m 0 -o space r${FLOPPY}a floppy5
	mount /dev/${FLOPPY}a ${MOUNT}
	(cd ${DESTDIR}/; \
	    ls -d ${INST1_DIRS} | cpio -pdalmuv ${MOUNT})
	(cd ${MOUNT}/dev; \
	    sh ${DESTDIR}/dev/MAKEDEV std; \
	    rm -rf fd; \
	    sh ${DESTDIR}/dev/MAKEDEV fd0 wd0 sd0)
	(cd ${DESTDIR}/; \
	    ls ${INST1_FILES} | cpio -pdalmuv ${MOUNT})
	install -c -o root -g wheel -m 755 etc.i386/inst1.profile \
	    ${MOUNT}/.profile
	install -c -o root -g wheel -m 755 etc.i386/inst1.install \
	    ${MOUNT}/install
	(cd ${MOUNT}/; \
	    ls ${INST1_FILES} >/tmp/filelist; \
	    find ${INST1_DIRS} | sort >>/tmp/filelist; \
	    sort -u /tmp/filelist >filelist)
	df -ik ${MOUNT}
	umount /dev/${FLOPPY}a
	fsck /dev/r${FLOPPY}a

inst2-floppy:
	disklabel -w -r ${FLOPPY} floppy5 /usr/mdec/fdboot /usr/mdec/bootfd
	newfs -b 4096 -c 80 -f 512 -i 65536 -m 0 -o space r${FLOPPY}a floppy5
	mount /dev/${FLOPPY}a ${MOUNT}
	(cd ${DESTDIR}/; \
	    ls ${INST2_FILES} | cpio -pdalmuv ${MOUNT})
	(cd ${DESTDIR}/; \
	    find ${INST2_CPIO} | cpio -odalmuv | gzip -9 >${MOUNT}/inst2.cpio.gz)
	install -c -o root -g wheel -m 755 etc.i386/inst2.profile \
	    ${MOUNT}/.profile
	install -c -o root -g wheel -m 755 etc.i386/inst2.install \
	    ${MOUNT}/install
	df -ik ${MOUNT}
	umount /dev/${FLOPPY}a
	fsck /dev/r${FLOPPY}a

distrib-dirs:
	mtree -u -f mtree/BSD.root.dist -p ${DESTDIR}/
	mtree -u -f mtree/BSD.var.dist -p ${DESTDIR}/var
	mtree -u -f mtree/BSD.usr.dist -p ${DESTDIR}/usr
	(cd ${DESTDIR}; rm -f ${DESTDIR}/sys; \
	    ln -s usr/src/sys sys )

.include <bsd.prog.mk>
