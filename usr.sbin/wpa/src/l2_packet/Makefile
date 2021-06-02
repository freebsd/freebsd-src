.include <src.opts.mk>

.include "../../Makefile.inc"

.include "../../Makefile.crypto"

LIB=	wpal2_packet
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/l2_packet

SRCS=	l2_packet_freebsd.c

CFLAGS+=-DIEEE8021X_EAPOL

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
