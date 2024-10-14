.if !defined(MK_CLANG)
.include "${SRCTOP}/share/mk/src.opts.mk"
.endif

.-include <${.PARSEFILE:S/local/site/}>

DIRDEPS_TARGETS_DIRS ?= targets targets/pseudo targets/packages
