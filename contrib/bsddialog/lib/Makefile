# PUBLIC DOMAIN - NO WARRANTY, see:
#     <http://creativecommons.org/publicdomain/zero/1.0/>
#
# Written in 2021 by Alfonso Sabato Siciliano

LIBRARY = bsddialog
LIBRARY_SO = lib${LIBRARY:=.so}
LIBRARY_A = lib${LIBRARY:=.a}
HEADERS = bsddialog.h bsddialog_theme.h bsddialog_progressview.h
SOURCES = barbox.c datebox.c formbox.c libbsddialog.c lib_util.c \
	menubox.c messagebox.c textbox.c theme.c timebox.c
OBJECTS = ${SOURCES:.c=.o}

.if defined(DEBUG)
CFLAGS += -g
.endif
CFLAGS += -D_XOPEN_SOURCE_EXTENDED -fPIC -Wall -Wextra -std=gnu99 \
	-fstack-protector-strong
LDFLAGS += -fstack-protector-strong -shared -Wl,-x -Wl,--fatal-warnings \
	-Wl,--warn-shared-textrel -Wl,-soname,${LIBRARY_SO}.${VERSION} \
	-L/usr/lib -lncursesw -ltinfow

LN = ln -s -f
RM = rm -f

all : ${LIBRARY}

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

clean:
	${RM} ${LIBRARY_SO}* *.o *~ *.gz ${LIBRARY_A}
