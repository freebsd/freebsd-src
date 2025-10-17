PACKAGE=	ncurses

.include <bsd.own.mk>
.include "${SRCTOP}/lib/ncurses/config.mk"

.PATH: ${NCURSES_DIR}/progs ${NCURSES_DIR}/man

PROGS=	tic tput infocmp toe tabs clear tset
SRCS.tic=	tic.c transform.c dump_entry.c tparm_type.c
SRCS.tput=	tput.c tparm_type.c transform.c dump_entry.c clear_cmd.c reset_cmd.c \
		tty_settings.c
SRCS.infocmp=	infocmp.c dump_entry.c
SRCS.tabs=	tabs.c tty_settings.c
SRCS.clear=	clear.c clear_cmd.c tty_settings.c
SRCS.tset=	tset.c tty_settings.c transform.c reset_cmd.c
CFLAGS+=	-I${NCURSES_DIR}/progs \
		-I${NCURSES_DIR}/include \
		-I${SRCTOP}/lib/ncurses/tinfo \
		-I${OBJTOP}/lib/ncurses/tinfo \
		-I${.CURDIR}
LIBADD=	tinfow
CLEANFILES=	termsort.h

termsort.h: MKtermsort.sh
	sh ${NCURSES_DIR}/progs/MKtermsort.sh ${AWK} ${NCURSES_DIR}/include/Caps > ${.TARGET}
dump_entry.c: termsort.h

LINKS.tic=	${BINDIR}/tic ${BINDIR}/captoinfo \
	${BINDIR}/tic ${BINDIR}/infotocap
LINKS.tset=	${BINDIR}/tset ${BINDIR}/reset

MLINKS.tic=	tic.1 captoinfo.1 \
	tic.1 infotocap.1
MLINKS.tset=	tset.1 reset.1

.include <bsd.progs.mk>

.SUFFIXES: .1 .1m
.1m.1:
	cat ${.IMPSRC} > ${.TARGET}
