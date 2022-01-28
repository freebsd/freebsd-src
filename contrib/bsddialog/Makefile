# PUBLIC DOMAIN - NO WARRANTY, see:
#     <http://creativecommons.org/publicdomain/zero/1.0/>
#
# Written in 2021 by Alfonso Sabato Siciliano

OUTPUT=  bsddialog
SOURCES= bsddialog.c
OBJECTS= ${SOURCES:.c=.o}
LIBPATH= ${.CURDIR}/lib
LIBBSDDIALOG= ${LIBPATH}/libbsddialog.so

CFLAGS+= -I${LIBPATH} -std=gnu99 -Wno-format-zero-length \
-fstack-protector-strong -Wsystem-headers -Werror -Wall -Wno-format-y2k -W \
-Wno-unused-parameter -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith \
-Wno-uninitialized -Wno-pointer-sign -Wno-empty-body -Wno-string-plus-int \
-Wno-unused-const-variable -Wno-tautological-compare -Wno-unused-value \
-Wno-parentheses-equality -Wno-unused-function -Wno-enum-conversion \
-Wno-unused-local-typedef -Wno-address-of-packed-member -Qunused-arguments
# `make -DDEBUG`
.if defined(DEBUG)
CFLAGS= -g -Wall -I${LIBPATH}
LIBDEBUG= -DDEBUG
.endif
LDFLAGS+= -Wl,-rpath=${LIBPATH} -L${LIBPATH} -lbsddialog

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
	make -C ${LIBPATH} ${LIBDEBUG}

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
