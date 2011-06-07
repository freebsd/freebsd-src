# $FreeBSD$
# Copyright (c) David E. O'Brien, 2000-2004

CONTRDIR=	${.CURDIR}/../../contrib/file
.PATH: ${CONTRDIR}

LIB=	magic
SHLIB_MAJOR=	4
DPADD=	${LIBZ}
LDADD=	-lz
MAN=	libmagic.3 magic.5

SRCS=	apprentice.c apptype.c ascmagic.c cdf.c cdf_time.c compress.c \
	encoding.c fsmagic.c funcs.c \
	is_tar.c magic.c print.c readcdf.c readelf.c softmagic.c
INCS=	magic.h

MAGICPATH?=	/usr/share/misc

CFLAGS+= -DMAGIC='"${MAGICPATH}/magic"' -DHAVE_CONFIG_H
CFLAGS+= -I${.CURDIR} -I${CONTRDIR}

WARNS?=		3

CLEANFILES+=	magic magic.mgc

FILES=		magic magic.mgc
FILESDIR=	${MAGICPATH}

MAGFILES=	${CONTRDIR}/Header\
		${CONTRDIR}/Localstuff\
		${CONTRDIR}/Magdir/[a-z]*

magic: ${MAGFILES}
	cat ${.ALLSRC} > ${.TARGET}

magic.mgc: mkmagic magic
	./mkmagic magic

CLEANFILES+=	mkmagic
build-tools: mkmagic
mkmagic: apprentice.c funcs.c magic.c print.c
	${CC} ${CFLAGS} -DCOMPILE_ONLY ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

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
