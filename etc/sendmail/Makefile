#	@(#)Makefile	8.19 (Berkeley) 1/14/97
# $FreeBSD$

M4=		m4
CHMOD=		chmod
ROMODE=		444
RM=		rm -f

SENDMAIL_DIR=	${.CURDIR}/../../contrib/sendmail
SMDIR=		${SENDMAIL_DIR}/src
CFDIR=		${SENDMAIL_DIR}/cf

# this is overkill, but....
M4FILES!=	find ${CFDIR} -type f -name '*.m4' -print

.SUFFIXES:	.mc .cf

.mc.cf:		${M4FILES}
	${RM} ${.TARGET}
	(cd ${.CURDIR} && \
	    ${M4} -D_CF_DIR_=${CFDIR}/ ${SENDMAIL_M4_FLAGS} \
		${CFDIR}/m4/cf.m4 ${@:R}.mc) > ${.TARGET}
	${CHMOD} ${ROMODE} ${.TARGET}

ALL=		freebsd.cf
CLEANFILES+=	freebsd.cf

# Local SENDMAIL_MC or SENDMAIL_CF may be set in /etc/make.conf.
# Warning! If set, this causes 'make install' to always copy it
# over /etc/mail/sendmail.cf!!!
# Caveat emptor!  Be sure you want this before you enable it.
.if defined(SENDMAIL_MC)
INSTALL_CF=	${SENDMAIL_MC:R}.cf
.else
.if defined(SENDMAIL_CF)
INSTALL_CF=	${SENDMAIL_CF}
.endif
.endif

DEST_CF=	${DESTDIR}/etc/mail/sendmail.cf

ALL+=		${INSTALL_CF}

# Make sure we don't remove /etc/mail/sendmail.cf on make clean
# since this will break a running system during a buildworld.
.if defined(INSTALL_CF)
.if ${INSTALL_CF} != ${DEST_CF}
CLEANFILES+=	${INSTALL_CF}
.endif
.endif

# Additional .cf files to build
.if defined(SENDMAIL_ADDITIONAL_MC)
SENDMAIL_ADDITIONAL_CF=	${SENDMAIL_ADDITIONAL_MC:S/.mc$/.cf/g}
ALL+=		${SENDMAIL_ADDITIONAL_CF}
CLEANFILES+=	${SENDMAIL_ADDITIONAL_CF}
.endif

all:		${ALL}

depend:

install:	${INSTALL_CF}
.if (defined(SENDMAIL_MC) && defined(SENDMAIL_CF))
	@echo ">>> ERROR: Both SENDMAIL_CF and SENDMAIL_MC can not be set"
	@false
.endif
.if defined(INSTALL_CF)
.if ${INSTALL_CF} != ${DEST_CF}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 ${INSTALL_CF} \
		${DEST_CF}
.endif
.if defined(SENDMAIL_ADDITIONAL_CF)
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 \
		${SENDMAIL_ADDITIONAL_CF} ${DESTDIR}/etc/mail
.endif
.endif

# Helper for src/etc/Makefile
distribution:	freebsd.cf freebsd.mc ${INSTALL_CF}
.if (defined(SENDMAIL_MC) && defined(SENDMAIL_CF))
	@echo ">>> ERROR: Both SENDMAIL_CF and SENDMAIL_MC can not be set"
	@false
.endif
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 ${.CURDIR}/freebsd.mc \
		${DESTDIR}/etc/mail/freebsd.mc
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 freebsd.cf \
		${DESTDIR}/etc/mail/freebsd.cf
.if defined(INSTALL_CF)
.if ${INSTALL_CF} != ${DEST_CF}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 ${INSTALL_CF} \
		${DEST_CF}
.endif
.else
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 freebsd.cf \
		${DEST_CF}
.endif
.ifndef SENDMAIL_SET_USER_ID
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${CFDIR}/cf/submit.mc \
	    ${DESTDIR}/etc/mail/submit.mc
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${CFDIR}/cf/submit.cf \
	    ${DESTDIR}/etc/mail/submit.cf
.endif
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${SMDIR}/helpfile \
	    ${DESTDIR}/etc/mail/helpfile
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/sendmail.st

.include <bsd.prog.mk>
