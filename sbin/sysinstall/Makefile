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

makedevs.c:	dev2c.sh Makefile
	mkdir -p dev
	cp ${.CURDIR}/../../etc/etc.i386/MAKEDEV dev
	( cd dev; sh ./MAKEDEV all )
	sh ${.CURDIR}/dev2c.sh dev > makedevs.tmp
	rm -rf dev
	uudecode < ${.CURDIR}/bteasy17.uu
	file2c 'const char boot0[] = {' '};' < \
		bteasy17 \
		>> makedevs.tmp
	rm -rf bteasy17
	file2c 'const char boot1[] = {' '};' < \
		../../sys/i386/boot/biosboot/boot1 \
		>> makedevs.tmp
	file2c 'const char boot2[] = {' '};' < \
		../../sys/i386/boot/biosboot/boot2 \
		>> makedevs.tmp
	tset -Q -S cons25 | sed 's/^.* //' | \
		file2c 'const char termcap_cons25[] = {' ',0};' \
		>> makedevs.tmp
	tset -Q -S cons25-m | sed 's/^.* //' | \
		file2c 'const char termcap_cons25_m[] = {' ',0};' \
		>> makedevs.tmp
	tset -Q -S vt100 | sed 's/^.* //' | \
		file2c 'const char termcap_vt100[] = {' ',0};' \
		>> makedevs.tmp
	mv makedevs.tmp makedevs.c

.include <bsd.prog.mk>

