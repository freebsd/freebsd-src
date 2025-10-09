# PUBLIC DOMAIN - NO WARRANTY, see:
#     <http://creativecommons.org/publicdomain/zero/1.0/>
#
# Written in 2021 by Alfonso Sabato Siciliano

OUTPUT =  bsddialog
SOURCES = bsddialog.c util_builders.c util_cli.c util_theme.c
OBJECTS = ${SOURCES:.c=.o}
PREFIX = /usr/local

.if defined(DEBUG)
CFLAGS += -g
.endif
CFLAGS += -I${LIBPATH} -std=gnu99 -Wall -Wextra -Werror

.if defined(NORPATH)
LDFLAGS += -ltinfow -L${LIBPATH} -lbsddialog
.else
LDFLAGS += -ltinfow -Wl,-rpath=${LIBPATH} -L${LIBPATH} -lbsddialog
.endif

INSTALL = install
RM = rm -f

all: ${OUTPUT}

install: all
	${INSTALL} -m 0755 ${OUTPUT} ${DESTDIR}${PREFIX}/bin/${OUTPUT}

uninstall:
	${RM} ${DESTDIR}${PREFIX}/bin/${OUTPUT}

${OUTPUT}: ${OBJECTS}
	${CC} ${LDFLAGS} ${OBJECTS} -o ${.PREFIX}

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}

clean:
	${RM} ${OUTPUT} *.o *~ *.core *.gz

.PHONY: all install uninstall clean
