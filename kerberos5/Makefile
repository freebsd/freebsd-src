# $FreeBSD$

SUBDIR=	lib libexec usr.bin usr.sbin

# These are the programs which depend on Kerberos.
KPROGS=	secure/usr.bin/ssh secure/usr.sbin/sshd

# This target is used to rebuild these programs WITH Kerberos.
kerberize:
.for entry in ${KPROGS}
	cd ${.CURDIR}/../${entry}; \
	${MAKE} ${MFLAGS} -DMAKE_KERBEROS5 cleandir; \
	${MAKE} ${MFLAGS} -DMAKE_KERBEROS5 obj; \
	${MAKE} ${MFLAGS} -DMAKE_KERBEROS5 depend; \
	${MAKE} ${MFLAGS} -DMAKE_KERBEROS5 all; \
	${MAKE} ${MFLAGS} -DMAKE_KERBEROS5 install
.endfor

# This target is used to rebuild these programs WITHOUT Kerberos.
dekerberize:
.for entry in ${KPROGS}
	cd ${.CURDIR}/../${entry}; \
	${MAKE} ${MFLAGS} cleandir; \
	${MAKE} ${MFLAGS} obj; \
	${MAKE} ${MFLAGS} depend; \
	${MAKE} ${MFLAGS} all; \
	${MAKE} ${MFLAGS} install
.endfor

.include <bsd.subdir.mk>
