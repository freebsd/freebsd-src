#	From: @(#)Makefile	8.2 (Berkeley) 1/4/94
#	$Id: Makefile,v 1.45.2.9 1998/04/19 13:34:48 bde Exp $
#
# Doing a make install builds /usr/include
#
# The ``rm -rf''s used below are safe because rm doesn't follow symbolic
# links.
#
depend lint tags:

CLEANFILES= osreldate.h version vers.c
SUBDIR= rpcsvc
FILES=	a.out.h ar.h assert.h bitstring.h ctype.h db.h dirent.h disktab.h \
	elf.h err.h f2c.h fnmatch.h fstab.h fts.h glob.h grp.h strhash.h \
	histedit.h kvm.h limits.h link.h locale.h malloc.h memory.h \
	mpool.h ndbm.h netdb.h nl_types.h nlist.h paths.h pthread.h \
	pthread_np.h pwd.h ranlib.h regex.h regexp.h resolv.h rune.h \
	runetype.h sched.h setjmp.h sgtty.h signal.h stab.h stddef.h stdio.h \
	stdlib.h string.h stringlist.h strings.h struct.h sysexits.h \
	tar.h time.h timers.h ttyent.h unistd.h utime.h utmp.h vis.h
.if defined(WANT_CSRG_LIBM)
FILES+=	math.h
.endif

ARPAFILES=	ftp.h inet.h nameser.h telnet.h tftp.h

PROTOFILES=	dumprestore.h routed.h rwhod.h talkd.h timed.h

RPCFILES=	auth.h auth_unix.h clnt.h pmap_clnt.h pmap_prot.h pmap_rmt.h \
		rpc.h rpc_msg.h svc.h svc_auth.h types.h xdr.h

MFILES=	float.h floatingpoint.h stdarg.h varargs.h
LFILES=	errno.h fcntl.h syslog.h termios.h

LDIRS=	net netns netatalk netinet netipx netkey nfs pccard sys vm \
		# netccitt netiso

LUDIR=	ufs
UDIRS=	ufs/ffs ufs/lfs ufs/mfs ufs/ufs

# Define SHARED to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies'').  ``symlinks'' is
# probably only useful for developers and should be avoided if you do not
# wish to tie your /usr/include and /usr/src together.
#SHARED=	symlinks
SHARED?=	copies


.if exists (${.CURDIR}/../sys/conf/newvers.sh)
all:	osreldate.h

osreldate.h:	${.CURDIR}/../sys/conf/newvers.sh
	@${ECHO} creating osreldate.h from newvers.sh
	. ${.CURDIR}/../sys/conf/newvers.sh;			\
	echo "$$COPYRIGHT" > osreldate.h;			\
	echo \#'undef __FreeBSD_version' >> osreldate.h;	\
	echo \#'define __FreeBSD_version' $$RELDATE >> osreldate.h
.else
all:
	@${ECHO} creating osreldate.h from sysctl info
	echo \#'undef __FreeBSD_version' > osreldate.h
	echo \#'define __FreeBSD_version '`sysctl -n kern.osreldate` \
		>> osreldate.h
.endif

beforeinstall: installhdrs ${SHARED}

.if make(installhdrs)
installhdrs: _SUBDIR
.endif
installhdrs:
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
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.OBJDIR}/osreldate.h \
		${DESTDIR}/usr/include
.for i in ${LFILES}
	ln -sf sys/$i ${DESTDIR}/usr/include/$i
.endfor
.for i in ${MFILES}
	ln -sf machine/$i ${DESTDIR}/usr/include/$i
.endfor

copies:
.for i in ${LDIRS} ${LUDIR} ${UDIRS} machine
	if [ -h ${DESTDIR}/usr/include/$i ]; then \
		rm -f ${DESTDIR}/usr/include/$i; \
	fi
.endfor
.for i in ${LDIRS} ${LUDIR} ${UDIRS} machine
	if [ ! -d ${DESTDIR}/usr/include/$i ]; then \
		mkdir ${DESTDIR}/usr/include/$i; \
		chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/$i; \
		chmod 755 ${DESTDIR}/usr/include/$i; \
	fi
.endfor
.for i in ${LDIRS} ${UDIRS}
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
.for i in ${LDIRS} ${LUDIR}
	rm -rf ${DESTDIR}/usr/include/$i
	ln -s ../../sys/$i ${DESTDIR}/usr/include/$i
.endfor
	rm -rf ${DESTDIR}/usr/include/machine
	ln -s ../../sys/${MACHINE}/include ${DESTDIR}/usr/include/machine

.include <bsd.prog.mk>
