#	$NetBSD: Makefile,v 1.25 2002/02/01 01:32:20 ross Exp $

.include <bsd.own.mk>

PROG=		lint1
SRCS=		cgram.y scan.l mem1.c mem.c err.c main1.c decl.c tree.c func.c \
		init.c emit.c emit1.c inittyp.c
MAN=		lint.7
YHEADER=

CPPFLAGS+=	-I${.CURDIR} -I.

.if ${OBJECT_FMT} == "a.out"
CPPFLAGS+=	-DTARGET_OBJFMT_AOUT
.elif ${OBJECT_FMT} == "COFF"
CPPFLAGS+=	-DTARGET_OBJFMT_COFF
.elif ${OBJECT_FMT} == "ELF"
CPPFLAGS+=	-DTARGET_OBJFMT_ELF
.else
.BEGIN:
	@echo "Unrecognized OBJECT_FMT ${OBJECT_FMT}"
	@false
.endif

LINTFLAGS=	-aehpz
WFORMAT=1		#hopeless
BINDIR=		/usr/libexec

scan.c: cgram.h

CLEANFILES+=	${MAN}

.if ${USETOOLS} == "yes"
LINT1=		${TOOLDIR}/libexec/${MACHINE_GNU_PLATFORM}-lint1
.endif
LINT1?=		./${PROG}

${MAN}:		makeman ${LINT1:C/^\.\///} Makefile
	sh ${.ALLSRC:M*makeman} ${LINT1} -m >${.TARGET}

LDADD+=		-lm
DPADD+=		${LIBM}

.ifndef HOSTPROG
LDADD+=		-ll
DPADD+=		${LIBL}

.include <bsd.prog.mk>
.endif
