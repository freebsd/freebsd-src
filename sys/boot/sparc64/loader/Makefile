# $FreeBSD$

LOADERBASE=	0x100000

BASE=		loader
PROG=		${BASE}
NEWVERSWHAT=	"bootstrap loader" sparc64

# Architecture-specific loader code
SRCS=		locore.s main.c
CFLAGS=		-mno-app-regs -nostdinc

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	<${.CURDIR}/../../common/Makefile.inc>
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I${.CURDIR}/../../.. -I.

CLEANFILES+=	${PROG}

CFLAGS+=	-W -Wall
LDFLAGS=	-X -N -Ttext ${LOADERBASE} -e _start -static

# where to get libstand from
#XXX need a better way to do this
LIBSTAND=	${.CURDIR}/../../../../lib/libstand/libstand.a
.if !exists(${LIBSTAND})
LIBSTAND=	${.OBJDIR}/../../../../lib/libstand/libstand.a
.if !exists(${LIBSTAND})
LIBSTAND=	-lstand
.endif
.endif
LIBOFW=		${.CURDIR}/../../ofw/libofw/libofw.a
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/
CFLAGS+=	-I${.CURDIR}/../../ofw/libofw/

CFLAGS+=	-elf -DLOADERBASE=${LOADERBASE}

# Debug me!
#CFLAGS+=	-g
#LDFLAGS+=	-g

${PROG}:	${OBJS}
	${LD} -o ${.TARGET} ${OBJS} ${LIBOFW} ${LIBSTAND} ${LDFLAGS}

locore.o:	locore.s
	${CC} ${CFLAGS} -D_LOCORE -xassembler-with-cpp -c locore.s -o locore.o
main.o:		main.c
	${CC} ${CFLAGS} -c main.c -o main.o

beforeinstall:
.if exists(${DESTDIR}/boot/loader)
	mv ${DESTDIR}/boot/loader ${DESTDIR}/boot/loader.old
.endif
.if exists(${.OBJDIR}/loader.help)
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.OBJDIR}/${BASE}.help ${DESTDIR}/boot
.else
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/${BASE}.help ${DESTDIR}/boot
.endif
.if !exists(${DESTDIR}/boot/loader.rc)
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/../../forth/loader.rc ${DESTDIR}/boot
.endif
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/../../forth/loader.4th ${DESTDIR}/boot
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/../../forth/support.4th ${DESTDIR}/boot
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/../../forth/loader.conf ${DESTDIR}/boot/defaults

# There are no things relevant to all boot parts of FreeBSD/sparc64 yet.
#.include <${.CURDIR}/../Makefile.inc>

.include <bsd.prog.mk>
