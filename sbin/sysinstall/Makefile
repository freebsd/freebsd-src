PROG = sysinstall
MANEXT = 1
NOMAN= yet

.PATH: ${.CURDIR}/../disklabel

SRCS =	exec.c dkcksum.c label.c main.c mbr.c \
	stage0.c stage1.c stage2.c stage3.c stage4.c stage5.c \
	termcap.c utils.c makedevs.c ourcurses.c

CFLAGS += -Wall -g -static
LDADD =  -ldialog -lncurses -lmytinfo
DPADD = ${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}

makedevs.c:	dev2c.sh
	mkdir -p dev
	cp ${.CURDIR}/../etc/etc.i386/MAKEDEV dev
	( cd dev; ./MAKEDEV all )
	sh ${.CURDIR}/dev2c.sh dev > makedevs.c
	rm -rf dev

.include <bsd.prog.mk>

