# $FreeBSD$

.if ${MACHINE_ARCH} != "ia64"
_wizard=	wizard.c
.endif

PROG=	sade
MAN=	sade.8
SRCS=	command.c config.c devices.c \
	disks.c dispatch.c dmenu.c \
	globals.c install.c keymap.c \
	label.c main.c menus.c misc.c \
	msg.c system.c termcap.c \
	variable.c ${_wizard} keymap.h
WARNS?=	3

# command.c

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif
CFLAGS+= -I${.CURDIR}/../../gnu/lib/libdialog -I.

DPADD=	${LIBDIALOG} ${LIBNCURSES} ${LIBUTIL} ${LIBDISK} ${LIBFTPIO}
LDADD=	-ldialog -lncurses -lutil -ldisk -lftpio

CLEANFILES=	keymap.tmp keymap.h

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
	( echo "	{ NULL, NULL }"; echo "};" ; echo "" ) >> keymap.tmp
	mv keymap.tmp keymap.h

.include <bsd.prog.mk>
