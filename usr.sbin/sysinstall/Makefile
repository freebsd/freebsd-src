# $FreeBSD$

PROG=	sysinstall
MAN=	sysinstall.8

CLEANFILES+=	makedevs.c rtermcap rtermcap.tmp dumpnlist
CLEANFILES+=	keymap.tmp keymap.h

SRCS=	anonFTP.c cdrom.c command.c config.c devices.c dhcp.c kget.c \
	disks.c dispatch.c dist.c dmenu.c doc.c dos.c floppy.c \
	ftp.c globals.c http.c index.c install.c installUpgrade.c keymap.c \
	label.c main.c makedevs.c media.c menus.c misc.c modules.c \
	mouse.c msg.c network.c nfs.c options.c package.c pccard.c \
	system.c tape.c tcpip.c termcap.c ufs.c usb.c user.c variable.c \
	wizard.c keymap.h

CFLAGS+= -Wall -I${.CURDIR}/../../gnu/lib/libdialog -I.
.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

DPADD=	${LIBDIALOG} ${LIBNCURSES} ${LIBUTIL} ${LIBDISK} ${LIBFTPIO}
LDADD=	-ldialog -lncurses -lutil -ldisk -lftpio

makedevs.c:	Makefile rtermcap
	rm -f makedevs.tmp
	echo '#include <sys/types.h>' > makedevs.tmp
	./rtermcap ansi | \
		file2c 'const char termcap_ansi[] = {' ',0};' \
		>> makedevs.tmp
	./rtermcap cons25w | \
		file2c 'const char termcap_cons25w[] = {' ',0};' \
		>> makedevs.tmp
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
	./rtermcap xterm | \
		file2c 'const char termcap_xterm[] = {' ',0};' \
		>> makedevs.tmp
.if ${MACHINE} == "i386"
	file2c 'u_char boot0[] = {' '};' < /boot/boot0 >> makedevs.tmp
	echo "size_t boot0_size = sizeof(boot0);" >> makedevs.tmp
	file2c 'u_char mbr[] = {' '};' < /boot/mbr >> makedevs.tmp
	echo "size_t mbr_size = sizeof(mbr);" >> makedevs.tmp
.endif
.if ${MACHINE} == "pc98"
	file2c 'u_char boot0[] = {' '};' < /boot/boot0 >> makedevs.tmp
	echo "size_t boot0_size = sizeof(boot0);" >> makedevs.tmp
	file2c 'u_char boot05[] = {' '};' < /boot/boot0.5 >> makedevs.tmp
	echo "size_t boot05_size = sizeof(boot05);" >> makedevs.tmp
.endif
	mv makedevs.tmp makedevs.c

build-tools:	rtermcap

rtermcap:	rtermcap.c
	${CC} -o ${.TARGET} ${.ALLSRC} -ltermcap


KEYMAPS= be.iso br275.iso danish.iso finnish.iso fr.iso \
	german.iso hr.iso hu.iso2.101keys it.iso icelandic.iso jp.106 \
	norwegian.iso pl_PL.ISO_8859-2 pt.iso ru.koi8-r si.iso \
	spanish.iso swedish.iso swissfrench.iso swissgerman.iso ua.koi8-u \
	ua.koi8-u.shift.alt uk.iso us.dvorak us.iso us.pc-ctrl us.unix

keymap.h:
	rm -f keymap.tmp
	for map in ${KEYMAPS} ; do \
		KEYMAP_PATH=${.CURDIR}/../../share/syscons/keymaps \
			kbdcontrol -L $$map | \
			sed -e '/^static accentmap_t/,$$d' >> keymap.tmp ; \
	done
	echo "static struct keymapInfo keymapInfos[] = {" >> keymap.tmp
	for map in ${KEYMAPS} ; do \
		echo -n '	{ "'$$map'", ' >> keymap.tmp ; \
		echo "&keymap_$$map }," | tr '[-.]' '_' >> keymap.tmp ; \
	done
	( echo "	{ 0 }"; echo "};" ; echo "" ) >> keymap.tmp
	mv keymap.tmp keymap.h

.include <bsd.prog.mk>
