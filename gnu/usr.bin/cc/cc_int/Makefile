#
# $Id: Makefile,v 1.2 1994/09/18 22:35:53 wollman Exp $
#

SRCS =	aux-output.c bc-emit.c bc-optab.c c-common.c caller-save.c calls.c combine.c convert.c cse.c dbxout.c dwarfout.c emit-rtl.c explow.c expmed.c expr.c final.c flow.c fold-const.c function.c getpwd.c global.c insn-attrtab.c insn-emit.c insn-extract.c insn-opinit.c insn-output.c insn-peep.c insn-recog.c integrate.c jump.c local-alloc.c loop.c obstack.c optabs.c print-rtl.c print-tree.c real.c recog.c reg-stack.c regclass.c reload.c reload1.c reorg.c rtl.c rtlanal.c sched.c sdbout.c stmt.c stor-layout.c stupid.c toplev.c tree.c unroll.c varasm.c version.c xcoffout.c
LIB =	cc_int
.if !defined(NO_SHARED_LIBCC_INT)
INTERNALLIB=
SHLIB_MAJOR=26
SHLIB_MINOR=0
.else

install:
	@true
.endif

.include <bsd.lib.mk>
