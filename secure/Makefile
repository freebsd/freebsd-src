# $FreeBSD$

SUBDIR= lib libexec usr.bin usr.sbin

# These are the programs which depend on crypto, but not Kerberos.
SPROGS=	lib/libfetch lib/libpam lib/libradius lib/libtelnet	\
	bin/ed libexec/telnetd usr.bin/fetch usr.bin/telnet	\
	usr.sbin/pkg_install usr.sbin/ppp usr.sbin/pppd		\
	usr.sbin/tcpdump/tcpdump
.if !defined(NO_SENDMAIL)
SPROGS+=usr.sbin/sendmail
.endif

# This target is used to rebuild these programs with crypto.
secure:
.for entry in ${SPROGS}
	cd ${.CURDIR}/../${entry}; \
	${MAKE} cleandir; \
	${MAKE} obj; \
	${MAKE} depend; \
	${MAKE} all; \
	${MAKE} install
.endfor

# This target is used to rebuild these programs without crypto.
insecure:
.for entry in ${SPROGS}
	cd ${.CURDIR}/../${entry}; \
	${MAKE} -DNOCRYPT cleandir; \
	${MAKE} -DNOCRYPT obj; \
	${MAKE} -DNOCRYPT depend; \
	${MAKE} -DNOCRYPT all; \
	${MAKE} -DNOCRYPT install
.endfor

.include <bsd.subdir.mk>
