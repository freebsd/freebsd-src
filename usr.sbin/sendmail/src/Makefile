#	@(#)Makefile	8.7 (Berkeley) 10/31/95

#########################################################################
#  This Makefile is for 4.4BSD only!!!  For all other systems, use	#
#  the "makesendmail" script.						#
#########################################################################

PROG=	sendmail

# define the database format to use for aliases et al.  Can be -DNEWDB (for
# the new BSD database package -- this is preferred) or -DNDBM for the NDBM
# database package.  The old putrescent V7 DBM package is no longer
# supported.
# You can define both NEWDB and NDBM during a transition period; old
# databases are read, but the new format will be used on any rebuilds.  On
# really gnarly systems, you can set this to null; it will crawl like a high
# spiral snail, but it will work.
DBMDEF=	-DNEWDB

CFLAGS+=-I${.CURDIR} ${DBMDEF} -DNETISO

SRCS=	alias.c arpadate.c clock.c collect.c conf.c convtime.c daemon.c \
	deliver.c domain.c envelope.c err.c headers.c macro.c main.c map.c \
	mci.c mime.c parseaddr.c queue.c readcf.c recipient.c savemail.c \
	srvrsmtp.c stab.c stats.c sysexits.c trace.c udb.c usersmtp.c \
	util.c version.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN1=	mailq.1 newaliases.1
MAN5=	aliases.5
MAN8=	sendmail.8
LINKS=	/usr/sbin/sendmail /usr/bin/newaliases \
	/usr/sbin/sendmail /usr/bin/mailq
BINDIR=	/usr/sbin
BINOWN=	root
BINGRP=	kmem
BINMODE=6555

beforeinstall:
#	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
#	    ${DESTDIR}/etc/sendmail.fc
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 /dev/null \
	    ${DESTDIR}/var/log/sendmail.st
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/sendmail.hf \
	    ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
