# $FreeBSD$

PROG=	sysinstall
MAN=	sysinstall.8
SRCS=	anonFTP.c cdrom.c command.c config.c devices.c dhcp.c \
	disks.c dispatch.c dist.c dmenu.c doc.c dos.c floppy.c \
	ftp.c globals.c http.c index.c install.c installUpgrade.c keymap.c \
	label.c main.c makedevs.c media.c menus.c misc.c modules.c \
	mouse.c msg.c network.c nfs.c options.c package.c pccard.c \
	system.c tape.c tcpip.c termcap.c ttys.c ufs.c usb.c user.c \
	variable.c wizard.c keymap.h

CFLAGS+= -I${.CURDIR}/../../gnu/lib/libdialog -I.
.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

DPADD=	${LIBDIALOG} ${LIBNCURSES} ${LIBUTIL} ${LIBDISK} ${LIBFTPIO}
LDADD=	-ldialog -lncurses -lutil -ldisk -lftpio

CLEANFILES=	makedevs.c rtermcap rtermcap.tmp dumpnlist
CLEANFILES+=	keymap.tmp keymap.h

makedevs.c:	Makefile rtermcap
	echo '#include <sys/types.h>' > makedevs.c
	./rtermcap ansi | \
		file2c 'const char termcap_ansi[] = {' ',0};' \
		>> makedevs.c
	./rtermcap cons25w | \
		file2c 'const char termcap_cons25w[] = {' ',0};' \
		>> makedevs.c
	./rtermcap cons25 | \
		file2c 'const char termcap_cons25[] = {' ',0};' \
		>> makedevs.c
	./rtermcap cons25-m | \
		file2c 'const char termcap_cons25_m[] = {' ',0};' \
		>> makedevs.c
	./rtermcap cons25r | \
		file2c 'const char termcap_cons25r[] = {' ',0};' \
		>> makedevs.c
	./rtermcap cons25r-m | \
		file2c 'const char termcap_cons25r_m[] = {' ',0};' \
		>> makedevs.c
	./rtermcap cons25l1 | \
		file2c 'const char termcap_cons25l1[] = {' ',0};' \
		>> makedevs.c
	./rtermcap cons25l1-m | \
		file2c 'const char termcap_cons25l1_m[] = {' ',0};' \
		>> makedevs.c
	./rtermcap vt100 | \
		file2c 'const char termcap_vt100[] = {' ',0};' \
		>> makedevs.c
	./rtermcap xterm | \
		file2c 'const char termcap_xterm[] = {' ',0};' \
		>> makedevs.c

.if ${MACHINE_ARCH} == i386
# XXX boot images aren't yet ready when "make depend" is run
.if !make(depend)
SRCS+=	makeboot.c
.endif
CLEANFILES+= makeboot.c
.if exists(${.OBJDIR}/../../sys/boot/${MACHINE}/boot0/boot0)
BOOT0=	${.OBJDIR}/../../sys/boot/${MACHINE}/boot0/boot0
.else
BOOT0=	/boot/boot0
.endif
makeboot.c: ${BOOT0}
.if ${MACHINE} == "i386"
.if exists(${.OBJDIR}/../../sys/boot/i386/mbr/mbr)
MBR=	${.OBJDIR}/../../sys/boot/i386/mbr/mbr
.else
MBR=	/boot/mbr
.endif
makeboot.c: ${MBR}
.elif ${MACHINE} == "pc98"
.if exists(${.OBJDIR}/../../sys/boot/pc98/boot0.5/boot0.5)
BOOT05=	${.OBJDIR}/../../sys/boot/pc98/boot0.5/boot0.5
.else
BOOT05=	/boot/boot0.5
.endif
makeboot.c: ${BOOT05}
.endif
makeboot.c: Makefile
	echo '#include <sys/types.h>' > makeboot.c
	file2c 'u_char boot0[] = {' '};' < ${BOOT0} >> makeboot.c
	echo "size_t boot0_size = sizeof(boot0);" >> makeboot.c
.if ${MACHINE} == i386
	file2c 'u_char mbr[] = {' '};' < ${MBR} >> makeboot.c
	echo "size_t mbr_size = sizeof(mbr);" >> makeboot.c
.elif ${MACHINE} == "pc98"
	file2c 'u_char boot05[] = {' '};' < ${BOOT05} >> makeboot.c
	echo "size_t boot05_size = sizeof(boot05);" >> makeboot.c
.endif
.endif

build-tools:	rtermcap

rtermcap:	rtermcap.c
	${CC} -o ${.TARGET} ${.ALLSRC} -ltermcap

.if ${MACHINE} == "pc98"
KEYMAPS= jp.pc98 jp.pc98.iso
.else
KEYMAPS= be.iso bg.bds.ctrlcaps bg.phonetic.ctrlcaps br275.iso \
	cs.latin2.qwertz danish.iso finnish.iso fr.iso \
	german.iso hr.iso hu.iso2.101keys it.iso icelandic.iso jp.106 \
	norwegian.iso pl_PL.ISO8859-2 pt.iso ru.koi8-r si.iso \
	spanish.iso swedish.iso swissfrench.iso swissgerman.iso ua.koi8-u \
	ua.koi8-u.shift.alt uk.iso us.dvorak us.iso us.pc-ctrl us.unix
.endif

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
