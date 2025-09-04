PACKAGE=	console-tools
PROG=		moused

SRCS=		moused.c \
		event-names.h \
		quirks.c \
		quirks.h \
		util.c \
		util.h \
		util-evdev.c \
		util-evdev.h \
		util-list.c \
		util-list.h
MAN=		moused.8 \
		moused.conf.5
CONFS=		moused.conf
QUIRKS=		5-generic-touchpad.quirks

CWARNFLAGS.quirks.c+=	-Wno-cast-align -Wno-shadow -Wno-cast-qual \
			-Wno-unused-variable -Wno-unused-parameter
CWARNFLAGS.util.c+=	-Wno-shadow
LIBADD=		m util
BINDIR=		/usr/sbin

FILES=		${QUIRKS:S|^|quirks/|}
FILESDIR=	/usr/share/${PROG}

.include <bsd.prog.mk>
