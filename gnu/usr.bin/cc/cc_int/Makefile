# $FreeBSD: src/gnu/usr.bin/cc/cc_int/Makefile,v 1.25.2.1 2000/07/04 05:41:39 obrien Exp $

.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}/config/${GCC_ARCH} ${GCCDIR}

LIB=		cc_int
NOPROFILE=	YES
NOPIC=		YES

SRCS=	c-aux-info.c c-common.c c-convert.c c-iterate.c \
	c-pragma.c c-typeck.c \
	caller-save.c calls.c combine.c convert.c cse.c \
	dbxout.c dwarfout.c emit-rtl.c explow.c expmed.c expr.c \
	final.c flow.c fold-const.c function.c getpwd.c global.c graph.c \
	hash.c insn-attrtab.c insn-emit.c insn-extract.c insn-opinit.c \
	insn-output.c insn-peep.c insn-recog.c \
	integrate.c jump.c lcm.c local-alloc.c loop.c optabs.c \
	print-rtl.c print-tree.c real.c recog.c reg-stack.c regclass.c \
	reload.c reload1.c reorg.c rtl.c rtlanal.c \
	sbitmap.c sdbout.c stmt.c stor-layout.c stupid.c \
	toplev.c tree.c unroll.c varasm.c version.c xcoffout.c \
	alias.c bitmap.c dwarf2out.c dyn-string.c except.c \
	gcse.c genrtl.c profile.c regmove.c varray.c \
	${OUT_FILE}
.if defined(USE_EGCS_HAIFA) && ${USE_EGCS_HAIFA} == 1
SRCS+=	haifa-sched.c
.else
SRCS+=	sched.c
.endif

#	shared between the drivers
SRCS+=	multilib.h choose-temp.c obstack.c prefix.c pexecute.c version.c

#	stuff that makes libiberty now required (2.95) and shouldn't be...
SRCS+=	obstack.c splay-tree.c

CFLAGS+=	-DTARGET_NAME=\"${target}\" -DIN_GCC

install:
	@true

${OUT_OBJ}.o ${OUT_OBJ}.So:	${OUT_FILE}

.include <bsd.lib.mk>
