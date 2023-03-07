# PUBLIC DOMAIN - NO WARRANTY, see:
#     <http://creativecommons.org/publicdomain/zero/1.0/>
#
# Written in 2021 by Alfonso Sabato Siciliano

VERSION = 0.4
LIBRARY = bsddialog
LIBRARY_SO = lib${LIBRARY:=.so}
LIBRARY_A = lib${LIBRARY:=.a}
HEADERS = bsddialog.h bsddialog_theme.h bsddialog_progressview.h
SOURCES = barbox.c calendarbox.c formbox.c infobox.c libbsddialog.c \
	lib_util.c menubox.c messagebox.c textbox.c theme.c timebox.c
OBJECTS = ${SOURCES:.c=.o}
CFLAGS += -D_XOPEN_SOURCE_EXTENDED -fPIC -Wall -Wextra
LDFLAGS += -fstack-protector-strong -shared -Wl,-x -Wl,--fatal-warnings \
	-Wl,--warn-shared-textrel -Wl,-soname,${LIBRARY_SO}.${VERSION} \
	-L/usr/lib -lncursesw -ltinfow

.if defined(DEBUG)
# `make -DDEBUG`
CFLAGS = -g -D_XOPEN_SOURCE_EXTENDED -fPIC -Wall -Wextra
.else
CFLAGS += -std=gnu99 -fstack-protector-strong
.endif

LOCALBASE = /usr/local
LN = ln -s -f
RM = rm -f
CP = cp
GZIP = gzip -cn
LDCONFIG = /sbin/ldconfig -m
MAN = ${OUTPUT}.3
GZIP = gzip -cn
MANDIR = ${LOCALBASE}/share/man/man3
INSTALL = install
RM = rm -f

all : man ${LIBRARY}

${LIBRARY}: ${LIBRARY_SO} ${LIBRARY_A}

${LIBRARY_SO}.${VERSION}: ${OBJECTS}
	${CC} ${LDFLAGS} ${.ALLSRC} -o ${LIBRARY_SO}.${VERSION}

${LIBRARY_SO}: ${LIBRARY_SO}.${VERSION}
	${LN} ${LIBRARY_SO}.${VERSION} ${LIBRARY_SO}

${LIBRARY_A}: ${OBJECTS}
	${AR} cr ${.TARGET} ${OBJECTS}
	${RANLIB} ${.TARGET}

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}

man:
	${GZIP} ${LIBRARY}.3 > ${LIBRARY}.3.gz

clean:
	${RM} ${LIBRARY_SO}* *.o *~ *.gz ${LIBRARY_A}


install:
	${INSTALL} -m 644 ${HEADERS} ${LOCALBASE}/include
	${INSTALL} -m 644 -s ${LIBRARY_SO}.${VERSION} ${LOCALBASE}/lib/
	${INSTALL} -l rs ${LOCALBASE}/lib/${LIBRARY_SO}.${VERSION} ${LOCALBASE}/lib/${LIBRARY_SO}
	${INSTALL} -m 644 ${LIBRARY_A} ${LOCALBASE}/lib
	${LDCONFIG} ${LOCALBASE}/lib
	${INSTALL} -m 644 ${LIBRARY}.3.gz ${MANDIR}

unistall:
	${RM} ${LOCALBASE}/include/${LIBRARY}*.h
	${RM} ${LOCALBASE}/lib/${LIBRARY_SO}
	${RM} ${LOCALBASE}/lib/${LIBRARY_SO}.${VERSION}
	${LDCONFIG} ${LOCALBASE}/lib
	${RM} ${MANDIR}/${LIBRARY}.3.gz
