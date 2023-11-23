
.include <src.opts.mk>

PROG=	calendar
SRCS=	calendar.c locale.c events.c dates.c parsedata.c io.c day.c \
	ostern.c paskha.c pom.c sunpos.c
LIBADD=	m util

.if ${MK_ICONV} == "yes"
CFLAGS+=	-DWITH_ICONV
.endif

FILESGROUPS+=	CALS
CALS=	calendars/calendar.freebsd
CALSDIR=	${SHAREDIR}/calendar

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
