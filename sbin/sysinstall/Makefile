PROG = sysinstall
MANEXT = 1
NOMAN= yet

.PATH: /usr/src/sbin/disklabel

SRCS =	stage1.c dkcksum.c bootarea.c mbr.c \
	utils.c termcap.c exec.c \
	stage0.c stage2.c stage3.c main.c

CFLAGS += -Wall
LDADD =  -ldialog -lncurses -lmytinfo
DPADD = ${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}

.include <bsd.prog.mk>
