CFLAGS+= -DCAP -DHAS_NCURSES -DHAS_UNISTD -DHAS_STDARG -DHAS_STDLIB \
	 -DHAS_CTYPE -DHAS_SYS_IOCTL -DHAS_SYS_WAIT -DSLCT_HDR

PROG=	ee
SRCS=	ee.c
LINKS=	${BINDIR}/ee ${BINDIR}/ree
DPADD=	${LIBNCURSES} ${LIBMYTINFO}
LDADD=	-lncurses -lmytinfo

LANGS=	en_US.ISO_8859-1 fr_FR.ISO_8859-1

afterinstall:
	@for i in ${LANGS}; do \
	  gencat -new ${DESTDIR}/usr/share/nls/$$i/ee.cat ${.CURDIR}/nls/$$i/ee.msg; \
	  chmod 444 ${.CURDIR}/nls/$$i/ee.msg; \
	done

.include <bsd.prog.mk>
