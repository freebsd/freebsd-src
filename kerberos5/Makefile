# $FreeBSD$

SUBDIR=	doc tools lib libexec usr.bin usr.sbin

# These are the programs which depend on Kerberos.
KPROGS=	lib/libpam \
	secure/lib/libssh secure/usr.bin/ssh secure/usr.sbin/sshd

# This target is used to rebuild these programs WITH Kerberos.
kerberize:
.for entry in ${KPROGS}
	cd ${.CURDIR}/../${entry}; \
	${MAKE} cleandir; \
	${MAKE} obj; \
	${MAKE} depend; \
	${MAKE} all; \
	${MAKE} install
.endfor

# This target is used to rebuild these programs WITHOUT Kerberos.
dekerberize:
.for entry in ${KPROGS}
	cd ${.CURDIR}/../${entry}; \
	${MAKE} -DNO_KERBEROS cleandir; \
	${MAKE} -DNO_KERBEROS obj; \
	${MAKE} -DNO_KERBEROS depend; \
	${MAKE} -DNO_KERBEROS all; \
	${MAKE} -DNO_KERBEROS install
.endfor

.include <bsd.subdir.mk>
