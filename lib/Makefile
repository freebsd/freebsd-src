# Any copyright is dedicated to the Public Domain, see:
#     <http://creativecommons.org/publicdomain/zero/1.0/>
#
# Written in 2021 by Alfonso Sabato Siciliano

VERSION = 0.1
LIBRARY = bsddialog
LIBRARY_SO = lib${LIBRARY:=.so}
HEADERS = bsddialog.h bsddialog_theme.h
SOURCES = barbox.c formbox.c infobox.c libbsddialog.c lib_util.c menubox.c \
	messagebox.c textbox.c theme.c timebox.c
OBJECTS= ${SOURCES:.c=.o}
FBSDFLAGS= -O2 -pipe -std=gnu99 -Wno-format-zero-length \
	-fstack-protector-strong -Qunused-arguments
CFLAGS = -I/usr/local/include -fPIC -Wall -Wextra ${FBSDFLAGS}
LDFLAGS = -fstack-protector-strong -shared -Wl,-x -Wl,--fatal-warnings \
	-Wl,--warn-shared-textrel -Wl,-soname,${LIBRARY_SO}.${VERSION} 

INSTALL_PREFIX=/usr/local
LN = ln -s -f
RM = rm -f
CP = cp
GZIP = gzip -cn
LDCONFIG = /sbin/ldconfig -m

.if defined(PORTNCURSES)
# PORT ncurses `make -DPORTNCURSES` or `make -D PORTNCURSES`
CFLAGS += -DPORTNCURSES -I/usr/local/include
LDFLAGS += -L/usr/local/lib -lform -lncurses -ltinfo
.else
# BASE ncurses
LDFLAGS += -L/usr/lib -lform -lncurses -ltinfo
.endif

MAN= ${OUTPUT}.3
GZIP= gzip -cn
MANDIR= /usr/local/share/man/man3

INSTALL= install
RM= rm -f

#all : man ${LIBRARY}
all : ${LIBRARY}

${LIBRARY}: ${OBJECTS}
	${CC} ${LDFLAGS} ${.ALLSRC} -o ${LIBRARY_SO}.${VERSION}
	# LN for devel
	${LN} ${LIBRARY_SO}.${VERSION} ${LIBRARY_SO}

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}

man:
	${GZIP} ${LIBRARY}.3 > ${LIBRARY}.3.gz

clean:
	${RM} ${LIBRARY_SO}* *.o *~ *.gz


install:
	${CP} ${LIBRARY}.h ${INSTALL_PREFIX}/include
	${CP} ${LIBRARY_SO}.${VERSION} ${INSTALL_PREFIX}/lib/
	${LN} ${INSTALL_PREFIX}/lib/${LIBRARY_SO}.${VERSION} ${INSTALL_PREFIX}/lib/${LIBRARY_SO}
	${LDCONFIG} ${INSTALL_PREFIX}/lib
	${CP} ${LIBRARY}.3.gz ${INSTALL_PREFIX}/man/man3/

unistall:
	${RM} ${INSTALL_PREFIX}/include/${LIBRARY}.h
	${RM} ${INSTALL_PREFIX}/lib/${LIBRARY_SO}
	${RM} ${INSTALL_PREFIX}/lib/${LIBRARY_SO}.${VERSION}
	${LDCONFIG} ${INSTALL_PREFIX}/lib
	${RM} ${INSTALL_PREFIX}/man/man3/${LIBRARY}.3.gz
