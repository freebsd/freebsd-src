CFLAGS+= -DCAP -DHAS_NCURSES -DHAS_UNISTD -DHAS_STDARG -DHAS_STDLIB \
	 -DHAS_CTYPE -DHAS_SYS_IOCTL -DHAS_SYS_WAIT -DSLCT_HDR

PROG=	ee
SRCS=	ee.c
LINKS=	${BINDIR}/ee ${BINDIR}/ree
DPADD=	ee.cat ${LIBNCURSES} ${LIBMYTINFO}
LDADD=	-lncurses -lmytinfo
CLEANFILES+=	ee.cat

# I'm not sure if this is kosher, but I don't see any system default just
# now.
LANG?=	en_US.ISO_8859-1

ee.cat:	${.CURDIR}/ee.msg
	gencat -new ee.cat ${.CURDIR}/ee.msg

afterinstall: ee.cat
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ee.cat \
		${DESTDIR}/usr/share/nls/${LANG}/ee.cat

.include <bsd.prog.mk>
