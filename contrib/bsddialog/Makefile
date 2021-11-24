# Any copyright is dedicated to the Public Domain, see:
#     <http://creativecommons.org/publicdomain/zero/1.0/>
#
# Written by Alfonso Sabato Siciliano

OUTPUT=  bsddialog
SOURCES= bsddialog.c
OBJECTS= ${SOURCES:.c=.o}
LIBPATH= ${.CURDIR}/lib
LIBBSDDIALOG= ${LIBPATH}/libbsddialog.so

CFLAGS= -Wall -I${LIBPATH}
LDFLAGS= -Wl,-rpath=${LIBPATH} -L${LIBPATH} -lbsddialog

BINDIR= /usr/local/bin
MAN= ${OUTPUT}.1
GZIP= gzip -cn
MANDIR= /usr/local/share/man/man1

INSTALL= install
RM= rm -f

all : ${OUTPUT}

${OUTPUT}: ${LIBBSDDIALOG} ${OBJECTS}
	${CC} ${LDFLAGS} ${OBJECTS} -o ${.PREFIX}

${LIBBSDDIALOG}:
.if defined(PORTNCURSES)
	make -C ${LIBPATH} -DPORTNCURSES
.else
	make -C ${LIBPATH}
.endif

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}

install:
	${INSTALL} -s -m 555 ${OUTPUT} ${BINDIR}
	${GZIP} ${MAN} > ${MAN}.gz
	${INSTALL} -m 444 ${MAN}.gz ${MANDIR}

unistall:
	${RM} ${BINDIR}/${OUTPUT}
	${RM} ${MANDIR}/${MAN}.gz

clean:
	make -C ${LIBPATH} clean
	${RM} ${OUTPUT} *.o *~ *.core ${MAN}.gz
