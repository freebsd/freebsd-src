#	@(#)Makefile	8.2 (Berkeley) 1/4/94
# $FreeBSD$
#
# Doing a make install builds /usr/include

CLEANFILES= osreldate.h version vers.c
SUBDIR= rpcsvc
FILES=	a.out.h ar.h assert.h bitstring.h ctype.h db.h dirent.h disktab.h \
	dlfcn.h elf.h err.h fnmatch.h fstab.h \
	fts.h glob.h grp.h strhash.h histedit.h ieeefp.h iso646.h \
	limits.h link.h locale.h malloc.h memory.h mpool.h \
	ndbm.h netdb.h nl_types.h nlist.h objformat.h \
	paths.h pthread.h pthread_np.h pwd.h \
	ranlib.h regex.h regexp.h resolv.h rune.h runetype.h setjmp.h sgtty.h \
	signal.h stab.h stddef.h stdio.h stdlib.h string.h stringlist.h \
	strings.h struct.h sysexits.h tar.h time.h timers.h \
	ttyent.h unistd.h utime.h utmp.h vis.h
.if defined(WANT_CSRG_LIBM)
FILES+=	math.h
.endif

ARPAFILES=	ftp.h inet.h nameser.h nameser_compat.h telnet.h tftp.h

PROTOFILES=	dumprestore.h routed.h rwhod.h talkd.h timed.h

RPCFILES=	auth.h auth_unix.h clnt.h pmap_clnt.h pmap_prot.h pmap_rmt.h \
		rpc.h rpc_com.h rpc_msg.h svc.h svc_auth.h types.h xdr.h \
		auth_des.h des.h des_crypt.h

MFILES=	float.h floatingpoint.h stdarg.h varargs.h

# posix4/aio.h conflicts with dysons and isn't installed:
PFILES=	mqueue.h sched.h semaphore.h # aio.h

SFILES=	joystick.h soundcard.h

LFILES=	aio.h errno.h fcntl.h inttypes.h linker_set.h poll.h syslog.h \
	termios.h ucontext.h

LDIRS=	cam cam/scsi isofs/cd9660 msdosfs net netatalk netatm netgraph \
	netinet netinet6 netipx netkey netncp netns nfs ntfs nwfs pccard \
	posix4 sys ufs/ffs ufs/mfs ufs/ufs vm

all:	osreldate.h

osreldate.h:	${.CURDIR}/../sys/conf/newvers.sh \
		${.CURDIR}/../sys/sys/param.h
	@${ECHO} creating osreldate.h from newvers.sh
	setvar PARAMFILE ${.CURDIR}/../sys/sys/param.h; \
	. ${.CURDIR}/../sys/conf/newvers.sh;			\
	echo "$$COPYRIGHT" > osreldate.h;			\
	echo \#'undef __FreeBSD_version' >> osreldate.h;	\
	echo \#'define __FreeBSD_version' $$RELDATE >> osreldate.h

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.OBJDIR}/osreldate.h \
		${DESTDIR}/usr/include
	cd ${.CURDIR}; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
			${DESTDIR}/usr/include
	cd ${.CURDIR}/arpa; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${ARPAFILES} \
			${DESTDIR}/usr/include/arpa
	cd ${.CURDIR}/protocols; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${PROTOFILES} \
			${DESTDIR}/usr/include/protocols
	cd ${.CURDIR}/rpc; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${RPCFILES} \
			${DESTDIR}/usr/include/rpc
.for i in ${LDIRS}
	cd ${.CURDIR}/../sys; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 $i/*.h \
			${DESTDIR}/usr/include/$i
.endfor
.if exists(${.CURDIR}/../sys/${MACHINE_ARCH}/include)
	cd ${.CURDIR}/../sys/${MACHINE_ARCH}/include; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
			${DESTDIR}/usr/include/machine
.endif
.for i in ${LFILES}
	ln -sf sys/$i ${DESTDIR}/usr/include/$i
.endfor
.for i in ${MFILES}
	ln -sf machine/$i ${DESTDIR}/usr/include/$i
.endfor
.for i in ${PFILES}
	ln -sf posix4/$i ${DESTDIR}/usr/include/$i
.endfor
.for i in ${SFILES}
	ln -sf ../sys/$i ${DESTDIR}/usr/include/machine/$i
.endfor

.include <bsd.prog.mk>
