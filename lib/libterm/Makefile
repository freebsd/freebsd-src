#	@(#)Makefile	5.10 (Berkeley) 6/1/90
#
# PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
# --------------------         -----   ----------------------
# CURRENT PATCH LEVEL:         1       00138
# --------------------         -----   ----------------------
#
# 11 Apr 93	Rodney W. Grimes	Added conditional for profiled
#					library LINKS

LIB=	termcap
CFLAGS+=-DCM_N -DCM_GT -DCM_B -DCM_D
SRCS=	termcap.c tgoto.c tputs.c

MAN3=	termcap.3
MLINKS=	termcap.3 tgetent.3 termcap.3 tgetflag.3 termcap.3 tgetnum.3 \
	termcap.3 tgetstr.3 termcap.3 tgoto.3 termcap.3 tputs.3
LINKS=	${LIBDIR}/libtermcap.a ${LIBDIR}/libtermlib.a \

.if !defined(NOPROFILE)
LINKS+=	${LIBDIR}/libtermcap_p.a ${LIBDIR}/libtermlib_p.a
.endif

.include <bsd.lib.mk>
