PROG = sysinstall
MANEXT = 1
NOMAN= yet

.PATH: /usr/src/sbin/disklabel

SRCS =	sysinstall.c \
	dkcksum.c bootarea.c mbr.c \
	utils.c stage0.c stage3.c main.c

CFLAGS += -Wall
LDADD =  -ldialog -lncurses -lmytinfo
DPADD = ${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}

.include <bsd.prog.mk>
