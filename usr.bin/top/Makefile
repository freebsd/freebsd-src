PROG=	top

TOPDIR=	${.CURDIR}/../../contrib/top
.PATH:	${TOPDIR}

CFLAGS+= -DHAVE_GETOPT -I${.CURDIR} -I${TOPDIR}

#
# The table size should be a prime number approximately twice as
# large as the number of lines in /etc/passwd.  The default number
# is 20011, use /etc/make.conf to override this.
#
.if defined(TOP_TABLE_SIZE)
CFLAGS+= -D"Table_size=${TOP_TABLE_SIZE}"
.endif

SRCS=	commands.c display.c machine.c screen.c top.c \
	username.c utils.c version.c

CLEANFILES+= top.1
DPADD=  ${LIBTERMCAP} ${LIBM} ${LIBKVM}
LDADD=  -ltermcap -lm -lkvm
BINGRP= kmem
BINMODE=2555

top.1:	${TOPDIR}/top.X top.local.1
	cat ${.ALLSRC} > top.1

.include <bsd.prog.mk>
