PROG = sysinstall
MANEXT = 1
NOMAN= yet

.PATH: /usr/src/sbin/disklabel

SRCS = bootarea.c exec.c dkcksum.c label.c \
       main.c mbr.c stage0.c stage1.c \
		 stage2.c stage3.c termcap.c utils.c

CFLAGS += -Wall
LDADD =  -ldialog -lncurses -lmytinfo
DPADD = ${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}

.include <bsd.prog.mk>
