# $FreeBSD$

.include <src.opts.mk>

SUBDIR= lib .WAIT \
	libexec ${_tests} usr.bin usr.sbin
SUBDIR_PARALLEL=

.if ${MK_TESTS} != "no"
_tests=	tests
.endif

# These are the programs which depend on crypto, but not Kerberos.
SPROGS=	lib/libfetch lib/libpam lib/libradius lib/libtelnet	\
	bin/ed libexec/telnetd usr.bin/fetch usr.bin/telnet	\
	usr.sbin/pkg_install usr.sbin/ppp usr.sbin/tcpdump/tcpdump
.if ${MK_SENDMAIL} != "no"
SPROGS+=usr.sbin/sendmail
.endif

# This target is used to rebuild these programs with crypto.
secure: .MAKE .PHONY
.for entry in ${SPROGS}
	cd ${.CURDIR}/../${entry}; \
	${MAKE} cleandir; \
	${MAKE} obj; \
	${MAKE} depend; \
	${MAKE} all; \
	${MAKE} install
.endfor

# This target is used to rebuild these programs without crypto.
insecure: .MAKE .PHONY
.for entry in ${SPROGS}
	cd ${.CURDIR}/../${entry}; \
	${MAKE} MK_CRYPT=no cleandir; \
	${MAKE} MK_CRYPT=no obj; \
	${MAKE} MK_CRYPT=no depend; \
	${MAKE} MK_CRYPT=no all; \
	${MAKE} MK_CRYPT=no install
.endfor

.include <bsd.subdir.mk>
