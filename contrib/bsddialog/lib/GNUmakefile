# PUBLIC DOMAIN - NO WARRANTY, see:
#     <http://creativecommons.org/publicdomain/zero/1.0/>
#
# Written in 2021 by Alfonso Sabato Siciliano

LIBRARY = bsddialog
LIBRARY_SO = lib${LIBRARY:=.so}
HEADERS = bsddialog.h bsddialog_theme.h bsddialog_progressview.h
SOURCES = barbox.c datebox.c formbox.c libbsddialog.c lib_util.c \
	menubox.c messagebox.c textbox.c theme.c timebox.c
OBJECTS = $(SOURCES:.c=.o)
PREFIX = /usr/local

ifneq ($(ENABLEDEBUG),)
CFLAGS += -g
endif
CFLAGS += -D_XOPEN_SOURCE_EXTENDED -D_XOPEN_SOURCE -D_GNU_SOURCE \
	-Wall -Wextra -Werror -fpic
LDFLAGS += -lncursesw -ltinfo
LIBFLAG = -shared

RM = rm -f
LN = ln -s -f

all: $(LIBRARY)

install: all
	${INSTALL} -m 0644 bsddialog.h ${DESTDIR}${PREFIX}/include/bsddialog.h
	${INSTALL} -m 0644 bsddialog_progressview.h ${DESTDIR}${PREFIX}/include/bsddialog_progressview.h
	${INSTALL} -m 0644 bsddialog_theme.h ${DESTDIR}${PREFIX}/include/bsddialog_theme.h
	${INSTALL} -m 0755 ${LIBRARY_SO}.${VERSION} ${DESTDIR}${PREFIX}/lib/${LIBRARY_SO}.${VERSION}
	${LN} ${LIBRARY_SO}.${VERSION} ${DESTDIR}${PREFIX}/lib/${LIBRARY_SO}

uninstall:
	${RM} ${DESTDIR}${PREFIX}/include/bsddialog.h
	${RM} ${DESTDIR}${PREFIX}/include/bsddialog_progressview.h
	${RM} ${DESTDIR}${PREFIX}/include/bsddialog_theme.h
	${RM} ${DESTDIR}${PREFIX}/lib/${LIBRARY_SO}.${VERSION}
	${RM} ${DESTDIR}${PREFIX}/lib/${LIBRARY_SO}

$(LIBRARY): $(OBJECTS)
	$(CC) $(LIBFLAG) $^ -o $(LIBRARY_SO).$(VERSION) $(LDFLAGS)
	${LN} ${LIBRARY_SO}.${VERSION} ${LIBRARY_SO}

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<

clean:
	$(RM) $(LIBRARY_SO)* *.o *~

.PHONY: all install uninstall ${LIBRARY} clean
