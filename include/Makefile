#	@(#)Makefile	8.2 (Berkeley) 1/4/94
# $FreeBSD$
#
# Doing a "make install" builds /usr/include.

CLEANFILES= osreldate.h version vers.c
SUBDIR= arpa protocols rpcsvc rpc
INCS=	a.out.h ar.h assert.h bitstring.h complex.h cpio.h _ctype.h ctype.h \
	db.h \
	dirent.h dlfcn.h elf.h elf-hints.h err.h fmtmsg.h fnmatch.h fstab.h \
	fts.h ftw.h getopt.h glob.h grp.h \
	hesiod.h histedit.h ieeefp.h ifaddrs.h \
	inttypes.h iso646.h kenv.h langinfo.h libgen.h limits.h link.h \
	locale.h malloc.h memory.h monetary.h mpool.h ndbm.h netconfig.h \
	netdb.h nl_types.h nlist.h nss.h nsswitch.h objformat.h paths.h \
	proc_service.h pthread.h \
	pthread_np.h pwd.h ranlib.h readpassphrase.h regex.h regexp.h \
	resolv.h rune.h runetype.h search.h setjmp.h sgtty.h signal.h stab.h \
	stdbool.h stddef.h stdio.h stdlib.h string.h stringlist.h \
	strings.h sysexits.h tar.h tgmath.h \
	time.h timeconv.h timers.h ttyent.h \
	ulimit.h unistd.h utime.h utmp.h uuid.h varargs.h vis.h wchar.h \
	wctype.h wordexp.h

MHDRS=	float.h floatingpoint.h stdarg.h

# posix4/mqueue.h is useless without an implementation and isn't installed:
PHDRS=	sched.h semaphore.h _semaphore.h # mqueue.h

LHDRS=	aio.h errno.h fcntl.h linker_set.h poll.h stdint.h syslog.h \
	termios.h ucontext.h

LDIRS=	cam geom net net80211 netatalk netatm netgraph netinet netinet6 \
	netipsec netipx netkey netnatm netncp netsmb nfs nfsclient nfsserver \
	pccard posix4 sys vm

LSUBDIRS=	cam/scsi dev/an dev/bktr dev/firewire dev/ic dev/iicbus \
	dev/ofw dev/ppbus dev/smbus dev/usb dev/wi dev/utopia fs/devfs \
	fs/fdescfs fs/fifofs fs/msdosfs fs/ntfs fs/nullfs fs/nwfs fs/portalfs \
	fs/procfs fs/smbfs fs/udf fs/umapfs fs/unionfs \
	geom/concat geom/gate geom/label geom/mirror geom/nop geom/raid3 \
	geom/shsec geom/stripe \
	isofs/cd9660 netatm/ipatm netatm/sigpvc netatm/spans netatm/uni \
	netgraph/atm netgraph/netflow \
	security/mac_biba security/mac_bsdextended \
	security/mac_lomac security/mac_mls security/mac_partition \
	ufs/ffs ufs/ufs

.if !defined(NO_BLUETOOTH)
LSUBSUBDIRS=	netgraph/bluetooth/include
.endif

# Define SHARED to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies'').  ``symlinks'' is
# probably only useful for developers and should be avoided if you do not
# wish to tie your /usr/include and /usr/src together.
#SHARED=	symlinks
SHARED?=	copies

INCS+=	osreldate.h

osreldate.h:	${.CURDIR}/../sys/conf/newvers.sh \
		${.CURDIR}/../sys/sys/param.h \
		${.CURDIR}/Makefile
	@${ECHO} creating osreldate.h from newvers.sh
	@setvar PARAMFILE ${.CURDIR}/../sys/sys/param.h; \
	MAKE=${MAKE} . ${.CURDIR}/../sys/conf/newvers.sh;	\
	echo "$$COPYRIGHT" > osreldate.h;			\
	echo "#ifdef _KERNEL" >> osreldate.h;			\
	echo '#error "<osreldate.h> cannot be used in the kernel, use <sys/param.h>"' >> osreldate.h; \
	echo "#else" >> osreldate.h;				\
	echo \#'undef __FreeBSD_version' >> osreldate.h;	\
	echo \#'define __FreeBSD_version' $$RELDATE >> osreldate.h; \
	echo "#endif" >> osreldate.h

.for i in ${LHDRS}
INCSLINKS+=	sys/$i ${INCLUDEDIR}/$i
.endfor
.for i in ${MHDRS}
INCSLINKS+=	machine/$i ${INCLUDEDIR}/$i
.endfor
.for i in ${PHDRS}
INCSLINKS+=	posix4/$i ${INCLUDEDIR}/$i
.endfor

.include <bsd.prog.mk>

installincludes: ${SHARED}
${SHARED}: compat

# Take care of stale directory-level symlinks.
compat:
.for i in ${LDIRS} ${LSUBDIRS} machine crypto
	if [ -L ${DESTDIR}${INCLUDEDIR}/$i ]; then \
		rm -f ${DESTDIR}${INCLUDEDIR}/$i; \
	fi
.endfor
	mtree -deU ${MTREE_FOLLOWS_SYMLINKS} \
	    -f ${.CURDIR}/../etc/mtree/BSD.include.dist \
	    -p ${DESTDIR}${INCLUDEDIR}
.if defined(WITH_BIND_LIBS) && !defined(NO_BIND)
	mtree -deU ${MTREE_FOLLOWS_SYMLINKS} \
	    -f ${.CURDIR}/../etc/mtree/BIND.include.dist \
	    -p ${DESTDIR}${INCLUDEDIR}
.endif

copies:
.for i in ${LDIRS} ${LSUBDIRS} ${LSUBSUBDIRS} altq crypto machine machine/pc
.if exists(${DESTDIR}${INCLUDEDIR}/$i)
	cd ${DESTDIR}${INCLUDEDIR}/$i; \
	for h in *.h; do \
		if [ -L $$h ]; then rm -f $$h; fi; \
	done
.endif
.endfor
.for i in ${LDIRS} ${LSUBDIRS:Ndev/acpica:Ndev/bktr} ${LSUBSUBDIRS}
	cd ${.CURDIR}/../sys; \
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 $i/*.h \
	    ${DESTDIR}${INCLUDEDIR}/$i
.endfor
	cd ${.CURDIR}/../sys/dev/acpica; \
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 acpiio.h \
	    ${DESTDIR}${INCLUDEDIR}/dev/acpica
	cd ${.CURDIR}/../sys/dev/bktr; \
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ioctl_*.h \
	    ${DESTDIR}${INCLUDEDIR}/dev/bktr
	cd ${.CURDIR}/../sys/contrib/altq/altq; \
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
	    ${DESTDIR}${INCLUDEDIR}/altq
	cd ${.CURDIR}/../sys/contrib/ipfilter/netinet; \
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
	    ${DESTDIR}${INCLUDEDIR}/netinet
	cd ${.CURDIR}/../sys/contrib/pf/net; \
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
	    ${DESTDIR}${INCLUDEDIR}/net
	cd ${.CURDIR}/../sys/opencrypto; \
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
	    ${DESTDIR}${INCLUDEDIR}/crypto
	cd ${.CURDIR}/../sys/${MACHINE_ARCH}/include; \
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
	    ${DESTDIR}${INCLUDEDIR}/machine
.if exists(${.CURDIR}/../sys/${MACHINE_ARCH}/include/pc)
	cd ${.CURDIR}/../sys/${MACHINE_ARCH}/include/pc; \
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
	    ${DESTDIR}${INCLUDEDIR}/machine/pc
.endif

symlinks:
	@${ECHO} "Setting up symlinks to kernel source tree..."
.for i in ${LDIRS}
	cd ${.CURDIR}/../sys/$i; \
	for h in *.h; do \
		ln -fs ../../../sys/$i/$$h ${DESTDIR}${INCLUDEDIR}/$i; \
	done
.endfor
.for i in ${LSUBDIRS:Ndev/acpica:Ndev/bktr}
	cd ${.CURDIR}/../sys/$i; \
	for h in *.h; do \
		ln -fs ../../../../sys/$i/$$h ${DESTDIR}${INCLUDEDIR}/$i; \
	done
.endfor
	cd ${.CURDIR}/../sys/dev/acpica; \
	for h in acpiio.h; do \
		ln -fs ../../../../sys/dev/acpica/$$h \
		    ${DESTDIR}${INCLUDEDIR}/dev/acpica; \
	done
	cd ${.CURDIR}/../sys/dev/bktr; \
	for h in ioctl_*.h; do \
		ln -fs ../../../../sys/dev/bktr/$$h \
		    ${DESTDIR}${INCLUDEDIR}/dev/bktr; \
	done
.for i in ${LSUBSUBDIRS}
	cd ${.CURDIR}/../sys/$i; \
	for h in *.h; do \
		ln -fs ../../../../../sys/$i/$$h ${DESTDIR}${INCLUDEDIR}/$i; \
	done
.endfor
	cd ${.CURDIR}/../sys/contrib/altq/altq; \
	for h in *.h; do \
		ln -fs ../../../sys/contrib/altq/altq/$$h \
		    ${DESTDIR}${INCLUDEDIR}/altq; \
	done
	cd ${.CURDIR}/../sys/contrib/ipfilter/netinet; \
	for h in *.h; do \
		ln -fs ../../../sys/contrib/ipfilter/netinet/$$h \
		    ${DESTDIR}${INCLUDEDIR}/netinet; \
	done
	cd ${.CURDIR}/../sys/contrib/pf/net; \
	for h in *.h; do \
		ln -fs ../../../sys/contrib/pf/net/$$h \
		    ${DESTDIR}${INCLUDEDIR}/net; \
	done
	cd ${.CURDIR}/../sys/opencrypto; \
	for h in *.h; do \
		ln -fs ../../../sys/opencrypto/$$h \
		    ${DESTDIR}${INCLUDEDIR}/crypto; \
	done
	cd ${.CURDIR}/../sys/${MACHINE_ARCH}/include; \
	for h in *.h; do \
		ln -fs ../../../sys/${MACHINE_ARCH}/include/$$h \
		    ${DESTDIR}${INCLUDEDIR}/machine; \
	done
.if exists(${.CURDIR}/../sys/${MACHINE_ARCH}/include/pc)
	cd ${.CURDIR}/../sys/${MACHINE_ARCH}/include/pc; \
	for h in *.h; do \
		ln -fs ../../../../sys/${MACHINE_ARCH}/include/pc/$$h \
		    ${DESTDIR}${INCLUDEDIR}/machine/pc; \
	done
.endif
