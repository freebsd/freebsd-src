#	$Id$

LIB=	intl
INTERNALLIB= true
INTERNALSTATICLIB= true

CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -DLOCALEDIR=\"/usr/share/locale\"
CFLAGS+= -DGNULOCALEDIR=\"/usr/share/locale\"
CFLAGS+= -DLOCALE_ALIAS_PATH=\"/usr/share/locale:.\"
CFLAGS+= -I${TXIDIR} -I${TXIDIR}/lib -I${TXIDIR}/intl -I.

SRCS=	bindtextdom.c dcgettext.c dgettext.c gettext.c
SRCS+=	finddomain.c loadmsgcat.c localealias.c textdomain.c
SRCS+=	l10nflist.c explodename.c intl-compat.c libintl.h

CLEANFILES+= libintl.h

libintl.h: libgettext.h
	@ln -sf ${.OODATE} ${.TARGET}

.include <bsd.lib.mk>

.PATH: ${TXIDIR}/intl
