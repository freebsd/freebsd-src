# PUBLIC DOMAIN - NO WARRANTY, see:
#     <http://creativecommons.org/publicdomain/zero/1.0/>
#
# Written in 2021 by Alfonso Sabato Siciliano

VERSION = 0.4
LIBRARY = bsddialog
LIBRARY_SO = lib${LIBRARY:=.so}
HEADERS = bsddialog.h bsddialog_theme.h bsddialog_progressview.h
SOURCES = barbox.c calendarbox.c formbox.c infobox.c libbsddialog.c \
	lib_util.c menubox.c messagebox.c textbox.c theme.c timebox.c
OBJECTS = $(SOURCES:.c=.o)
CFLAGS = -D_XOPEN_SOURCE_EXTENDED -D_XOPEN_SOURCE -D_GNU_SOURCE -Wall -Wextra \
	-Wno-implicit-fallthrough -Werror -fpic
LDFLAGS = -lncursesw -ltinfo
LIBFLAG = -shared

RM = rm -f
LN = ln -s -f

all : $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	$(CC) $(LIBFLAG) $^ -o $(LIBRARY_SO).$(VERSION) $(LDFLAGS)
	${LN} ${LIBRARY_SO}.${VERSION} ${LIBRARY_SO}

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<

clean:
	$(RM) $(LIBRARY_SO)* *.o *~
