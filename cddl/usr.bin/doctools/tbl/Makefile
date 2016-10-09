PROG=	tbl
SRCS=	t0.c \
	t1.c \
	t2.c \
	t3.c \
	t4.c \
	t5.c \
	t6.c \
	t7.c \
	t8.c \
	t9.c \
	tb.c \
	tc.c \
	te.c \
	tf.c \
	tg.c \
	ti.c \
	tm.c \
	ts.c \
	tt.c \
	tu.c \
	tv.c

.if defined(WORLDTMP)
CFLAGS+=	-DMACDIR='"${WORLDTMP}/legacy/${MACDIR}"'
.else
CFLAGS+=	-DMACDIR='"${MACDIR}"'
.endif

.include <bsd.prog.mk>

.PATH: ${DOCTOOLSDIR}/tbl
