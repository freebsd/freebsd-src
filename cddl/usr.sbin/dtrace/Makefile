.include <src.opts.mk>

.PATH: ${SRCTOP}/cddl/contrib/opensolaris/cmd/dtrace

PACKAGE=	dtrace
PROG=		dtrace
SRCS=		dtrace.c
BINDIR?=	/usr/sbin

WARNS?=		1

CFLAGS+= -DIN_BASE
CFLAGS+= -DSKIP_SPL_SYS_CONDVAR_H
CFLAGS+= -I${ZFSTOP}/include
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+=	-I${SRCTOP}/sys/cddl/compat/opensolaris \
		-I${SRCTOP}/cddl/compat/opensolaris/include \
		-I${OPENSOLARIS_USR_DISTDIR}/head \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libdtrace/common \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libproc/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/uts/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/compat
CFLAGS+= -DHAVE_ISSETUGID

# Optional debugging stuff...
#CFLAGS+=	-DNEED_ERRLOC
#YFLAGS+=	-d

.if ${MK_DTRACE_ASAN} != "no"
CFLAGS+=	-fsanitize=address -fsanitize=undefined
LDFLAGS+=	-fsanitize=address -fsanitize=undefined
.endif

LIBADD=	dtrace ctf elf proc spl xo pthread

.if ${MK_DTRACE_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
