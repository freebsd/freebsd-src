PROG= bc

BCSRCS= execute.c global.c load.c main.c number.c storage.c util.c
GENSRCS= bc.c scan.c
SRCS= ${GENSRCS} ${BCSRCS}

CFLAGS+= -D_POSIX_SOURCE -I.
CLEANFILES+= ${GENSRCS} y.tab.h

.include <bsd.prog.mk>
