PROG = sysinstall
MANEXT = 1
NOMAN= yet

.PATH: /usr/src/sbin/disklabel

SRCS =	bootarea.c editor.c exec.c dkcksum.c label.c main.c mbr.c \
	stage0.c stage1.c stage2.c stage3.c stage4.c stage5.c \
	termcap.c utils.c makedevs.c ourcurses.c

CFLAGS += -Wall -g -static -I${.CURDIR}/../../sys/
CLEANFILES += makedevs.c
LDADD =  -ldialog -lncurses -lmytinfo
DPADD = ${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}

makedevs.c:	dev2c.sh
	sh ${.CURDIR}/dev2c.sh ${DESTDIR}/dev > makedevs.c ; \

.include <bsd.prog.mk>

