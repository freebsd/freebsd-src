#	@(#)Makefile	8.8 (Berkeley) 3/28/97
# $FreeBSD$

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
SMDIR=	${SENDMAIL_DIR}/src
.PATH: ${SMDIR}

BINDIR=	/usr/libexec/sendmail

PROG=	sendmail
MAN=	mailq.1 newaliases.1 aliases.5 sendmail.8 
SRCS=	alias.c arpadate.c bf.c collect.c conf.c control.c \
	convtime.c daemon.c deliver.c domain.c envelope.c err.c headers.c \
	macro.c main.c map.c mci.c milter.c mime.c parseaddr.c queue.c \
	ratectrl.c readcf.c recipient.c savemail.c sasl.c sfsasl.c \
	shmticklib.c sm_resolve.c srvrsmtp.c stab.c stats.c sysexits.c \
	timers.c tls.c trace.c udb.c usersmtp.c util.c version.c
BINOWN=	root
BINGRP=	smmsp
.ifdef SENDMAIL_SET_USER_ID
BINMODE=4555
.else
BINMODE=2555
.endif

# Define the database format to use for aliases et al.
DBMDEF=	-DNEWDB
# If you don't want NIS alias/map support, comment out this line
.if !defined(NO_NIS)
NIS=	-DNIS
.endif
# Map extensions
MAPS=	-DMAP_REGEX -DDNSMAP

CSTD?=	c89
CFLAGS+= -I${SMDIR} -I${SENDMAIL_DIR}/include -I.
CFLAGS+= ${DBMDEF} ${NIS} -DTCPWRAPPERS ${MAPS}

.if !defined(NOINET6)
CFLAGS+= -DNETINET6
.endif

DPADD=	${LIBUTIL} ${LIBWRAP}
LDADD=	-lutil -lwrap

.if exists(${.OBJDIR}/../../lib/libsm)
LIBSMDIR:=	${.OBJDIR}/../../lib/libsm
.else
LIBSMDIR!=	cd ${.CURDIR}/../../lib/libsm; make -V .OBJDIR
.endif
LIBSM:=		${LIBSMDIR}/libsm.a

.if exists(${.OBJDIR}/../../lib/libsmutil)
LIBSMUTILDIR:=	${.OBJDIR}/../../lib/libsmutil
.else
LIBSMUTILDIR!=	cd ${.CURDIR}/../../lib/libsmutil; make -V .OBJDIR
.endif
LIBSMUTIL:=	${LIBSMUTILDIR}/libsmutil.a

DPADD+=	${LIBSMUTIL} ${LIBSM}
LDADD+=	${LIBSMUTIL} ${LIBSM}

SRCS+=	sm_os.h
CLEANFILES+=sm_os.h

.if !defined(NOCRYPT) && !defined(NO_OPENSSL) && !defined(RELEASE_CRUNCH)
# STARTTLS support
CFLAGS+=	-DSTARTTLS -D_FFR_TLS_1 -D_FFR_DEAL_WITH_ERROR_SSL
DPADD+=		${LIBSSL} ${LIBCRYPTO}
LDADD+=		-lssl -lcrypto
.endif

# User customizations to the sendmail build environment
CFLAGS+=${SENDMAIL_CFLAGS}
DPADD+=${SENDMAIL_DPADD}
LDADD+=${SENDMAIL_LDADD}
LDFLAGS+=${SENDMAIL_LDFLAGS}

sm_os.h:
	ln -sf ${SENDMAIL_DIR}/include/sm/os/sm_os_freebsd.h sm_os.h

.include <bsd.prog.mk>
