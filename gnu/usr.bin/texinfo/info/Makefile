#
# $id$
#

INFOSRCDIR= ${.CURDIR}/../../../../contrib/texinfo/info
TXIDIR= ${.CURDIR}/../../../../contrib/texinfo/libtxi
LIBTXI=	${.CURDIR}/../libtxi/libtxi.a
BINDIR= /usr/bin

PROG= info

SRCS+=	dir.c display.c doc.c echo_area.c filesys.c info-utils.c info.c infodoc.c infomap.c
SRCS+=	m-x.c nodes.c search.c session.c signals.c terminal.c tilde.c window.c indices.c
SRCS+=	xmalloc.c nodemenu.c footnotes.c dribble.c variables.c gc.c man.c clib.c

# $(INFODIR) is defined in /usr/share/mk/bsd.own.mk
CFLAGS+= -DHANDLE_MAN_PAGES -DNAMED_FUNCTIONS=1
CFLAGS+= -DDEFAULT_INFOPATH=\"${INFODIR}:/usr/local/info:.\"
CFLAGS+= -DSTDC_HEADERS=1 -DHAVE_UNISTD_H=1 -DHAVE_TERMIOS_H=1 -DHAVE_STRINGS_H=1 -DHAVE_STRING_H=1
CFLAGS+= -DHAVE_VARARGS_H=1 -DHAVE_SYS_TIME_H=1 -DHAVE_SYS_FCNTL_H=1 -DHAVE_SYS_FILE_H=1
CFLAGS+= -DHAVE_ALLOCA=1 -DHAVE_SETVBUF=1 -DHAVE_GETCWD=1 -DHAVE_MEMSET=1 -DHAVE_BZERO=1
CFLAGS+= -DHAVE_STRCHR=1 -DHAVE_STRCASECMP=1 -DHAVE_VFPRINTF=1 -DHAVE_VSPRINTF=1 -DHAVE_STRERROR=1
CFLAGS+= -DHAVE_SIGPROCMASK=1 -DHAVE_SIGSETMASK=1 -I../libtxi -I$(TXIDIR)

LDADD+= -ltermcap -L../libtxi -ltxi
DPADD+= ${LIBTERMCAP} ${LIBTXI}

.PATH: $(INFOSRCDIR)

.include <bsd.prog.mk>
