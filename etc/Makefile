#	@(#)Makefile	5.11 (Berkeley) 5/21/91

NOOBJ=	noobj

# disktab may be wrong -- hcx9 is a tahoe, but gets its own.
# -rw-r--r--
BINOWN= root
BINGRP= wheel
BIN1=	aliases csh.cshrc csh.login csh.logout dm.conf \
	ftpusers gettytab group hosts host.conf hosts.equiv hosts.lpd \
	inetd.conf motd myname netstart networks phones \
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
FREEBSD= CONTRIB.386BSD CONTRIB.FreeBSD RELNOTES.FreeBSD ${COPYRIGHT}
#
# Floppy drive name and files for building FreeBSD Floppies
FLOPPY?=	fd0
MOUNT?=		/mnt
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
FILESYSTEM_DIRS=	bin dev etc mnt sbin usr usr/bin usr/mdec usr/sbin
FILESYSTEM_TREES=	dev
FILESYSTEM_FILES=	${COPYRIGHT}
FILESYSTEM_FILES+=	bin/[ bin/expr bin/ls bin/mkdir bin/rm
FILESYSTEM_FILES+=	bin/sh bin/sync bin/test
FILESYSTEM_FILES+=	dev/MAKEDEV
FILESYSTEM_FILES+=	etc/group
FILESYSTEM_FILES+=	etc/master.passwd etc/passwd etc/pwd.db
FILESYSTEM_FILES+=	sbin/disklabel sbin/halt sbin/init
FILESYSTEM_FILES+=	sbin/mount sbin/mount_pcfs
FILESYSTEM_FILES+=	sbin/newfs
FILESYSTEM_FILES+=	sbin/umount
FILESYSTEM_FILES+=	sbin/fdisk
FILESYSTEM_FILES+=	usr/bin/cpio
FILESYSTEM_FILES+=	${MDEC}
FILESYSTEM_FILES+=	usr/sbin/bad144

CPIO_FILES=	${COPYRIGHT}
CPIO_FILES+=	usr/bin/gunzip usr/bin/gzcat usr/bin/gzip usr/bin/zcat
CPIO_CPIO=	bin/chmod bin/cat bin/cp bin/dd bin/df bin/mv bin/pwd bin/stty
CPIO_CPIO+=	etc/protocols etc/services etc/spwd.db
CPIO_CPIO+=	sbin/ifconfig sbin/fsck sbin/mknod sbin/mount_isofs
CPIO_CPIO+=	sbin/reboot sbin/route sbin/slattach
CPIO_CPIO+=	tmp
CPIO_CPIO+=	usr/bin/awk usr/bin/chgrp usr/bin/ftp
CPIO_CPIO+=	usr/bin/more usr/bin/tar usr/bin/tip
CPIO_CPIO+=	usr/bin/elvis usr/bin/ex usr/bin/vi usr/bin/view
CPIO_CPIO+=	usr/sbin/update usr/sbin/chown
CPIO_CPIO_DIRS=	var var/tmp

CRYPT_LIB=	lib/libcrypt
CRYPT_SRCS=	bin/ed bin/rcp
CRYPT_SRCS+=	libexec/ftpd libexec/makekey libexec/rexecd libexec/rlogind
CRYPT_SRCS+=	libexec/rshd libexec/telnetd libexec/uucpd
CRYPT_SRCS+=	usr.bin/bdes usr.bin/lock usr.bin/login usr.bin/passwd
CRYPT_SRCS+=	usr.bin/rlogin usr.bin/rsh usr.bin/su usr.bin/telnet
CRYPT_DIRS=	bin usr usr/bin usr/lib usr/libexec 

all clean cleandir depend etc install lint:

crypt:
	rm -f ${LIBCRYPT};
	(cd ${.CURDIR}/../${CRYPT_LIB}; \
		${MAKE} cleandir obj depend all install)
	for i in ${CRYPT_SRCS}; do \
		cd ${.CURDIR}/../$$i; \
		${MAKE} cleandir obj depend all; \
	done
	
non-crypt:
	rm -f ${LIBCRYPT}
	for i in ${CRYPT_SRCS}; do \
		cd ${.CURDIR}/../$$i; \
		${MAKE} cleandir obj depend all; \
	done
	
distribution: distrib-dirs
	install -c -o ${BINOWN} -g ${BINGRP} -m 644 ${BIN1} ${DESTDIR}/etc
	install -c -o ${BINOWN} -g ${BINGRP} -m 666 ${BIN2} ${DESTDIR}/etc
	install -c -o root -g wheel -m 755 ${BIN3} ${DESTDIR}/etc
	install -c -o root -g wheel -m 600 crontab ${DESTDIR}/var/cron/tabs/root
	install -c -o root -g wheel -m 600 /dev/null ${DESTDIR}/var/cron/log
	install -c -o root -g wheel -m 600 master.passwd ${DESTDIR}/etc
	pwd_mkdb -p -d ${DESTDIR}/etc ${DESTDIR}/etc/master.passwd
	install -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	     MAKEDEV.local etc.${MACHINE}/MAKEDEV ${DESTDIR}/dev
.if defined(CDROMDIST)
	(cd ${DESTDIR}/dev; sh MAKEDEV all)
.endif
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
.if defined(NOCRYPT)
	${MAKE} non-crypt
	(cd ..; NOCRYPT=nocrypt; export NOCRYPT; ${MAKE} install)
.else
	${MAKE} crypt
	(cd ..; ${MAKE} install)
.endif
	(cd ../usr.sbin/sendmail/src; \
	    ${MAKE} install; \
	 cd ../cf/cf; \
	    ${MAKE} tcpproto.cf; \
	    install -o root -g wheel -m 644 tcpproto.cf \
	        ${DESTDIR}/etc/sendmail.cf)
	(cd ../; \
	    install -c -o root -g wheel -m 444 ${FREEBSD} ${DESTDIR}/)
	(cd ..; ${MAKE} mdec; )
.if ${MACHINE} == "tahoe"
	(cd etc.tahoe; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${WCS1} \
	    ${DESTDIR}/)
.endif
.if ${MACHINE} == "vax"
	(cd etc.vax; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${PCS} \
	    ${DESTDIR}/)
.endif

hcx9-distribution:
	(cd etc.tahoe; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${WCS2} \
	    ${DESTDIR}/)

kcopy-kernels: ../sys/i386/conf/GENERICAH ../sys/i386/conf/GENERICBT
	(cd ../sys/compile; rm -rf GENERICAH GENERICBT)
	(cd ../sys/i386/conf; config GENERICAH; config GENERICBT)
	(cd ../sys/compile/GENERICAH; ${MAKE} depend; ${MAKE} all; \
	    install -c -o root -g wheel -m 755 386bsd \
	        ${DESTDIR}/386bsd.GENERICAH)
	(cd ../sys/compile/GENERICBT; ${MAKE} depend; ${MAKE} all; \
	    install -c -o root -g wheel -m 755 386bsd \
	        ${DESTDIR}/386bsd.GENERICBT)

kcopy-floppy:
	disklabel -w -r ${FLOPPY} floppy5 /usr/mdec/fdboot /usr/mdec/bootfd
	newfs -b 4096 -c 80 -f 512 -i 16384 -m 0 -o space r${FLOPPY}a floppy5
	mount /dev/${FLOPPY}a ${MOUNT}
	chown root.wheel ${MOUNT}/.
	chmod 755 ${MOUNT}/.
	(cd ${DESTDIR}/; \
	    ls -d ${KC_DIRS} | cpio -pdamuv ${MOUNT})
	(cd ${MOUNT}/dev; \
	    sh ${DESTDIR}/dev/MAKEDEV std; \
	    rm -rf fd; \
	    sh ${DESTDIR}/dev/MAKEDEV fd0 wd0 sd0 sd1)
	(cd ${DESTDIR}/; \
	    ls ${KC_FILES} | cpio -pdamuv ${MOUNT})
	install -c -o root -g wheel -m 755 etc.i386/kc.profile \
	    ${MOUNT}/.profile

kcopy-ah-floppy:
	${MAKE} kcopy-floppy
	(cd ../sys/compile/GENERICAH; \
	    install -c -o root -g wheel -m 755 386bsd ${MOUNT}/)
	df -ik ${MOUNT}
	umount /dev/${FLOPPY}a
	fsck /dev/r${FLOPPY}a
	dd if=/dev/r${FLOPPY}a of=${RELEASEDIR}/floppies/kcopy-ah-floppy \
		bs=15b count=160
	gzip --no-name -9 -c ${RELEASEDIR}/floppies/kcopy-ah-floppy \
		>${RELEASEDIR}/floppies/kcopy-ah-floppy.gz

kcopy-bt-floppy:
	${MAKE} kcopy-floppy
	(cd ../sys/compile/GENERICBT; \
	    install -c -o root -g wheel -m 755 386bsd ${MOUNT}/)
	df -ik ${MOUNT}
	umount /dev/${FLOPPY}a
	fsck /dev/r${FLOPPY}a
	dd if=/dev/r${FLOPPY}a of=${RELEASEDIR}/floppies/kcopy-bt-floppy \
		bs=15b count=160
	gzip --no-name -9 -c ${RELEASEDIR}/floppies/kcopy-bt-floppy \
		>${RELEASEDIR}/floppies/kcopy-bt-floppy.gz

filesystem-floppy:
	disklabel -w -r ${FLOPPY} floppy5 /usr/mdec/fdboot /usr/mdec/bootfd
	newfs -b 4096 -c 80 -f 512 -i 10240 -m 0 -o space r${FLOPPY}a floppy5
	mount /dev/${FLOPPY}a ${MOUNT}
	chown root.wheel ${MOUNT}/.
	chmod 755 ${MOUNT}/.
	(cd ${DESTDIR}/; \
	    ls -d ${FILESYSTEM_DIRS} | cpio -pdamuv ${MOUNT})
	(cd ${MOUNT}/dev; \
	    sh ${DESTDIR}/dev/MAKEDEV std; \
	    rm -rf fd; \
	    sh ${DESTDIR}/dev/MAKEDEV fd0 fd1 wd0 sd0 sd1; \
	    rm -f rfd1[b-z] fd1[b-z])
	(cd ${DESTDIR}/; \
	    ls ${FILESYSTEM_FILES} | cpio -pdamuv ${MOUNT})
	install -c -o root -g wheel -m 755 etc.i386/inst1.profile \
	    ${MOUNT}/.profile
	install -c -o root -g wheel -m 755 etc.i386/inst1.install \
	    ${MOUNT}/install
	(cd ${MOUNT}/; \
	    ls ${FILESYSTEM_FILES} >/tmp/filelist; \
	    ls -d ${FILESYSTEM_DIRS} >>/tmp/filelist; \
	    find ${FILESYSTEM_TREES} >>/tmp/filelist; \
	    sort -u /tmp/filelist >filelist; \
	    rm /tmp/filelist)
	df -ik ${MOUNT}
	umount /dev/${FLOPPY}a
	fsck /dev/r${FLOPPY}a
	dd if=/dev/r${FLOPPY}a of=${RELEASEDIR}/floppies/filesystem-floppy \
		bs=15b count=160
	gzip --no-name -9 -c ${RELEASEDIR}/floppies/filesystem-floppy \
		>${RELEASEDIR}/floppies/filesystem-floppy.gz

cpio-floppy:
	disklabel -w -r ${FLOPPY} floppy5 /usr/mdec/fdboot /usr/mdec/bootfd
	newfs -b 4096 -c 80 -f 512 -i 65536 -m 0 -o space r${FLOPPY}a floppy5
	mount /dev/${FLOPPY}a ${MOUNT}
	chown root.wheel ${MOUNT}/.
	chmod 755 ${MOUNT}/.
	(cd ${DESTDIR}/; \
		ls ${CPIO_FILES} | cpio -pdamuv ${MOUNT})
	(cd ${DESTDIR}/; \
	    (find ${CPIO_CPIO}; ls -d ${CPIO_CPIO_DIRS}) | \
		cpio -oav | gzip -9 >${MOUNT}/inst2.cpio.gz)
	install -c -o root -g wheel -m 755 etc.i386/inst2.profile \
	    ${MOUNT}/.profile
	install -c -o root -g wheel -m 755 etc.i386/inst2.install \
	    ${MOUNT}/install
	df -ik ${MOUNT}
	umount /dev/${FLOPPY}a
	fsck /dev/r${FLOPPY}a
	dd if=/dev/r${FLOPPY}a of=${RELEASEDIR}/floppies/cpio-floppy \
		bs=15b count=160
	gzip --no-name -9 -c ${RELEASEDIR}/floppies/cpio-floppy \
		>${RELEASEDIR}/floppies/cpio-floppy.gz

bin-tarball:
	(cd ${DESTDIR}; \
		tar cf - . | gzip --no-name -9 -c | \
			split -b 240640 - \
			${RELEASEDIR}/tarballs/bin_tgz.)

src-tarball:
	(cd ${DESTDIR}; \
		tar --exclude usr/src/${CRYPT_LIB} -cf - usr/src | gzip --no-name -9 -c | \
			split -b 240640 - \
			${RELEASEDIR}/tarballs/src_tgz.)

ssrc-tarball:
	(cd ${DESTDIR}; \
		tar -cf - usr/src/${CRYPT_LIB} | gzip --no-name -9 -c | \
			split -b 240640 - \
			${RELEASEDIR}/tarballs/ssrc_tgz.)

des-tarball:
	rm -rf ${RELEASEDIR}/tmpdes
	mkdir ${RELEASEDIR}/tmpdes
	for i in ${CRYPT_DIRS}; do \
		cd ${RELEASEDIR}/tmpdes; \
		mkdir $$i; \
		chown ${BINOWN}.${GRPOWN} $$i; \
		chmod 755 $$i; \
	done
	# This is ugly, it force installs a /usr/lib/libcrypt.a so
	# that the other makes will be built with des.
	#
	(cd ${.CURDIR}/../${CRYPT_LIB}; \
		unset NOCRYPT; \
		DESTDIR=; export DESTDIR; \
		${MAKE} cleandir obj depend all install; \
		NOMAN=noman; export NOMAN; \
		DESTDIR=${RELEASEDIR}/tmpdes; export DESTDIR; \
		${MAKE} cleandir obj depend all install)
	for i in ${CRYPT_SRCS}; do \
		unset NOCRYPT; \
		DESTDIR=${RELEASEDIR}/tmpdes; export DESTDIR; \
		NOMAN=noman; export NOMAN; \
		cd ${.CURDIR}/../$$i; \
		${MAKE} cleandir obj depend all install; \
	done
	(cd ${RELEASEDIR}/tmpdes; \
		tar cf - . | gzip --no-name -9 -c | \
			split -b 240640 - \
			${RELEASEDIR}/tarballs/des_tgz.)
	rm -rf ${RELEASEDIR}/tmpdes

distrib-dirs:
	mtree -u -f mtree/BSD.root.dist -p ${DESTDIR}/
	mtree -u -f mtree/BSD.var.dist -p ${DESTDIR}/var
	mtree -u -f mtree/BSD.usr.dist -p ${DESTDIR}/usr
.if defined(CDROMDIST)
	mtree -u -f mtree/BSD.local.dist -p ${DESTDIR}/usr/local
.endif
	cd ${DESTDIR}/; rm -f ${DESTDIR}/sys; ln -s usr/src/sys sys

floppies: kcopy-ah-floppy kcopy-bt-floppy filesystem-floppy \
	  cpio-floppy

release: release-dirs distribution kcopy-kernels floppies \
	 bin-tarball des-tarball
	
release-dirs:
	rm -rf ${RELEASEDIR}/filesystem
	mkdir ${RELEASEDIR}/filesystem
	chown root.wheel ${RELEASEDIR}/filesystem
	chmod 755 ${RELEASEDIR}/filesystem
	rm -rf ${RELEASEDIR}/tarballs
	mkdir ${RELEASEDIR}/tarballs
	chown root.wheel ${RELEASEDIR}/tarballs
	chmod 755 ${RELEASEDIR}/tarballs
	rm -rf ${RELEASEDIR}/floppies
	mkdir ${RELEASEDIR}/floppies
	chown root.wheel ${RELEASEDIR}/floppies
	chmod 755 ${RELEASEDIR}/floppies

.include <bsd.prog.mk>
