#	@(#)Makefile	8.8 (Berkeley) 3/28/97
# $FreeBSD$

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
SMDIR=	${SENDMAIL_DIR}/src
.PATH:	${SMDIR}

BINDIR?=/usr/libexec/sendmail

PROG=	sendmail

# Define the database format to use for aliases et al.
DBMDEF=	-DNEWDB

# If you don't want NIS alias/map support, comment out this line
NIS=	-DNIS

# Map extensions
MAPS=	-DMAP_REGEX

CFLAGS+=-I${SMDIR} -I${SENDMAIL_DIR}/include
CFLAGS+=${DBMDEF} ${NIS} -DNETINET6 -DTCPWRAPPERS ${MAPS}

SRCS=	alias.c arpadate.c bf_torek.c clock.c collect.c conf.c control.c \
	convtime.c daemon.c deliver.c domain.c envelope.c err.c headers.c \
	macro.c main.c map.c mci.c milter.c mime.c parseaddr.c queue.c \
	readcf.c recipient.c savemail.c sfsasl.c shmticklib.c srvrsmtp.c \
	stab.c stats.c sysexits.c timers.c trace.c udb.c usersmtp.c util.c \
	version.c
DPADD=	${LIBUTIL} ${LIBWRAP}
LDADD=	-lutil -lwrap
MAN1=	mailq.1 newaliases.1
MAN5=	aliases.5
MAN8=	sendmail.8 
BINMODE=4555

.if exists(${.OBJDIR}/../../lib/libsmutil)
LIBSMUTILDIR:=	${.OBJDIR}/../../lib/libsmutil
.else
LIBSMUTILDIR!=	cd ${.CURDIR}/../../lib/libsmutil; make -V .OBJDIR
.endif
LIBSMUTIL:=	${LIBSMUTILDIR}/libsmutil.a

DPADD=	${LIBSMUTIL}
LDADD=	${LIBSMUTIL}

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
# STARTTLS support
CFLAGS+=-DSTARTTLS -D_FFR_TLS_O_T -D_FFR_TLS_1 -D_FFR_TLS_TOREK
LDADD+=	-lssl -lcrypto
DPADD+=	${LIBSSL} ${LIBCRYPTO}
.endif

# User customizations to the sendmail build environment
CFLAGS+=${SENDMAIL_CFLAGS}
DPADD+=${SENDMAIL_DPADD}
LDADD+=${SENDMAIL_LDADD}
LDFLAGS+=${SENDMAIL_LDFLAGS}

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/sendmail.st
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${SMDIR}/helpfile \
	    ${DESTDIR}/etc/mail/helpfile

afterinstall:
	@if [ -d ${DESTDIR}/etc/mail -a -f ${DESTDIR}/etc/sendmail.cf -a \
	      ! -f ${DESTDIR}/etc/mail/sendmail.cf ]; then \
		set -x; \
		mv -f ${DESTDIR}/etc/sendmail.cf \
		    ${DESTDIR}/etc/mail/sendmail.cf; \
	fi

.include <bsd.prog.mk>
