PROG = sysinstall
MANEXT = 1
NOMAN= yet
CLEANFILES=	makedevs.c rtermcap

.PATH: ${.CURDIR}/../disklabel

SRCS =	exec.c dkcksum.c label.c main.c mbr.c \
	stage0.c stage1.c stage2.c stage3.c stage5.c \
	termcap.c utils.c makedevs.c ourcurses.c

CFLAGS += -Wall -g -static
LDADD =  -ldialog -lncurses -lmytinfo
DPADD = ${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}

.if exists(${.CURDIR}/../../sys/i386/boot/biosboot/obj)
BOOTS=${.CURDIR}/../../sys/i386/boot/biosboot/obj
.else
BOOTS=${.CURDIR}/../../sys/i386/boot/biosboot
.endif

makedevs.c:	dev2c.sh Makefile rtermcap
	mkdir -p dev
	cp ${.CURDIR}/../../etc/etc.i386/MAKEDEV dev
	( cd dev; sh ./MAKEDEV all )
	sh ${.CURDIR}/dev2c.sh dev > makedevs.tmp
	rm -rf dev
	uudecode < ${.CURDIR}/bteasy17.uu
	file2c 'const unsigned char boot0[] = {' '};' \
		< bteasy17 >> makedevs.tmp
	rm -rf bteasy17
	file2c 'const unsigned char boot1[] = {' '};' \
		< ${BOOTS}/boot1 >> makedevs.tmp
	file2c 'const unsigned char boot2[] = {' '};' \
		< ${BOOTS}/boot2 >> makedevs.tmp
	./rtermcap cons25 | \
		file2c 'const char termcap_cons25[] = {' ',0};' \
		>> makedevs.tmp
	./rtermcap cons25-m | \
		file2c 'const char termcap_cons25_m[] = {' ',0};' \
		>> makedevs.tmp
	./rtermcap vt100 | \
		file2c 'const char termcap_vt100[] = {' ',0};' \
		>> makedevs.tmp
	mv makedevs.tmp makedevs.c

rtermcap:	${.CURDIR}/rtermcap.c
	${CC} -o rtermcap ${.CURDIR}/rtermcap.c -ltermcap

.include <bsd.prog.mk>

