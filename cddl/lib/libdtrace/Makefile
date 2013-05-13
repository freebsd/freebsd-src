# $FreeBSD$

.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libdtrace/common
.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libgen/common

LIB=		dtrace
SRCS=		dt_aggregate.c \
		dt_as.c \
		dt_buf.c \
		dt_cc.c \
		dt_cg.c \
		dt_consume.c \
		dt_decl.c \
		dt_dis.c \
		dt_dof.c \
		dt_error.c \
		dt_errtags.c \
		dt_grammar.y \
		dt_handle.c \
		dt_ident.c \
		dt_isadep.c \
		dt_inttab.c \
		dt_lex.l \
		dt_link.c \
		dt_list.c \
		dt_map.c \
		dt_module.c \
		dt_names.c \
		dt_open.c \
		dt_options.c \
		dt_parser.c \
		dt_pcb.c \
		dt_pid.c \
		dt_pq.c \
		dt_pragma.c \
		dt_print.c \
		dt_printf.c \
		dt_proc.c \
		dt_program.c \
		dt_provider.c \
		dt_regset.c \
		dt_string.c \
		dt_strtab.c \
		dt_subr.c \
		dt_work.c \
		dt_xlator.c \
		gmatch.c

DSRCS=		errno.d			\
		io.d			\
		psinfo.d		\
		signal.d		\
		unistd.d

WARNS?=		1

CFLAGS+=	-I${.OBJDIR} -I${.CURDIR} \
		-I${.CURDIR}/../../../sys/cddl/dev/dtrace/${MACHINE_ARCH} \
		-I${.CURDIR}/../../../sys/cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/compat/opensolaris/include \
		-I${OPENSOLARIS_USR_DISTDIR}/head \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libctf/common \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libdtrace/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/uts/common

#CFLAGS+=	-DYYDEBUG

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=	-I${OPENSOLARIS_SYS_DISTDIR}/uts/intel -DDIS_MEM
.PATH:		${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libdtrace/i386
.PATH:		${.CURDIR}/../../../sys/cddl/dev/dtrace/${MACHINE_ARCH}
.elif ${MACHINE_CPUARCH} == "sparc64"
CFLAGS+=	-I${OPENSOLARIS_SYS_DISTDIR}/uts/sparc
.PATH:		${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libdtrace/sparc
.elif ${MACHINE_CPUARCH} == "mips"
CFLAGS+=	-I${OPENSOLARIS_SYS_DISTDIR}/uts/mips
.PATH:		${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libdtrace/mips
.PATH:		${.CURDIR}/../../../sys/cddl/dev/dtrace/mips
.elif ${MACHINE_CPUARCH} == "powerpc"
CFLAGS+=	-I${OPENSOLARIS_SYS_DISTDIR}/uts/powerpc
.PATH:		${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libdtrace/powerpc
.PATH:		${.CURDIR}/../../../sys/cddl/dev/dtrace/powerpc
.else
# temporary hack
CFLAGS+=	-I${OPENSOLARIS_SYS_DISTDIR}/uts/intel
.endif

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
SRCS+=		dis_tables.c
DSRCS+=		regs_x86.d
.endif

LFLAGS+=-l

YFLAGS+=-d

CLEANFILES=	dt_errtags.c dt_names.c

dt_errtags.c:
	sh ${OPENSOLARIS_USR_DISTDIR}/lib/libdtrace/common/mkerrtags.sh < ${OPENSOLARIS_USR_DISTDIR}/lib/libdtrace/common/dt_errtags.h > dt_errtags.c

dt_names.c:
	sh ${OPENSOLARIS_USR_DISTDIR}/lib/libdtrace/common/mknames.sh < ${OPENSOLARIS_SYS_DISTDIR}/uts/common/sys/dtrace.h > dt_names.c

beforedepend:	dt_errtags.c dt_names.c

beforeinstall:
.if !defined(LIBRARIES_ONLY) && exists(${DESTDIR}/usr/lib/dtrace)
.for file in ${DSRCS}
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} ${.CURDIR}/${file} ${DESTDIR}/usr/lib/dtrace
.endfor
.endif

.include <bsd.lib.mk>
