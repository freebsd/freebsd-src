#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$
#
# Files are:
#	/etc/remote		remote host description file
#	/etc/phones		phone number file, owned by ${OWNER} and
#				mode 6??
#	/var/log/aculog		ACU accounting file, owned by ${OWNER} and
#				mode 6?? {if ACULOG defined}

.if exists(${.OBJDIR}/../libacu)
LIBACU=${.OBJDIR}/../libacu/libacu.a
.else
LIBACU=${.CURDIR}/../libacu/libacu.a
.endif

PROG=	tip
DPADD=	${LIBACU} ${LIBUTIL}
LDADD=	${LIBACU} -lutil
LINKS=	${BINDIR}/tip ${BINDIR}/cu
MAN=	tip.1 modems.5
MLINKS=	tip.1 cu.1
SRCS=	acu.c acutab.c cmds.c cmdtab.c cu.c hunt.c log.c partab.c \
	remote.c tip.c tipout.c value.c vars.c

BINDIR?=	/usr/bin
BINOWN=		uucp
BINGRP=		dialer
#BINMODE?=	4510

# XXX: there is some concern that `tip' in its current state shouldn't run
#      SUID.  If it believed it should, the mode above may still no be proper.

# Dirty, rotten hack. This can be removed when we are confident that there
# is no cu(1) with the schg-bit set.
beforeinstall:
	-@chflags noschg ${DESTDIR}/${BINDIR}/cu

.include <bsd.prog.mk>
