#
# $Id: Makefile,v 1.17 1999/04/06 20:08:01 obrien Exp $
#
.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}/config/${GCC_ARCH} ${GCCDIR}

SRCS=	c-common.c c-pragma.c \
	caller-save.c calls.c combine.c convert.c cse.c \
	dbxout.c dwarfout.c emit-rtl.c explow.c expmed.c expr.c \
	final.c flow.c fold-const.c function.c getpwd.c global.c \
	insn-attrtab.c insn-emit.c insn-extract.c insn-opinit.c \
	insn-output.c insn-peep.c insn-recog.c \
	integrate.c jump.c local-alloc.c loop.c obstack.c optabs.c \
	print-rtl.c print-tree.c real.c recog.c reg-stack.c regclass.c \
	reload.c reload1.c reorg.c rtl.c rtlanal.c \
	sdbout.c stmt.c stor-layout.c stupid.c \
	toplev.c tree.c unroll.c varasm.c version.c xcoffout.c \
	alias.c bitmap.c dwarf2out.c dyn-string.c except.c \
	gcse.c genrtl.c profile.c regmove.c varray.c \
	${OUT_FILE}
.if defined(USE_EGCS_HAIFA) && ${USE_EGCS_HAIFA} == 1
SRCS+=	haifa-sched.c
.else
SRCS+=	sched.c
.endif

CFLAGS+=	-DTARGET_NAME=\"${MACHINE_ARCH}-unknown-freebsd\"

LIB=		cc_int
NOPROFILE=	YES
NOPIC=		YES

install:
	@true

${OUT_OBJ}.o ${OUT_OBJ}.So:	${OUT_FILE}

.include <bsd.lib.mk>
