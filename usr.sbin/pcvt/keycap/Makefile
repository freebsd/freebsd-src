.if !exists(${.CURDIR}/../Makefile.inc)
error:
	@echo
	@echo "                      You MUST link/copy"
	@echo
	@echo "       ../Makefile.inc.NetBSD    or   ../Makefile.inc.FreeBSD"
	@echo
	@echo "             to ../Makefile.inc before you can proceed !"
	@echo
.else

LIB	=	keycap
CAPDIR	=	/usr/share/misc
CAPPATH	=	$(CAPDIR)/keycap.pcvt
KEYCAPSRC=	keycap.src
CFLAGS	+=	-DKEYCAP_PATH=\"$(CAPPATH)\"
SRCS	=	keycap.c
MAN3	=	keycap.${MAN3EXT}
MLINKS+=	keycap.${MAN3EXT} kgetent.${MAN3EXT} \
		keycap.${MAN3EXT} kgetnum.${MAN3EXT} \
		keycap.${MAN3EXT} kgetflag.${MAN3EXT} \
		keycap.${MAN3EXT} kgetstr.${MAN3EXT}
MAN5	=	man5/keycap.${MAN5EXT}
#CLEANFILES+=	keycap.0 man5/keycap.0

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${.CURDIR}/${KEYCAPSRC} ${DESTDIR}${CAPPATH}

.include <bsd.lib.mk>

.endif
