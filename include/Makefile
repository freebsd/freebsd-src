#	From: @(#)Makefile	8.2 (Berkeley) 1/4/94
#	$Id: Makefile,v 1.52 1997/03/09 07:12:37 bde Exp $
#
# Doing a make install builds /usr/include
#
# The ``rm -rf''s used below are safe because rm doesn't follow symbolic
# links.
#
all depend lint tags:

CLEANFILES= osreldate.h version vers.c
SUBDIR= rpcsvc
FILES=	a.out.h ar.h assert.h bitstring.h ctype.h db.h dirent.h disktab.h \
	err.h fnmatch.h fstab.h fts.h glob.h grp.h strhash.h histedit.h \
	kvm.h limits.h link.h locale.h login_cap.h malloc.h memory.h mpool.h \
	ndbm.h netdb.h nl_types.h nlist.h paths.h pthread.h pthread_np.h pwd.h \
	ranlib.h regex.h regexp.h resolv.h rune.h runetype.h setjmp.h sgtty.h \
	signal.h stab.h stddef.h stdio.h stdlib.h string.h strings.h \
	struct.h sysexits.h tar.h time.h timers.h ttyent.h unistd.h utime.h \
	utmp.h vis.h
.if defined(WANT_CSRG_LIBM)
FILES+=	math.h
.endif

ARPAFILES=	ftp.h inet.h nameser.h telnet.h tftp.h

PROTOFILES=	dumprestore.h routed.h rwhod.h talkd.h timed.h

RPCFILES=	auth.h auth_unix.h clnt.h pmap_clnt.h pmap_prot.h pmap_rmt.h \
		rpc.h rpc_msg.h svc.h svc_auth.h types.h xdr.h

MFILES=	float.h floatingpoint.h stdarg.h varargs.h
LFILES=	errno.h fcntl.h syslog.h termios.h

LDIRS=	msdosfs net netns netatalk netinet netipx netkey nfs pccard sys vm \
		# netccitt netiso

LNOHEADERDIRS=	isofs ufs
LSUBDIRS=	isofs/cd9660 ufs/ffs ufs/lfs ufs/mfs ufs/ufs

# Define SHARED to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
# SHARED=	copies
SHARED?=	symlinks

beforeinstall: ${SHARED}
	cd ${.CURDIR}; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${FILES} ${DESTDIR}/usr/include
	cd ${.CURDIR}/arpa; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${ARPAFILES} ${DESTDIR}/usr/include/arpa
	cd ${.CURDIR}/protocols; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${PROTOFILES} ${DESTDIR}/usr/include/protocols
	cd ${.CURDIR}/rpc; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${RPCFILES} ${DESTDIR}/usr/include/rpc
	@rm -f osreldate.h
.if exists (${.CURDIR}/../sys/conf/newvers.sh)
	@${ECHO} creating osreldate.h from newvers.sh
	. ${.CURDIR}/../sys/conf/newvers.sh;			\
	echo "$$COPYRIGHT" > osreldate.h;			\
	echo \#'undef __FreeBSD_version' >> osreldate.h;	\
	echo \#'define __FreeBSD_version' $$RELDATE >> osreldate.h
.else
	@${ECHO} creating osreldate.h from sysctl info
	@echo \#'undef __FreeBSD_version' > osreldate.h
	@echo \#'define __FreeBSD_version '`sysctl -n kern.osreldate` \
		>> osreldate.h
.endif
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 osreldate.h \
		${DESTDIR}/usr/include
.for i in ${LFILES}
	ln -sf sys/$i ${DESTDIR}/usr/include/$i
.endfor
.for i in ${MFILES}
	ln -sf machine/$i ${DESTDIR}/usr/include/$i
.endfor

copies:
.for i in ${LDIRS} ${LNOHEADERDIRS} machine
	if [ -h ${DESTDIR}/usr/include/$i ]; then \
		rm -f ${DESTDIR}/usr/include/$i; \
	fi
.endfor
	mtree -deU -f ${.CURDIR}/../etc/mtree/BSD.include.dist \
		-p ${DESTDIR}/usr/include
.for i in ${LDIRS} ${LSUBDIRS}
	cd ${.CURDIR}/../sys; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 $i/*.h \
		${DESTDIR}/usr/include/$i
.endfor
.if exists(${.CURDIR}/../sys/${MACHINE}/include)
	cd ${.CURDIR}/../sys/${MACHINE}/include; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
		${DESTDIR}/usr/include/machine
.endif

symlinks:
	@${ECHO} "Setting up symlinks to kernel source tree..."
.for i in ${LDIRS} ${LNOHEADERDIRS}
	rm -rf ${DESTDIR}/usr/include/$i
	ln -s /sys/$i ${DESTDIR}/usr/include/$i
.endfor
	rm -rf ${DESTDIR}/usr/include/machine
	ln -s /sys/${MACHINE}/include ${DESTDIR}/usr/include/machine

.include <bsd.prog.mk>
