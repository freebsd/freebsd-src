#	@(#)Makefile	5.11 (Berkeley) 2/19/91

PROG=	passwd
SRCS=	local_passwd.c passwd.c pw_copy.c pw_util.c yp_passwd.c getpwent.c \
	krb_passwd.c pw_fastmkdb.c
CFLAGS+=-I${.CURDIR} -DYP
.PATH:  ${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../rlogin ${.CURDIR}/../../lib/libc/gen

DPADD+= ${LIBRPCSVC}
LDADD+= -lrpcsvc

.if exists(${DESTDIR}/usr/lib/libcrypt.a)
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.endif
.if exists(${DESTDIR}/usr/lib/libkrb.a)
CFLAGS+= -DKERBEROS
DPADD+= ${LIBKRB}
LDADD+= -lkrb -ldes
.endif
.if defined (PW_COMPACT)
CFLAGS+=-DPW_COMPACT
.endif

getpwent.o: getpwent.c
	${CC} ${CFLAGS} -UYP -c ${.IMPSRC}

BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
