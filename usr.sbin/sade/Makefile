# $FreeBSD$

.if ${MACHINE_ARCH} != "ia64"
_wizard=	wizard.c
.endif

PROG=	sysinstall
MAN=	sysinstall.8
SRCS=	anonFTP.c cdrom.c command.c config.c devices.c dhcp.c \
	disks.c dispatch.c dist.c dmenu.c doc.c dos.c floppy.c \
	ftp.c globals.c http.c index.c install.c installUpgrade.c keymap.c \
	label.c main.c makedevs.c media.c menus.c misc.c modules.c \
	mouse.c msg.c network.c nfs.c options.c package.c \
	system.c tape.c tcpip.c termcap.c ttys.c ufs.c user.c \
	variable.c ${_wizard} keymap.h

CFLAGS+= -DUSE_GZIP=1
.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif
CFLAGS+= -I${.CURDIR}/../../gnu/lib/libdialog -I.

DPADD=	${LIBDIALOG} ${LIBNCURSES} ${LIBUTIL} ${LIBDISK} ${LIBFTPIO}
LDADD=	-ldialog -lncurses -lutil -ldisk -lftpio

CLEANFILES=	makedevs.c rtermcap
CLEANFILES+=	keymap.tmp keymap.h

.if exists(${.CURDIR}/../../share/termcap/termcap.src)
RTERMCAP=	TERMCAP=${.CURDIR}/../../share/termcap/termcap.src ./rtermcap
.else
RTERMCAP=	./rtermcap
.endif

makedevs.c:	Makefile rtermcap
	echo '#include <sys/types.h>' > makedevs.c
	${RTERMCAP} ansi | \
		file2c 'const char termcap_ansi[] = {' ',0};' \
		>> makedevs.c
	${RTERMCAP} cons25w | \
		file2c 'const char termcap_cons25w[] = {' ',0};' \
		>> makedevs.c
	${RTERMCAP} cons25 | \
		file2c 'const char termcap_cons25[] = {' ',0};' \
		>> makedevs.c
	${RTERMCAP} cons25-m | \
		file2c 'const char termcap_cons25_m[] = {' ',0};' \
		>> makedevs.c
	${RTERMCAP} cons25r | \
		file2c 'const char termcap_cons25r[] = {' ',0};' \
		>> makedevs.c
	${RTERMCAP} cons25r-m | \
		file2c 'const char termcap_cons25r_m[] = {' ',0};' \
		>> makedevs.c
	${RTERMCAP} cons25l1 | \
		file2c 'const char termcap_cons25l1[] = {' ',0};' \
		>> makedevs.c
	${RTERMCAP} cons25l1-m | \
		file2c 'const char termcap_cons25l1_m[] = {' ',0};' \
		>> makedevs.c
	${RTERMCAP} vt100 | \
		file2c 'const char termcap_vt100[] = {' ',0};' \
		>> makedevs.c
	${RTERMCAP} xterm | \
		file2c 'const char termcap_xterm[] = {' ',0};' \
		>> makedevs.c

build-tools:	rtermcap

rtermcap:	rtermcap.c
	${CC} -o ${.TARGET} ${.ALLSRC} -ltermcap

.if ${MACHINE} == "pc98"
KEYMAPS= jp.pc98 jp.pc98.iso
.else
KEYMAPS= be.iso bg.bds.ctrlcaps bg.phonetic.ctrlcaps br275.iso \
	ce.iso2 cs.latin2.qwertz danish.iso el.iso07 \
	estonian.cp850 estonian.iso estonian.iso15 finnish.iso fr.iso \
	german.iso gr.elot.acc gr.us101.acc  hr.iso hu.iso2.101keys \
	it.iso icelandic.iso jp.106 norwegian.iso pl_PL.ISO8859-2 \
	pt.iso ru.koi8-r si.iso sk.iso2 spanish.iso swedish.iso \
	swissfrench.iso \
	swissgerman.iso ua.koi8-u ua.koi8-u.shift.alt uk.iso us.dvorak \
	us.iso us.pc-ctrl us.unix
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
