PROG=	sysinstall
NOMAN=	yes
CLEANFILES=	makedevs.c rtermcap

.PATH: ${.CURDIR}/../disklabel

SRCS=	globals.c	main.c		dmenu.c		menus.c	\
	misc.c		msg.c		system.c	install.c \
	termcap.c	makedevs.c	media.c		variable.c \
	devices.c	dist.c		lang.c		wizard.c \
	disks.c		command.c

CFLAGS += -Wall -g -I${.CURDIR}/../libdisk

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

makedevs.c:	dev2c.sh Makefile rtermcap
#	mkdir -p dev
#	cp ${.CURDIR}/../../etc/etc.i386/MAKEDEV dev
#	( cd dev; sh ./MAKEDEV all )
#	sh ${.CURDIR}/dev2c.sh dev > makedevs.tmp
#	rm -rf dev
	rm -f makedevs.tmp
	echo '#include <sys/types.h>' > makedevs.tmp
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
	file2c 'const u_char font_iso_8x14[] = {' '};' \
		< ${.CURDIR}/../../share/syscons/fonts/iso-8x14.fnt \
		>> makedevs.tmp
	file2c 'const u_char font_cp850_8x14[] = {' '};' \
		< ${.CURDIR}/../../share/syscons/fonts/cp850-8x14.fnt \
		>> makedevs.tmp
	file2c 'const u_char font_koi8_r_8x14[] = {' '};' \
		< ${.CURDIR}/../../share/syscons/fonts/koi8-r-8x14.fnt \
		>> makedevs.tmp
	file2c 'const u_char koi8_r2cp866[] = {' '};' \
		< ${.CURDIR}/../../share/syscons/scrnmaps/koi8-r2cp866 \
		>> makedevs.tmp
	mv makedevs.tmp makedevs.c

rtermcap:	${.CURDIR}/rtermcap.c
	${CC} -o rtermcap ${.CURDIR}/rtermcap.c -ltermcap

.include <bsd.prog.mk>

