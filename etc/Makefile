#	from: @(#)Makefile	5.11 (Berkeley) 5/21/91
#	$Id: Makefile,v 1.125 1995/10/28 02:01:34 ache Exp $

# disktab may be wrong -- hcx9 is a tahoe, but gets its own.
# -rw-r--r--
BINOWN= root
BINGRP= wheel
BIN1=   aliases csh.cshrc csh.login csh.logout dm.conf \
	ftpusers gettytab group hosts host.conf hosts.equiv hosts.lpd \
	inetd.conf login.access motd modems netstart \
	networks phones pccard.conf printcap profile protocols \
	rc rc.local rc.serial etc.${MACHINE}/rc.${MACHINE} \
	remote security services sysconfig shells \
	syslog.conf ttys etc.${MACHINE}/disktab rpc make.conf \
	${.CURDIR}/../gnu/usr.bin/man/manpath/manpath.config \
	${.CURDIR}/../usr.bin/mail/misc/mail.rc

# -rw-rw-rw-
BIN2=	motd

# -rwxr-xr-x root.wheel, for the new cron root.wheel
BIN3=	daily weekly monthly

CLEANFILES+=	*.c *.o *.lo
CLEANFILES+=	filesystem filesystem.cache filesystem.mk
CLEANFILES+=	kcopy kcopy.cache kcopy.mk

MAKEDEVS=	(cd ${MOUNT}/dev; \
		    sh ${DESTDIR}/dev/MAKEDEV std; \
		    rm -rf fd; \
		    sh ${DESTDIR}/dev/MAKEDEV fd0 fd1; \
		    rm -rf fd0?* rfd0?* fd1?* rfd1?*; \
		    sh ${DESTDIR}/dev/MAKEDEV wd0 wd1 sd0 sd1 cd0 mcd0 mcd1; \
		    rm -rf *wd[01][ijklm] rmcd*)

NEWFS=		newfs -b 4096 -c 80 -f 512 -m 0 -o space -u 0 -t 0
ZIPNSPLIT=	gzip --no-name -9 -c | split -b 240640 -

MTREE=	BSD.local.dist BSD.root.dist BSD.usr.dist BSD.var.dist
NAMEDB=	PROTO.localhost.rev named.boot named.root make-localhost
PPPCNF= ppp.conf.filter.sample ppp.conf.iij ppp.conf.sample \
	ppp.linkup.sample ppp.secret.sample
PCS=	pcs750.bin
WCS1=	wcs fppwcs poc poc1 poc2 fppoc
WCS2=	fpevent fppwcs fppwcs_dual hdcwcs load_diags start_fpp wcs wcs_dual

# Special top level files for FreeBSD
COPYRIGHT=	COPYRIGHT
FREEBSD=
FREEBSD+=	${COPYRIGHT}
#
# Floppy drive name, mount point, type and parameters for building Floppies
FLOPPY?=	fd0
MOUNT?=		/mnt
FLOPPY_TYPE?=	fd1440
FLOPPY_BS?=	18b
FLOPPY_TRACKS?=	160
#
MDEC=		usr/mdec/bootfd usr/mdec/fdboot
MDEC+=		usr/mdec/bootsd usr/mdec/sdboot
MDEC+=		usr/mdec/bootwd usr/mdec/wdboot
#
KC_DIRS=	bin dev mnt sbin etc
KC_FILES=	${COPYRIGHT}
KC_LINKS=	bin/[ bin/cp bin/echo bin/sh bin/test
KC_LINKS+=	sbin/fsck sbin/halt sbin/init
KC_LINKS+=	sbin/mount sbin/mount_cd9660
KC_LINKS+=	sbin/umount
#
CD_DIRS=	etc usr
#
FILESYSTEM_DIRS=	bin dev etc mnt proc sbin usr usr/bin usr/mdec usr/sbin
FILESYSTEM_TREES=	dev
FILESYSTEM_FILES=	${COPYRIGHT}
FILESYSTEM_FILES+=	dev/MAKEDEV
FILESYSTEM_FILES+=	etc/group
FILESYSTEM_FILES+=	etc/master.passwd etc/passwd etc/pwd.db
FILESYSTEM_FILES+=	${MDEC}
FILESYSTEM_LINKS=	bin/[ bin/expr bin/ls bin/mkdir bin/rm
FILESYSTEM_LINKS+=	bin/sh bin/sync bin/test
FILESYSTEM_LINKS+=	sbin/disklabel sbin/fdisk sbin/halt sbin/init
FILESYSTEM_LINKS+=	sbin/mount
FILESYSTEM_LINKS+=	sbin/newfs
FILESYSTEM_LINKS+=	sbin/umount
FILESYSTEM_LINKS+=	usr/bin/cpio
FILESYSTEM_LINKS+=	usr/bin/gunzip usr/bin/gzcat usr/bin/gzip usr/bin/zcat
FILESYSTEM_LINKS+=	usr/sbin/bad144
FILESYSTEM_CPIO=	.profile
FILESYSTEM_CPIO+=	root/.profile
FILESYSTEM_CPIO+=	bin/[
FILESYSTEM_CPIO+=	bin/chmod bin/cat bin/cp bin/df bin/expr bin/ls
FILESYSTEM_CPIO+=	bin/ln bin/mkdir bin/mv bin/rm bin/sync
FILESYSTEM_CPIO+=	bin/test
FILESYSTEM_CPIO+=	etc/spwd.db
FILESYSTEM_CPIO+=	sbin/disklabel sbin/fdisk sbin/halt sbin/init
FILESYSTEM_CPIO+=	sbin/mount
FILESYSTEM_CPIO+=	sbin/newfs
FILESYSTEM_CPIO+=	sbin/umount
FILESYSTEM_CPIO_DIRS=	root

CPIO_FILES=	${COPYRIGHT}
CPIO_CPIO=	bin/dd bin/ps bin/pwd bin/stty
CPIO_CPIO+=	etc/protocols etc/remote etc/services
CPIO_CPIO+=	etc/termcap
CPIO_CPIO+=	sbin/dmesg
CPIO_CPIO+=	sbin/ifconfig sbin/fsck sbin/mknod sbin/mount_cd9660
CPIO_CPIO+=	sbin/mount_procfs
CPIO_CPIO+=	sbin/reboot sbin/route sbin/slattach
CPIO_CPIO+=	usr/bin/awk usr/bin/chgrp usr/bin/cpio usr/bin/ex usr/bin/ftp
CPIO_CPIO+=	usr/bin/gunzip usr/bin/gzcat usr/bin/gzip
CPIO_CPIO+=	usr/bin/more usr/bin/tar usr/bin/tip
CPIO_CPIO+=	usr/bin/vi usr/bin/view usr/bin/zcat
CPIO_CPIO+=	usr/lib/libc.so.*
CPIO_CPIO+=	usr/lib/libcurses.so.*
CPIO_CPIO+=	usr/lib/libgcc.so.*
CPIO_CPIO+=	usr/lib/libm.so.*
CPIO_CPIO+=	usr/lib/libtermcap.so.*
CPIO_CPIO+=	usr/lib/libutil.so.*
CPIO_CPIO+=	usr/libexec/ld.so
CPIO_CPIO+=	usr/sbin/bad144 usr/sbin/chown
CPIO_CPIO+=	usr/share/misc/termcap
CPIO_CPIO_DIRS=		tmp usr/lib usr/libexec usr/share usr/share/misc
CPIO_CPIO_DIRS+=	var var/tmp var/run var/spool var/spool/lock

SCRYPT_LIB=	lib/libcrypt
DESCRYPT_LIB=	secure/lib/libcrypt
CRYPT_SRCS=	bin/ed bin/rcp
CRYPT_SRCS+=	sbin/init
.if !defined(NOCRYPT)
CRYPT_SRCS+=	secure
.endif
CRYPT_DIRS=	bin sbin usr usr/bin usr/lib

# Compatibility stuff, remove those links
LATIN1LINKS = \
	da_DK de_AT de_CH de_DE en_AU en_CA en_GB en_US es_ES fi_FI \
	fr_BE fr_CA fr_CH fr_FR is_IS it_CH it_IT nl_BE nl_NL no_NO \
	pt_PT sv_SE

all depend etc install lint:

scrypt:
	rm -f ${LIBCRYPT};
	(cd ${.CURDIR}/../${SCRYPT_LIB}; \
		${MAKE} cleandir obj depend all install)
	for i in ${CRYPT_SRCS}; do \
		cd ${.CURDIR}/../$$i; \
		${MAKE} cleandir obj depend all; \
	done

descrypt:
	rm -f ${LIBCRYPT};
	(cd ${.CURDIR}/../${DESCRYPT_LIB}; \
		${MAKE} cleandir obj depend all install)
	for i in ${CRYPT_SRCS}; do \
		cd ${.CURDIR}/../$$i; \
		${MAKE} cleandir obj depend all; \
	done

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
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/log/wtmp
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/run/utmp
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 664 ${.CURDIR}/minfree \
	    ${DESTDIR}/var/crash
	(cd ${.CURDIR}/etc.${MACHINE}; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} \
	    -m 444 fstab.* ${DESTDIR}/etc)
	(cd ${.CURDIR}/../usr.sbin/sendmail/src; \
	    ${MAKE} obj; \
	    ${MAKE} all; \
	    ${MAKE} install; \
	 cd ../cf/cf; \
	    ${MAKE} obj; \
	    ${MAKE} freefall.cf; \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 obj/freefall.cf \
		${DESTDIR}/etc/sendmail.cf)
	(cd ${.CURDIR}/..; \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FREEBSD} \
		${DESTDIR}/)
	(cd ${.CURDIR}/../share/man; ${MAKE} makedb; )

crunch:
	crunchgen ${.CURDIR}/../usr.sbin/crunch/examples/kcopy.conf
	${MAKE} -f kcopy.mk objs exe
	crunchgen ${.CURDIR}/../usr.sbin/crunch/examples/filesystem.conf
	${MAKE} -f filesystem.mk objs exe

extract:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	    ${.CURDIR}/etc.i386/EXTRACT_bin.sh \
	    ${RELEASEDIR}/tarballs/bin/EXTRACT.sh
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	    ${.CURDIR}/etc.i386/EXTRACT_src.sh \
	    ${RELEASEDIR}/tarballs/bin/EXTRACT.sh
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	    ${.CURDIR}/etc.i386/EXTRACT_secr.sh \
	    ${RELEASEDIR}/tarballs/bin/EXTRACT.sh

hcx9-distribution:
	(cd ${.CURDIR}/etc.tahoe; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} \
	    -m 444 ${WCS2} ${DESTDIR}/)

kcopy-kernels: ${.CURDIR}/../sys/i386/conf/GENERIC
	(cd ${.CURDIR}/../sys/compile; rm -rf GENERIC)
	(cd ${.CURDIR}/../sys/i386/conf; config GENERIC)
	(cd ${.CURDIR}/../sys/compile/GENERIC; ${MAKE} depend; ${MAKE} all; \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 kernel \
	        ${DESTDIR}/kernel.GENERIC)

kcopy-floppy:
	echo y | fdformat ${FLOPPY}
	disklabel -w -r -B -b ${DESTDIR}/usr/mdec/fdboot \
		-s ${DESTDIR}/usr/mdec/bootfd ${FLOPPY} ${FLOPPY_TYPE}
	${NEWFS} -i 8192 r${FLOPPY} ${FLOPPY_TYPE}
	mount /dev/${FLOPPY} ${MOUNT}
	chown ${BINOWN}.${BINGRP} ${MOUNT}/.
	chmod 755 ${MOUNT}/.
	(cd ${DESTDIR}/; \
	    ls -d ${KC_DIRS} | cpio -pdamuv ${MOUNT})
	${MAKEDEVS}
	(cd ${DESTDIR}/; \
	    ls ${KC_FILES} | cpio -pdamuv ${MOUNT})
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${.CURDIR}/etc.i386/kc.profile ${MOUNT}/etc/rc
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 \
	    kcopy ${MOUNT}/bin/kcopy
	(cd ${MOUNT}/; \
	    for i in ${KC_LINKS}; do \
		ln bin/kcopy $$i; \
	    done)

kcopy.flp:
	(cd ${.CURDIR}; ${MAKE} kcopy-floppy)
	(cd ${.CURDIR}/../sys/compile/GENERIC; \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 kernel ${MOUNT}/)
	df -ik ${MOUNT}
	# XXX umount is returning 1 for some reason :-(
	-umount /dev/${FLOPPY}
	fsck /dev/r${FLOPPY}
	dd if=/dev/r${FLOPPY} of=${RELEASEDIR}/floppies/kcopy.flp \
		bs=${FLOPPY_BS} count=${FLOPPY_TRACKS}
	gzip --no-name -9 -c ${RELEASEDIR}/floppies/kcopy.flp \
		>${RELEASEDIR}/floppies/kcopy.flp.gz &

cdins-floppy:
	(cd ${.CURDIR}; ${MAKE} kcopy-floppy)
	(cd ${DESTDIR}/; \
	    ls -d ${CD_DIRS} | cpio -pdamuv ${MOUNT})
	(cd ${MOUNT}/usr; \
	    ln -s /cdrom/filesys/usr/libexec libexec; \
	    ln -s /cdrom/filesys/usr/lib lib)
	mkdir ${MOUNT}/cdrom
	chown ${BINOWN}.${BINGRP} ${MOUNT}/cdrom
	chmod 755 ${MOUNT}/cdrom
	#${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 \
	#    ${.CURDIR}/etc.i386/cdinst1.profile ${MOUNT}/.profile
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${.CURDIR}/etc.i386/cdinst1.install ${MOUNT}/install
	ln ${MOUNT}/install ${MOUNT}/etc/rc

cdins.flp:
	(cd ${.CURDIR}; ${MAKE} kcopy-floppy)
	(cd ${.CURDIR}/../sys/compile/GENERIC; \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 kernel ${MOUNT}/)
	df -ik ${MOUNT}
	# XXX umount is returning 1 for some reason :-(
	-umount /dev/${FLOPPY}
	fsck /dev/r${FLOPPY}
	dd if=/dev/r${FLOPPY} of=${RELEASEDIR}/floppies/cdins.flp \
		bs=${FLOPPY_BS} count=${FLOPPY_TRACKS}
	gzip --no-name -9 -c ${RELEASEDIR}/floppies/cdins.flp \
		>${RELEASEDIR}/floppies/cdins.flp.gz &

filesyst.flp:
	echo y | fdformat ${FLOPPY}
	disklabel -w -r -B -b ${DESTDIR}/usr/mdec/fdboot \
		-s ${DESTDIR}/usr/mdec/bootfd ${FLOPPY} ${FLOPPY_TYPE}
	${NEWFS} -i 10240 r${FLOPPY}  ${FLOPPY_TYPE}
	mount /dev/${FLOPPY}  ${MOUNT}
	chown ${BINOWN}.${BINGRP} ${MOUNT}/.
	chmod 755 ${MOUNT}/.
	(cd ${DESTDIR}/; \
	    ls -d ${FILESYSTEM_DIRS} | cpio -pdamuv ${MOUNT})
	${MAKEDEVS}
	(cd ${DESTDIR}/; \
	    ls ${FILESYSTEM_FILES} | cpio -pdamuv ${MOUNT}; \
	    (find ${FILESYSTEM_CPIO}; ls -d ${FILESYSTEM_CPIO_DIRS}) | \
		cpio -H newc --block-size=16 -oav | \
		gzip -9 >${MOUNT}/inst1.cpio.gz)
	#${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 \
	#    ${.CURDIR}/etc.i386/inst1.profile ${MOUNT}/.profile
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${.CURDIR}/etc.i386/inst1.install ${MOUNT}/install
	ln ${MOUNT}/install ${MOUNT}/etc/rc
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 \
	    filesystem ${MOUNT}/bin/filesystem
	(cd ${MOUNT}/; \
	    for i in ${FILESYSTEM_LINKS}; do \
		ln bin/filesystem $$i; \
	    done)
	(cd ${MOUNT}/; \
	    ls ${FILESYSTEM_FILES} >/tmp/filelist; \
	    ls ${FILESYSTEM_LINKS} >>/tmp/filelist; \
	    ls -d ${FILESYSTEM_DIRS} >>/tmp/filelist; \
	    find ${FILESYSTEM_TREES} >>/tmp/filelist; \
	    sort -u -r /tmp/filelist >filelist; \
	    rm /tmp/filelist)
	df -ik ${MOUNT}
	# XXX umount is returning 1 for some reason :-(
	-umount /dev/${FLOPPY}
	fsck /dev/r${FLOPPY}
	dd if=/dev/r${FLOPPY} of=${RELEASEDIR}/floppies/filesyst.flp \
		bs=${FLOPPY_BS} count=${FLOPPY_TRACKS}
	gzip --no-name -9 -c ${RELEASEDIR}/floppies/filesyst.flp \
		>${RELEASEDIR}/floppies/filesyst.flp.gz &

cpio.flp:
	echo y | fdformat ${FLOPPY}
	disklabel -w -r -B -b ${DESTDIR}/usr/mdec/fdboot \
		-s ${DESTDIR}/usr/mdec/bootfd ${FLOPPY} ${FLOPPY_TYPE}
	${NEWFS} -i 65536 r${FLOPPY}  ${FLOPPY_TYPE}
	mount /dev/${FLOPPY} ${MOUNT}
	chown ${BINOWN}.${BINGRP} ${MOUNT}/.
	chmod 755 ${MOUNT}/.
	(cd ${DESTDIR}/; \
		ls ${CPIO_FILES} | cpio -pdamuv ${MOUNT})
	#
	# XXX This ugliness is because the default termcap file is simply too
	# big and we don't need such a hugh one for the initial installation,
	# yet we want the symlink in /etc to point to the right place so we
	# need to install the smaller one in the same location.
	#
	mv ${DESTDIR}/usr/share/misc/termcap ${DESTDIR}/usr/share/misc/otermcap
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${.CURDIR}/termcap.small ${DESTDIR}/usr/share/misc/termcap
	(cd ${DESTDIR}/; \
	    (find ${CPIO_CPIO}; ls -d ${CPIO_CPIO_DIRS}) | \
		cpio -H newc --block-size=16 -oav | \
		gzip -9 >${MOUNT}/inst2.cpio.gz)
	# XXX cpio is done, put everything back in shape for the bindist.
	mv ${DESTDIR}/usr/share/misc/otermcap ${DESTDIR}/usr/share/misc/termcap
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${.CURDIR}/etc.i386/cpio.rc ${MOUNT}/rc
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${.CURDIR}/etc.i386/cpio.install ${MOUNT}/install
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${.CURDIR}/etc.i386/cpio.magic ${MOUNT}/magic
	df -ik ${MOUNT}
	# XXX umount is returning 1 for some reason :-(
	-umount /dev/${FLOPPY}
	fsck /dev/r${FLOPPY}
	dd if=/dev/r${FLOPPY} of=${RELEASEDIR}/floppies/cpio.flp \
		bs=${FLOPPY_BS} count=${FLOPPY_TRACKS}
	gzip --no-name -9 -c ${RELEASEDIR}/floppies/cpio.flp \
		>${RELEASEDIR}/floppies/cpio.flp.gz &

bin-tarball:
	(cd ${DESTDIR}; \
		mkdir -p ${RELEASEDIR}/tarballs/bin; \
		tar cf - . | \
		${ZIPNSPLIT} ${RELEASEDIR}/tarballs/bin/bin.)

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
	(set -x ; cd ${.CURDIR}/../${DESCRYPT_LIB}; \
		NOCRYPT=; \
		unset NOCRYPT; \
		DESTDIR=; export DESTDIR; \
		${MAKE} cleandir obj depend all install; \
		NOMAN=noman; export NOMAN; \
		DESTDIR=${RELEASEDIR}/tmpdes; export DESTDIR; \
		${MAKE} cleandir obj depend all install)
	for i in ${CRYPT_SRCS}; do \
		NOCRYPT=; \
		unset NOCRYPT; \
		DESTDIR=${RELEASEDIR}/tmpdes; export DESTDIR; \
		NOMAN=noman; export NOMAN; \
		cd ${.CURDIR}/../$$i; \
		${MAKE} cleandir obj depend all install; \
	done
	(cd ${RELEASEDIR}/tmpdes; \
		tar cf - . | \
			${ZIPNSPLIT} ${RELEASEDIR}/tarballs/des/des_tgz.)
	rm -rf ${RELEASEDIR}/tmpdes

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

floppies:	kcopy.flp filesyst.flp cpio.flp cdins.flp

release:	release-dirs distribution crunch extract kcopy-kernels \
		floppies bin-tarball des-tarball clean

release-dirs:
	chflags -R noschg ${RELEASEDIR}/
	rm -rf ${RELEASEDIR}/*
	mtree -d -U -f ${.CURDIR}/mtree/BSD.release.dist -p ${RELEASEDIR}

.include <bsd.prog.mk>
