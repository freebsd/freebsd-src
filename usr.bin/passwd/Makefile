#	@(#)Makefile	5.11 (Berkeley) 2/19/91

PROG=	passwd
SRCS=	local_passwd.c passwd.c pw_copy.c pw_util.c yp_passwd.c getpwent.c
CFLAGS+=-I${.CURDIR} -DYP
.PATH:  ${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../rlogin ${.CURDIR}/../../lib/libc/gen

DPADD+= ${LIBRPCSVC} #${LIBKRB}
LDADD+= -lrpcsvc #-lkrb

.if exists(/usr/lib/libcrypt.a)
#CFLAGS+=-DKERBEROS
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.endif

getpwent.o: getpwent.c
	${CC} ${CFLAGS} -UYP -c ${.IMPSRC}

BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
