#	@(#)Makefile	8.8 (Berkeley) 3/28/97
#	$Id$

SMDIR=	${.CURDIR}/../../contrib/sendmail/src
.PATH:	${SMDIR}

PROG=	sendmail

# Define the database format to use for aliases et al.
DBMDEF=	-DNEWDB

# If you don't want NIS alias/map support, comment out this line
NIS=	-DNIS

# Map extensions
MAPS=	-DMAP_REGEX

CFLAGS+=-I${SMDIR} ${DBMDEF} ${NIS} -DTCPWRAPPERS ${MAPS}
CFLAGS+=-D_FFR_MAX_MIME_HEADER_LENGTH
CFLAGS+=-D_FFR_MAX_HEADERS_LENGTH

SRCS=	alias.c arpadate.c clock.c collect.c conf.c control.c convtime.c \
	daemon.c deliver.c domain.c envelope.c err.c headers.c macro.c \
	main.c map.c mci.c mime.c parseaddr.c queue.c readcf.c recipient.c \
	safefile.c savemail.c snprintf.c srvrsmtp.c stab.c stats.c \
	sysexits.c trace.c udb.c usersmtp.c util.c version.c
DPADD=	${LIBUTIL} ${LIBWRAP}
LDADD=	-lutil -lwrap
MAN1=	mailq.1 newaliases.1
MAN5=	aliases.5
MAN8=	sendmail.8 
LINKS=	${BINDIR}/sendmail /usr/bin/newaliases \
	${BINDIR}/sendmail /usr/bin/mailq \
	${BINDIR}/sendmail /usr/bin/hoststat \
	${BINDIR}/sendmail ${BINDIR}/purgestat
BINMODE=4555

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/sendmail.st
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${SMDIR}/sendmail.hf \
	    ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
