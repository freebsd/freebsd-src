# $FreeBSD$

SRCDIR=	${.CURDIR}/../../../contrib/libf2c

.PATH: ${SRCDIR} ${SRCDIR}/libU77 ${SRCDIR}/libF77 ${SRCDIR}/libI77

LIB=		g2c
SHLIB_MAJOR=	1

# Traditional FORTRAN Library members defined in libF77

F77MISC=	F77_aloc.c main.c s_rnge.c abort_.c getarg_.c \
		iargc_.c getenv_.c signal_.c s_stop.c s_paus.c system_.c \
		cabs.c derf_.c derfc_.c erf_.c erfc_.c sig_die.c exit_.c \
		setarg.c setsig.c

F77POW=		pow_ci.c pow_dd.c pow_di.c pow_hh.c pow_ii.c pow_ri.c pow_zi.c \
		pow_zz.c

F77CX=		c_abs.c c_cos.c c_div.c c_exp.c c_log.c c_sin.c c_sqrt.c
F77DCX=		${F77CX:S/^c_/z_/g}

F77INT=		i_abs.c i_dim.c i_dnnt.c i_indx.c i_len.c i_mod.c i_nint.c \
		i_sign.c

F77HALF=	h_abs.c h_dim.c h_dnnt.c h_indx.c h_len.c h_mod.c h_nint.c \
		h_sign.c

F77REAL=	r_abs.c r_acos.c r_asin.c r_atan.c r_atn2.c r_cnjg.c r_cos.c \
		r_cosh.c r_dim.c r_exp.c r_imag.c r_int.c r_lg10.c r_log.c \
		r_mod.c r_nint.c r_sign.c r_sin.c r_sinh.c r_sqrt.c r_tan.c \
		r_tanh.c
F77DBL=		${F77REAL:S/^r_/d_/g} d_prod.c

F77CMP=		l_ge.c l_gt.c l_le.c l_lt.c hl_ge.c hl_gt.c hl_le.c hl_lt.c

F77EFL=		ef1asc_.c ef1cmc_.c

F77CHAR=	s_cat.c s_cmp.c s_copy.c

F77F90BIT=	lbitbits.c lbitshft.c qbitbits.c qbitshft.c

F77SRCS=	${F77MISC} ${F77POW} ${F77CX} ${F77DCX} ${F77INT} ${F77HALF} \
		${F77REAL} ${F77DBL} ${F77CMP} ${F77EFL} ${F77CHAR} ${F77F90BIT}

# I/O Library members defined in libI77

I77SRCS=	backspace.c close.c dfe.c dolio.c due.c endfile.c \
		err.c fmt.c fmtlib.c iio.c ilnw.c inquire.c lread.c lwrite.c \
		open.c rdfmt.c rewind.c rsfe.c rsli.c rsne.c sfe.c sue.c \
		typesize.c uio.c util.c wref.c wrtfmt.c wsfe.c wsle.c wsne.c \
		xwsne.c ftell_.c

# UNIX Library members defined in libU77

U77SRCS=	gerror_.c perror_.c ierrno_.c itime_.c time_.c \
		unlink_.c fnum_.c getpid_.c getuid_.c getgid_.c kill_.c \
		rand_.c srand_.c irand_.c sleep_.c idate_.c ctime_.c \
		etime_.c dtime_.c isatty_.c ltime_.c fstat_.c stat_.c \
		lstat_.c access_.c link_.c getlog_.c ttynam_.c getcwd_.c \
		symlnk_.c vxttime_.c vxtidate_.c gmtime_.c fdate_.c \
		secnds_.c bes.c dbes.c chdir_.c chmod_.c lnblnk_.c \
		hostnm_.c rename_.c fgetc_.c fputc_.c umask_.c \
		sys_clock_.c date_.c second_.c flush1_.c mclock_.c \
		alarm_.c datetime_.c

# Library members refered to as libE77 by EGCS

E77OBJS=	abort.o access.o alarm.o besj0.o besj1.o besjn.o \
		besy0.o besy1.o besyn.o chdir.o chmod.o ctime.o date.o \
		dbesj0.o dbesj1.o dbesjn.o dbesy0.o dbesy1.o dbesyn.o derf.o \
		derfc.o dtime.o ef1asc.o ef1cmc.o erf.o erfc.o etime.o exit.o \
		fdate.o fget.o fgetc.o flush.o flush1.o fnum.o fput.o fputc.o \
		fseek.o fstat.o ftell.o gerror.o getarg.o getcwd.o getenv.o \
		getgid.o getlog.o getpid.o getuid.o gmtime.o hostnm.o iargc.o \
		idate.o ierrno.o irand.o isatty.o itime.o kill.o link.o \
		lnblnk.o lstat.o ltime.o mclock.o perror.o rand.o rename.o \
		secnds.o second.o signal.o sleep.o srand.o stat.o symlnk.o \
		system.o time.o ttynam.o umask.o unlink.o vxtidt.o vxttim.o

LIBF77SRCS=	${F77SRCS} ${I77SRCS} ${U77SRCS} ${E77SRCS} MAIN.c

SRCS=		${LIBF77SRCS}
OBJS=		${E77OBJS}

CFLAGS+=	-DHAVE_CONFIG_H=1 -DSTDC_HEADERS=1 -DHAVE_TEMPNAM=1
CFLAGS+=	-DNO_EOF_CHAR_CHECK=1 -DRETSIGTYPE=void
CFLAGS+=	-DSkip_f2c_Undefs=1 -DIEEE_drem=1 -DAllow_TYQUAD -DPedantic
CFLAGS+=	-I${SRCDIR} -I${SRCDIR}/libF77 -I${SRCDIR}/libI77 -I${SRCDIR}/libU77
CFLAGS+=	-I${.CURDIR}

.for x in F I U
${x}77SRCS+=	Version${x}.c
CLEANFILES+=	Version${x}.c
Version${x}.c: ${SRCDIR}/lib${x}77/Version.c
	ln -sf ${.ALLSRC} ${.TARGET}
.endfor

${E77OBJS}: f2cext.c
	${CC} ${CFLAGS} -c -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.if !defined(NOPIC)
${E77OBJS:.o=.So}: f2cext.c
	${CC} ${PICFLAG} -DPIC ${CFLAGS} -c -DL${.PREFIX} -o ${.TARGET} \
	    ${.ALLSRC:M*.c}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}
.endif

.if !defined(NOPROFILE)
${E77OBJS:.o=.po}: f2cext.c
	${CC} -p ${CFLAGS} -c -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
	@${LD} -o ${.TARGET}.tmp -X -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}
.endif

INCS=	g2c.h

.include <bsd.lib.mk>
