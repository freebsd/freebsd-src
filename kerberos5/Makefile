# $FreeBSD$

SUBDIR=	doc lib libexec usr.bin usr.sbin

# These are the programs which depend on Kerberos.
KPROGS=	lib/libpam \
	secure/lib/libssh secure/usr.bin/ssh secure/usr.sbin/sshd

# This target is used to rebuild these programs WITH Kerberos.
kerberize:
.for entry in ${KPROGS}
	cd ${.CURDIR}/../${entry}; \
	${MAKE} ${MFLAGS} cleandir; \
	${MAKE} ${MFLAGS} obj; \
	${MAKE} ${MFLAGS} depend; \
	${MAKE} ${MFLAGS} all; \
	${MAKE} ${MFLAGS} install
.endfor

# This target is used to rebuild these programs WITHOUT Kerberos.
dekerberize:
.for entry in ${KPROGS}
	cd ${.CURDIR}/../${entry}; \
	${MAKE} ${MFLAGS} -DNO_KERBEROS cleandir; \
	${MAKE} ${MFLAGS} -DNO_KERBEROS obj; \
	${MAKE} ${MFLAGS} -DNO_KERBEROS depend; \
	${MAKE} ${MFLAGS} -DNO_KERBEROS all; \
	${MAKE} ${MFLAGS} -DNO_KERBEROS install
.endfor

.include <bsd.subdir.mk>
