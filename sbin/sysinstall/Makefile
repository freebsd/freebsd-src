PROG = sysinstall
BINDIR = sbin
MANEXT = 1
NOMAN= yet

.PATH: /usr/src/sbin/disklabel

SRCS = sysinstall.c dkcksum.c

CFLAGS += -Wall
LDADD =  -ldialog -lncurses -lmytinfo
DPADD = ${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}

.include <bsd.prog.mk>
