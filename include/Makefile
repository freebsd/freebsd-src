#	@(#)Makefile	8.2 (Berkeley) 1/4/94
# $FreeBSD$
#
# Doing a make install builds /usr/include
#
# The ``rm -rf''s used below are safe because rm doesn't follow symbolic
# links.

CLEANFILES= osreldate.h version vers.c
SUBDIR= rpcsvc rpc
FILES=	a.out.h ar.h assert.h bitstring.h complex.h ctype.h db.h \
	dirent.h disktab.h \
	dlfcn.h elf.h elf-hints.h err.h fnmatch.h fstab.h \
	fts.h glob.h grp.h strhash.h \
	hesiod.h histedit.h ieeefp.h ifaddrs.h iso646.h langinfo.h \
	libgen.h limits.h link.h locale.h malloc.h memory.h mpool.h \
	netconfig.h ndbm.h netdb.h nl_types.h nlist.h nsswitch.h objformat.h \
	paths.h pthread.h pthread_np.h pwd.h \
	ranlib.h regex.h regexp.h resolv.h rune.h runetype.h \
	search.h setjmp.h sgtty.h \
	signal.h stab.h stdbool.h stddef.h stdio.h stdlib.h string.h \
	stringlist.h strings.h sysexits.h tar.h time.h timers.h \
	ttyent.h unistd.h utime.h utmp.h vis.h wchar.h wctype.h
.if defined(WANT_CSRG_LIBM)
FILES+=	math.h
.endif

ARPAFILES=	ftp.h inet.h nameser.h nameser_compat.h telnet.h tftp.h

PROTOFILES=	dumprestore.h routed.h rwhod.h talkd.h timed.h

MFILES=	float.h floatingpoint.h stdarg.h varargs.h

# posix4/aio.h conflicts with dysons and isn't installed:
PFILES=	mqueue.h sched.h semaphore.h # aio.h

# Only for default SHARED=copies case
SFILES=	soundcard.h joystick.h

LFILES=	aio.h errno.h fcntl.h inttypes.h linker_set.h poll.h syslog.h \
	termios.h ucontext.h

LDIRS=	cam net netatalk netatm netgraph netinet netinet6 \
	netipx netkey netnatm netncp netns netsmb nfs pccard posix4 \
	sys vm

LNOHEADERDIRS=	fs isofs ufs dev

LSUBDIRS=	cam/scsi dev/ic dev/ppbus dev/usb dev/wi \
	fs/fdescfs fs/fifofs fs/msdosfs fs/ntfs fs/nullfs fs/nwfs fs/portalfs \
	fs/procfs fs/smbfs fs/umapfs fs/unionfs isofs/cd9660 \
	ufs/ffs ufs/ufs

# For SHARED=symlinks, cam is a symlink, so cam/scsi is taken care of
LSYMSUBDIRS=	${LSUBDIRS:Ncam/scsi}

# Define SHARED to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies'').  ``symlinks'' is
# probably only useful for developers and should be avoided if you do not
# wish to tie your /usr/include and /usr/src together.
#SHARED=	symlinks
SHARED?=	copies

all:	osreldate.h

osreldate.h:	${.CURDIR}/../sys/conf/newvers.sh \
		${.CURDIR}/../sys/sys/param.h
	@${ECHO} creating osreldate.h from newvers.sh
	setvar PARAMFILE ${.CURDIR}/../sys/sys/param.h; \
	. ${.CURDIR}/../sys/conf/newvers.sh;			\
	echo "$$COPYRIGHT" > osreldate.h;			\
	echo "#ifdef _KERNEL" >> osreldate.h;			\
	echo '#error "osreldate.h must not be used in the kernel, use sys/param.h"' >> osreldate.h; \
	echo "#else" >> osreldate.h;				\
	echo \#'undef __FreeBSD_version' >> osreldate.h;	\
	echo \#'define __FreeBSD_version' $$RELDATE >> osreldate.h; \
	echo "#endif" >> osreldate.h

beforeinstall: ${SHARED}
	@rm -f ${DESTDIR}/usr/include/timepps.h
	cd ${.CURDIR}; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${FILES} ${DESTDIR}/usr/include
	cd ${.CURDIR}/arpa; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${ARPAFILES} ${DESTDIR}/usr/include/arpa
	cd ${.CURDIR}/protocols; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${PROTOFILES} ${DESTDIR}/usr/include/protocols
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.OBJDIR}/osreldate.h \
		${DESTDIR}/usr/include
.for i in ${LFILES}
	ln -sf sys/$i ${DESTDIR}/usr/include/$i
.endfor
.for i in ${MFILES}
	ln -sf machine/$i ${DESTDIR}/usr/include/$i
.endfor
.for i in ${PFILES}
	ln -sf posix4/$i ${DESTDIR}/usr/include/$i
.endfor

copies:
.for i in ${LDIRS} ${LSYMSUBDIRS} machine
	if [ -h ${DESTDIR}/usr/include/$i ]; then \
		rm -f ${DESTDIR}/usr/include/$i; \
	fi
.endfor
	mtree -deU ${MTREE_FOLLOWS_SYMLINKS} -f ${.CURDIR}/../etc/mtree/BSD.include.dist \
		-p ${DESTDIR}/usr/include
.for i in ${LDIRS} ${LSUBDIRS}
	cd ${.CURDIR}/../sys; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 $i/*.h \
		${DESTDIR}/usr/include/$i
.endfor
.if exists(${.CURDIR}/../sys/contrib/ipfilter/netinet)
	cd ${.CURDIR}/../sys/contrib/ipfilter/netinet; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
		${DESTDIR}/usr/include/netinet
.endif
.if exists(${.CURDIR}/../sys/${MACHINE_ARCH}/include)
	cd ${.CURDIR}/../sys/${MACHINE_ARCH}/include; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
		${DESTDIR}/usr/include/machine
.endif
.for i in ${SFILES}
	ln -sf ../sys/$i ${DESTDIR}/usr/include/machine/$i
.endfor

symlinks:
	@${ECHO} "Setting up symlinks to kernel source tree..."
.for i in ${LDIRS}
	rm -rf ${DESTDIR}/usr/include/$i
	ln -s ../../sys/$i ${DESTDIR}/usr/include/$i
.endfor
.for i in ${LNOHEADERDIRS}
	rm -rf ${DESTDIR}/usr/include/$i
	mkdir ${DESTDIR}/usr/include/$i
.endfor
.for i in ${LSYMSUBDIRS}
	ln -s ../../../sys/$i ${DESTDIR}/usr/include/$i
.endfor
	rm -rf ${DESTDIR}/usr/include/machine
	ln -s ../../sys/${MACHINE_ARCH}/include ${DESTDIR}/usr/include/machine

.include <bsd.prog.mk>
