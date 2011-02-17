# $FreeBSD$

TOPDIR=	${.CURDIR}/../../contrib/top
.PATH: ${TOPDIR}

PROG=	top
SRCS=	commands.c display.c machine.c screen.c top.c \
	username.c utils.c version.c
SRCS+=	sigdesc.h top.local.h
CFLAGS+= -DHAVE_GETOPT -DHAVE_STRERROR -DORDER
CFLAGS+= -I${.CURDIR} -I${TOPDIR} -I.

WARNS?=	0

#
# The table size should be a prime number approximately twice as
# large as the number of lines in /etc/passwd.  The default number
# is 20011; use /etc/make.conf to override this.
#
.if defined(TOP_TABLE_SIZE)
CFLAGS+= -D"Table_size=${TOP_TABLE_SIZE}"
.endif

DPADD=	${LIBTERMCAP} ${LIBM} ${LIBKVM}
LDADD=	-ltermcap -lm -lkvm

CLEANFILES= sigdesc.h
SIGCONV_AWK= ${.CURDIR}/../../contrib/top/sigconv.awk
SIGNAL_H= ${DESTDIR}/usr/include/sys/signal.h
sigdesc.h: ${SIGCONV_AWK} ${SIGNAL_H}
	awk -f ${SIGCONV_AWK} < ${SIGNAL_H} > ${.TARGET}

CLEANFILES+= top.local.h top.x
.SUFFIXES: .X .x .H .h
.X.x .H.h:
	@${ECHO} Making ${.TARGET} from ${.IMPSRC}
	@sed -e's,%LoadMax%,5.0,g' \
	-e's,%TableSize%,20011,g' \
	-e's,%NominalTopn%,18,g' \
	-e's,%topn%,-1,g' \
	-e's,%delay%,2,g' \
	-e's,%random%,1,g' \
	${.IMPSRC} > ${.TARGET}

CLEANFILES+= top.1
top.1: top.x top.local.1
	cat ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
