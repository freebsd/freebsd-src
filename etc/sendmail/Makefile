#	@(#)Makefile	8.19 (Berkeley) 1/14/97
# $FreeBSD: src/etc/sendmail/Makefile,v 1.3 1999/12/19 19:55:18 obrien Exp $

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

# Local sendmail.cf, may be set in /etc/make.conf.  Warning! If set, this
# causes 'make install' to always copy it over /etc/mail/sendmail.cf!!!
# Caveat emptor!  Be sure you want this before you enable it.
.if defined(SENDMAIL_CF)
ALL+=	${SENDMAIL_CF}
.endif

CLEANFILES+=$(ALL)

all: $(ALL)

depend:

install:
.if defined(SENDMAIL_CF)
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 ${SENDMAIL_CF} \
		${DESTDIR}/etc/mail/sendmail.cf
.endif

# Helper for src/etc/Makefile
etc-sendmail.cf: freebsd.cf
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 644 freebsd.cf \
		${DESTDIR}/etc/mail/sendmail.cf

# this is overkill, but....
M4FILES!= find ${CFDIR} -type f -name '*.m4' -print

$(ALL):  $(M4FILES)

.include <bsd.prog.mk>
