PROG=	sysinstall
MAN8=	sysinstall.8

BINDIR=/stand
NOSHARED=YES

CLEANFILES+=	makedevs.c rtermcap rtermcap.tmp dumpnlist
CLEANFILES+=	keymap.tmp keymap.h pccard_conf.h

.PATH: ${.CURDIR}/../disklabel ${.CURDIR}/../../usr.bin/cksum

SRCS=	anonFTP.c cdrom.c command.c config.c devices.c dhcp.c kget.c \
	disks.c dispatch.c dist.c dmenu.c doc.c dos.c floppy.c \
	ftp.c globals.c index.c install.c installUpgrade.c keymap.c \
	label.c lndir.c main.c makedevs.c media.c menus.c misc.c mouse.c \
	msg.c network.c nfs.c options.c package.c pccard.c \
	system.c tape.c tcpip.c termcap.c ufs.c user.c variable.c wizard.c \
	keymap.h pccard_conf.h

CFLAGS+= -Wall -I${.CURDIR}/../../gnu/lib/libdialog -I${.OBJDIR}
CFLAGS+= -I${.CURDIR}/../../sys

PCCARD?= NO

DPADD=	${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO} ${LIBUTIL} ${LIBDISK} ${LIBFTPIO}
LDADD=	-ldialog -lncurses -lmytinfo -lutil -ldisk -lftpio

pccard_conf.h:
.if ${PCCARD} == "YES"
	echo "#define PCCARD 1" > pccard_conf.h
.else
	echo "#undef PCCARD" > pccard_conf.h
.endif

makedevs.c:	Makefile rtermcap keymap.h
	rm -f makedevs.tmp
	echo '#include <sys/types.h>' > makedevs.tmp
	./rtermcap ansi | \
		file2c 'const char termcap_ansi[] = {' ',0};' \
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
.if ${MACHINE_ARCH} != "alpha"
	file2c 'u_char boot0[] = {' '};' < /boot/boot0 >> makedevs.tmp
	file2c 'u_char mbr[] = {' '};' < /boot/mbr >> makedevs.tmp
.endif
	mv makedevs.tmp makedevs.c

rtermcap:	${.CURDIR}/rtermcap.c
	${CC} -o rtermcap ${.CURDIR}/rtermcap.c -ltermcap


KEYMAPS= be.iso br275.iso danish.iso finnish.iso fr.iso \
	german.iso hr.iso hu.iso2.101keys it.iso icelandic.iso jp.106 \
	norwegian.iso pl_PL.ISO_8859-2 pt.iso ru.koi8-r si.iso \
	spanish.iso swedish.iso swissfrench.iso swissgerman.iso uk.iso \
	us.dvorak us.iso


keymap.h:
	rm -f keymap.tmp
	for map in ${KEYMAPS} ; do \
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
