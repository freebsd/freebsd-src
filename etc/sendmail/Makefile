#	@(#)Makefile	8.19 (Berkeley) 1/14/97
# $FreeBSD$

M4=	m4
CFDIR=	${.CURDIR}/../../contrib/sendmail/cf
CHMOD=	chmod
ROMODE=	444
RM=	rm -f

.SUFFIXES:  .mc .cf

.mc.cf:
	$(RM) ${.TARGET}
	(cd ${.CURDIR} && \
	    $(M4) -D_CF_DIR_=${CFDIR}/ ${CFDIR}/m4/cf.m4 ${@:R}.mc) > ${.TARGET}
	$(CHMOD) $(ROMODE) ${.TARGET}

ALL=	freebsd.cf

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

.if defined(INSTALL_CF)
ALL+=	${INSTALL_CF}
.endif

# Additional .cf files to build
.if defined(SENDMAIL_ADDITIONAL_MC)
ALL+=	${SENDMAIL_ADDITIONAL_MC:S/.mc$/.cf/g}
.endif

CLEANFILES+=$(ALL)

all: $(ALL)

depend:

.if defined(INSTALL_CF)
install: ${INSTALL_CF}
.if (defined(SENDMAIL_MC) && defined(SENDMAIL_CF))
	@echo ">>> ERROR: Both SENDMAIL_CF and SENDMAIL_MC can not be set"
	@false
.endif
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 ${INSTALL_CF} \
		${DESTDIR}/etc/mail/sendmail.cf
.else
install:
.endif

# Helper for src/etc/Makefile
distribution: freebsd.cf freebsd.mc
.if (defined(SENDMAIL_MC) && defined(SENDMAIL_CF))
	@echo ">>> ERROR: Both SENDMAIL_CF and SENDMAIL_MC can not be set"
	@false
.endif
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 ${.CURDIR}/freebsd.mc \
		${DESTDIR}/etc/mail/freebsd.mc
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 freebsd.cf \
		${DESTDIR}/etc/mail/freebsd.cf
.if defined(INSTALL_CF)
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 ${INSTALL_CF} \
		${DESTDIR}/etc/mail/sendmail.cf
.else
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 freebsd.cf \
		${DESTDIR}/etc/mail/sendmail.cf
.endif
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/sendmail.st

# this is overkill, but....
M4FILES!= find ${CFDIR} -type f -name '*.m4' -print

$(ALL):  $(M4FILES)

.include <bsd.prog.mk>
