#	From: @(#)Makefile	8.2 (Berkeley) 1/4/94
#	$Id: Makefile,v 1.39 1996/08/04 22:34:23 wosch Exp $
#
# Doing a make install builds /usr/include
#
# The ``rm -rf''s used below are safe because rm doesn't follow symbolic
# links.
#
all depend lint tags:

CLEANFILES=version vers.c
SUBDIR= rpcsvc
FILES=	a.out.h ar.h assert.h bitstring.h ctype.h db.h dirent.h disktab.h \
	err.h f2c.h fnmatch.h fstab.h fts.h glob.h grp.h strhash.h histedit.h \
	kvm.h limits.h link.h locale.h malloc.h memory.h mpool.h ndbm.h \
	netdb.h nl_types.h nlist.h paths.h pthread.h pthread_np.h pwd.h \
	ranlib.h regex.h regexp.h resolv.h rune.h runetype.h setjmp.h sgtty.h \
	signal.h stab.h stddef.h stdio.h stdlib.h string.h strings.h \
	struct.h sysexits.h tar.h time.h timers.h ttyent.h unistd.h utime.h \
	utmp.h vis.h
.if defined(WANT_CSRG_LIBM)
FILES+=	math.h
.endif

MFILES=	float.h floatingpoint.h stdarg.h varargs.h
LFILES=	errno.h fcntl.h syslog.h termios.h

DIRS=	arpa protocols rpc
LDIRS=  net netatalk netinet netipx netkey nfs pccard sys vm #netccitt netiso netns
LUDIR=	ufs
UDIRS=	ufs/ffs ufs/lfs ufs/mfs ufs/ufs

# Define SHARED to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
# SHARED=	copies
SHARED?=	symlinks

OSREL	= ${DESTDIR}/usr/include/osreldate.h
beforeinstall: ${SHARED}
	@${ECHO} installing ${FILES}
	@cd ${.CURDIR}; \
	for i in ${FILES}; do \
		cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		    ${DESTDIR}/usr/include/$$i; \
	done
.if exists (${.CURDIR}/../sys/conf/newvers.sh)
	@${ECHO} creating osreldate.h from newvers.sh
	. ${.CURDIR}/../sys/conf/newvers.sh;			\
	echo "$$COPYRIGHT" >> ${OSREL}.new;			\
	echo \#'undef __FreeBSD_version' >> ${OSREL}.new;	\
	echo \#'define __FreeBSD_version' $$RELDATE >> ${OSREL}.new
.else
	@${ECHO} creating osreldate.h from sysctl info
	@echo \#'undef __FreeBSD_version' > ${OSREL}.new
	@echo \#'define __FreeBSD_version '`sysctl -n kern.osreldate` \
		>> ${OSREL}.new
.endif
	@if cmp -s ${OSREL} ${OSREL}.new ; then \
		rm -f ${OSREL}.new ; else \
		mv -f ${OSREL}.new ${OSREL} ; fi
	@chown ${BINOWN}.${BINGRP} ${OSREL}
	@chmod 444 ${OSREL}
	@${ECHO} installing ${DIRS}
	@-for i in ${DIRS}; do \
		if [ ! -d ${DESTDIR}/usr/include/$$i ]; \
		then \
			mkdir ${DESTDIR}/usr/include/$$i; \
		fi; \
		chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/$$i; \
		chmod 755 ${DESTDIR}/usr/include/$$i; \
		(cd ${.CURDIR}/$$i; for j in *.[ih]; do \
			cmp -s $$j ${DESTDIR}/usr/include/$$i/$$j || \
			    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 $$j \
			    ${DESTDIR}/usr/include/$$i/$$j; \
		done); \
	done
	@${ECHO} installing ${LFILES}
	@-for i in ${LFILES}; do \
		rm -f ${DESTDIR}/usr/include/$$i; \
		ln -s sys/$$i ${DESTDIR}/usr/include/$$i; \
		chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/$$i; \
	done
	@${ECHO} installing ${MFILES}
	@-for i in ${MFILES}; do \
		rm -f ${DESTDIR}/usr/include/$$i; \
		ln -s machine/$$i ${DESTDIR}/usr/include/$$i; \
		chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/$$i; \
	done

copies:
	rm -rf ${DESTDIR}/usr/include/${LUDIR}
	mkdir ${DESTDIR}/usr/include/${LUDIR}
	chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/${LUDIR}
	chmod 755 ${DESTDIR}/usr/include/${LUDIR}
	@-for i in ${LDIRS} ${UDIRS}; do				\
		${ECHO} ${SHARED} $$i;					\
		rm -rf ${DESTDIR}/usr/include/$$i;			\
		cd ${.CURDIR}/../sys;					\
		tar cf - $$i/*.h |					\
			(cd ${DESTDIR}/usr/include; tar xpfB -);	\
		chown -R ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/$$i;\
		chmod -R 444 ${DESTDIR}/usr/include/$$i;		\
		chmod 755 ${DESTDIR}/usr/include/$$i;			\
	done
	rm -rf ${DESTDIR}/usr/include/machine
	mkdir ${DESTDIR}/usr/include/machine
	cd ${.CURDIR}/../sys/${MACHINE}/include && \
	    tar cf - *.h | (cd ${DESTDIR}/usr/include/machine; tar xpfB -);
	chown -R ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/machine;
	chmod -R 444 ${DESTDIR}/usr/include/machine;
	chmod 755 ${DESTDIR}/usr/include/machine;

symlinks:
	@for i in ${LDIRS} ${LUDIR}; do					\
		${ECHO} ${SHARED} $$i;					\
		rm -rf ${DESTDIR}/usr/include/$$i;			\
		ln -s /sys/$$i ${DESTDIR}/usr/include/$$i;		\
		chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/$$i;	\
	done
	rm -rf ${DESTDIR}/usr/include/machine
	ln -s /sys/${MACHINE}/include ${DESTDIR}/usr/include/machine

.include <bsd.prog.mk>
