# $FreeBSD$

MAINTAINER=	markm@FreeBSD.org

SUBDIR=	include lib libexec usr.bin usr.sbin

# These are the programs which depend on Kerberos.
KPROGS=	bin/rcp gnu/usr.bin/cvs libexec/ftpd libexec/rlogind libexec/rshd \
	sbin/dump sbin/restore usr.bin/login usr.bin/passwd \
	usr.bin/rlogin usr.bin/rsh usr.bin/su

# This target is used to rebuild these programs WITH Kerberos.
kerberize:
.for entry in ${KPROGS}
	cd ${.CURDIR}/../${entry}; \
	${MAKE} ${MFLAGS} -DMAKE_KERBEROS4 cleandir; \
	${MAKE} ${MFLAGS} -DMAKE_KERBEROS4 obj; \
	${MAKE} ${MFLAGS} -DMAKE_KERBEROS4 depend; \
	${MAKE} ${MFLAGS} -DMAKE_KERBEROS4 all; \
	${MAKE} ${MFLAGS} -DMAKE_KERBEROS4 install
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
