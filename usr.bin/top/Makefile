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

DPADD=  ${LIBCURSES} ${LIBTERMCAP} ${LIBM} ${LIBKVM}
LDADD=  -lcurses -ltermcap -lm -lkvm
BINGRP= kmem
BINMODE=2555

.include <bsd.prog.mk>
