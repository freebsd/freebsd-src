PROG=	sysinstall
NOMAN=	yes
CLEANFILES=	makedevs.c rtermcap

.PATH: ${.CURDIR}/../disklabel

SRCS=	globals.c	main.c		dmenu.c			\
	menus.c		misc.c		msg.c			\
	system.c	install.c	termcap.c		\
	media.c		variable.c	devices.c		\
	dist.c		lang.c		wizard.c		\
	disks.c		command.c	decode.c		\
	label.c		tcpip.c		media_strategy.c	\
	makedevs.c	ftp.c		config.c

CFLAGS+=	-Wall -g -I${.CURDIR}/../libdisk \
		-I${.CURDIR}/../../gnu/lib/libdialog

LDADD=  -ldialog -lncurses -lmytinfo
.if exists(${.CURDIR}/../libdisk/obj)
LDADD+= -L${.CURDIR}/../libdisk/obj -ldisk
.else
LDADD+= -L${.CURDIR}/../libdisk -ldisk
.endif

DPADD= ${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}

.if exists(${.CURDIR}/../../sys/i386/boot/biosboot/obj)
BOOTS=${.CURDIR}/../../sys/i386/boot/biosboot/obj
.else
BOOTS=${.CURDIR}/../../sys/i386/boot/biosboot
.endif

.if exists(${.CURDIR}/../../share/syscons/scrnmaps/obj)
MKSCRNMAP=${.CURDIR}/../../share/syscons/scrnmaps/obj/koi8-r2cp866.mk
.else
MKSCRNMAP=${.CURDIR}/../../share/syscons/scrnmaps/koi8-r2cp866.mk
.endif

makedevs.c:	dev2c.sh Makefile rtermcap
	rm -f makedevs.tmp
	mkdir -p dev
	echo '#include <sys/types.h>' > makedevs.tmp
	cp ${.CURDIR}/../../etc/etc.i386/MAKEDEV dev
	( cd dev; sh ./MAKEDEV all )
	sh ${.CURDIR}/dev2c.sh dev > makedevs.tmp
	rm -rf dev
	uudecode < ${.CURDIR}/bteasy17.uu
	file2c 'const unsigned char boot0[] = {' '};' \
		< bteasy17 >> makedevs.tmp
	rm -rf bteasy17
	./rtermcap cons25 | \
		file2c 'const char termcap_cons25[] = {' ',0};' \
		>> makedevs.tmp
	./rtermcap cons25-m | \
		file2c 'const char termcap_cons25_m[] = {' ',0};' \
		>> makedevs.tmp
	./rtermcap cons25r | \
		file2c 'const char termcap_cons25r[] = {' ',0};' \
		>> makedevs.tmp
	./rtermcap cons25r-m | \
		file2c 'const char termcap_cons25r_m[] = {' ',0};' \
		>> makedevs.tmp
	./rtermcap cons25l1 | \
		file2c 'const char termcap_cons25l1[] = {' ',0};' \
		>> makedevs.tmp
	./rtermcap cons25l1-m | \
		file2c 'const char termcap_cons25l1_m[] = {' ',0};' \
		>> makedevs.tmp
	./rtermcap vt100 | \
		file2c 'const char termcap_vt100[] = {' ',0};' \
		>> makedevs.tmp
	uudecode < ${.CURDIR}/../../share/syscons/fonts/iso-8x16.fnt \
		&& file2c 'const u_char font_iso_8x16[] = {' '};' \
		< iso-8x16 >> makedevs.tmp
	rm iso-8x16
	uudecode < ${.CURDIR}/../../share/syscons/fonts/cp850-8x16.fnt \
		&& file2c 'const u_char font_cp850_8x16[] = {' '};' \
		< cp850-8x16 >> makedevs.tmp
	rm cp850-8x16
	uudecode < ${.CURDIR}/../../share/syscons/fonts/cp866-8x16.fnt \
		&& file2c 'const u_char font_cp866_8x16[] = {' '};' \
		< cp866-8x16 >> makedevs.tmp
	rm cp866-8x16
	${MKSCRNMAP} koi8-r2cp866 \
		&& file2c 'const u_char koi8_r2cp866[] = {' '};' \
		< koi8-r2cp866 >> makedevs.tmp
	rm koi8-r2cp866
	mv makedevs.tmp makedevs.c

rtermcap:	${.CURDIR}/rtermcap.c
	${CC} -o rtermcap ${.CURDIR}/rtermcap.c -ltermcap

.include <bsd.prog.mk>

