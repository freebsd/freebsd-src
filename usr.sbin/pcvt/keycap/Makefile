
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
MAN5	=	man5/keycap.${MAN5EXT}
#CLEANFILES+=	keycap.0 man5/keycap.0

beforeinstall:
	${INSTALL} -c -m ${LIBMODE} -o ${LIBOWN} -g ${LIBGRP} \
	${.CURDIR}/${KEYCAPSRC} ${DESTDIR}${CAPPATH}

.include <bsd.lib.mk>

.endif
