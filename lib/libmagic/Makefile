# $FreeBSD$
# Copyright (c) David E. O'Brien, 2000-2004

CONTRDIR=	${.CURDIR}/../../contrib/file
.PATH: ${CONTRDIR}

LIB=	magic
SHLIB_MAJOR=	1
MAN=	libmagic.3 magic.5

SRCS=	apprentice.c apptype.c ascmagic.c compress.c fsmagic.c funcs.c \
	is_tar.c magic.c print.c readelf.c softmagic.c
INCS=	magic.h

MAGICPATH?=	/usr/share/misc

CFLAGS+= -DMAGIC='"${MAGICPATH}/magic"' -DBUILTIN_ELF -DELFCORE -DHAVE_CONFIG_H
CFLAGS+= -I${.CURDIR} -I${CONTRDIR}

CLEANFILES+=	magic magic.mgc magic.mime.mgc

FILES=		magic magic.mgc ${CONTRDIR}/magic.mime magic.mime.mgc
FILESDIR=	${MAGICPATH}

MAGFILES=	${CONTRDIR}/Header\
		${CONTRDIR}/Localstuff\
		${CONTRDIR}/Magdir/[a-z]*

magic: ${MAGFILES}
	cat ${.ALLSRC} > ${.TARGET}

magic.mgc: mkmagic magic
	./mkmagic magic

magic.mime.mgc: mkmagic magic.mime
	./mkmagic ${CONTRDIR}/magic.mime

CLEANFILES+=	mkmagic
build-tools: mkmagic
mkmagic: apprentice.c funcs.c magic.c print.c
	${CC} -DHAVE_CONFIG_H -DCOMPILE_ONLY \
	    -I${.CURDIR} -I${CONTRDIR} -o ${.TARGET} ${.ALLSRC}

FILEVER!= awk '$$1 == "\#define" && $$2 == "VERSION" { print $$3; exit }' \
			${.CURDIR}/config.h
CLEANFILES+=	${MAN}
.for mp in ${MAN}
${mp}: ${mp:C/[0-9]/man/}
	sed -e 's/__FSECTION__/5/g' -e 's/__CSECTION__/1/g' \
		-e 's/__VERSION__/${FILEVER}/g' \
		-e 's,__MAGIC__,${MAGICPATH}/magic,g' ${.ALLSRC} > ${.TARGET}
.endfor

.include <bsd.lib.mk>
