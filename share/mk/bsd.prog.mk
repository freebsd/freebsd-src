#	from: @(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91

.include <bsd.init.mk>
.include <bsd.compiler.mk>
.include <bsd.linker.mk>

.include <bsd.suffixes-extra.mk>

# XXX The use of COPTS in modern makefiles is discouraged.
.if defined(COPTS)
.warning ${.CURDIR}: COPTS should be CFLAGS.
CFLAGS+=${COPTS}
.endif

.if defined(PROG_CXX)
PROG=	${PROG_CXX}
.endif

.if !empty(LDFLAGS:M-Wl,*--oformat,*) || !empty(LDFLAGS:M-static)
MK_DEBUG_FILES=	no
.endif

# ELF hardening knobs
.if ${MK_BIND_NOW} != "no"
LDFLAGS+= -Wl,-znow
.endif
.if ${LINKER_TYPE} != "mac"
.if ${MK_RELRO} == "no"
LDFLAGS+= -Wl,-znorelro
.else
LDFLAGS+= -Wl,-zrelro
.endif
.endif
# Static PIE is not yet supported/tested.
.if ${MK_PIE} != "no" && (!defined(NO_SHARED) || ${NO_SHARED:tl} == "no")
CFLAGS+= -fPIE
CXXFLAGS+= -fPIE
LDFLAGS+= -pie
OBJ_EXT=pieo
.else
OBJ_EXT=o
.endif
.if ${MK_RETPOLINE} != "no"
.if ${COMPILER_FEATURES:Mretpoline} && ${LINKER_FEATURES:Mretpoline}
CFLAGS+= -mretpoline
CXXFLAGS+= -mretpoline
# retpolineplt is broken with static linking (PR 233336)
.if !defined(NO_SHARED) || ${NO_SHARED:tl} == "no"
LDFLAGS+= -Wl,-zretpolineplt
.endif
.else
.if !defined(_NO_INCLUDE_COMPILERMK)
.warning Retpoline requested but not supported by compiler or linker
.endif
.endif
.endif
# LLD sensibly defaults to -znoexecstack, so do the same for BFD
LDFLAGS.bfd+= -Wl,-znoexecstack
.if ${MK_BRANCH_PROTECTION} != "no"
CFLAGS+=  -mbranch-protection=standard
.if ${LINKER_FEATURES:Mbti-report} && defined(BTI_REPORT_ERROR)
LDFLAGS+= -Wl,-zbti-report=error
.endif
.endif

# Initialize stack variables on function entry
.if ${OPT_INIT_ALL} != "none"
.if ${COMPILER_FEATURES:Minit-all}
CFLAGS+= -ftrivial-auto-var-init=${OPT_INIT_ALL}
CXXFLAGS+= -ftrivial-auto-var-init=${OPT_INIT_ALL}
.if ${OPT_INIT_ALL} == "zero" && ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} < 160000
CFLAGS+= -enable-trivial-auto-var-init-zero-knowing-it-will-be-removed-from-clang
CXXFLAGS+= -enable-trivial-auto-var-init-zero-knowing-it-will-be-removed-from-clang
.endif
.else
.if !defined(_NO_INCLUDE_COMPILERMK)
.warning INIT_ALL (${OPT_INIT_ALL}) requested but not supported by compiler
.endif
.endif
.endif

# Zero used registers on return (mitigate some ROP)
.if ${MK_ZEROREGS} != "no"
.if ${COMPILER_FEATURES:Mzeroregs}
ZEROREG_TYPE?= used
CFLAGS+= -fzero-call-used-regs=${ZEROREG_TYPE}
CXXFLAGS+= -fzero-call-used-regs=${ZEROREG_TYPE}
.endif
.endif

# bsd.sanitizer.mk is not installed, so don't require it (e.g. for ports).
.sinclude "bsd.sanitizer.mk"

.if ${MACHINE_CPUARCH} == "riscv" && ${LINKER_FEATURES:Mriscv-relaxations} == ""
CFLAGS += -mno-relax
.endif

.if defined(CRUNCH_CFLAGS)
CFLAGS+=${CRUNCH_CFLAGS}
.endif

.if defined(NO_ROOT)
.if !defined(TAGS) || ! ${TAGS:Mpackage=*}
TAGS+=		package=${PACKAGE:Uutilities}
.endif
TAG_ARGS=	-T ${TAGS:ts,:[*]}
.endif

.if defined(NO_SHARED) && ${NO_SHARED:tl} != "no"
LDFLAGS+= -static
.endif

.if ${MK_DEBUG_FILES} != "no"
PROG_FULL=${PROG}.full
# Use ${DEBUGDIR} for base system debug files, else .debug subdirectory
.if defined(BINDIR) && (\
    ${BINDIR} == "/bin" ||\
    ${BINDIR:C%/libexec(/.*)?%/libexec%} == "/libexec" ||\
    ${BINDIR} == "/sbin" ||\
    ${BINDIR:C%/usr/(bin|bsdinstall|libexec|lpr|sendmail|sm.bin|sbin|tests)(/.*)?%/usr/bin%} == "/usr/bin" ||\
    ${BINDIR} == "/usr/lib" \
     )
DEBUGFILEDIR=	${DEBUGDIR}${BINDIR}
.else
DEBUGFILEDIR?=	${BINDIR}/.debug
.endif
.if !exists(${DESTDIR}${DEBUGFILEDIR})
DEBUGMKDIR=
.endif
.else
PROG_FULL=	${PROG}
.endif

.if defined(PROG)
PROGNAME?=	${PROG}
.if ${MK_DEBUG_FILES} != "no"
DEBUGFILE= ${PROGNAME}.debug
.endif

.if defined(SRCS)

OBJS+=  ${SRCS:N*.h:${OBJS_SRCS_FILTER:ts:}:S/$/.${OBJ_EXT}/g}

# LLVM bitcode / textual IR representations of the program
BCOBJS+=${SRCS:N*.[hsS]:N*.asm:${OBJS_SRCS_FILTER:ts:}:S/$/.bco/g}
LLOBJS+=${SRCS:N*.[hsS]:N*.asm:${OBJS_SRCS_FILTER:ts:}:S/$/.llo/g}

.if target(beforelinking)
beforelinking: ${OBJS}
${PROG_FULL}: beforelinking
.endif
${PROG_FULL}: ${OBJS}
.if defined(PROG_CXX)
	${CXX:N${CCACHE_BIN}} ${CXXFLAGS:N-M*} ${LDFLAGS} -o ${.TARGET} \
	    ${OBJS} ${LDADD}
.else
	${CC:N${CCACHE_BIN}} ${CFLAGS:N-M*} ${LDFLAGS} -o ${.TARGET} ${OBJS} \
	    ${LDADD}
.endif
.if ${MK_CTF} != "no"
	${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ${OBJS}
.endif

.else	# !defined(SRCS)

.if !target(${PROG})
.if defined(PROG_CXX)
SRCS=	${PROG}.cc
.else
SRCS=	${PROG}.c
.endif

# Always make an intermediate object file because:
# - it saves time rebuilding when only the library has changed
# - the name of the object gets put into the executable symbol table instead of
#   the name of a variable temporary object.
# - it's useful to keep objects around for crunching.
OBJS+=		${PROG}.${OBJ_EXT}
BCOBJS+=	${PROG}.bc
LLOBJS+=	${PROG}.ll
CLEANFILES+=	${PROG}.${OBJ_EXT} ${PROG}.bc ${PROG}.ll

.if target(beforelinking)
beforelinking: ${OBJS}
${PROG_FULL}: beforelinking
.endif
${PROG_FULL}: ${OBJS}
.if defined(PROG_CXX)
	${CXX:N${CCACHE_BIN}} ${CXXFLAGS:N-M*} ${LDFLAGS} -o ${.TARGET} \
	    ${OBJS} ${LDADD}
.else
	${CC:N${CCACHE_BIN}} ${CFLAGS:N-M*} ${LDFLAGS} -o ${.TARGET} ${OBJS} \
	    ${LDADD}
.endif
.if ${MK_CTF} != "no"
	${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ${OBJS}
.endif
.endif # !target(${PROG})

.endif # !defined(SRCS)

.if ${MK_DEBUG_FILES} != "no"
CLEANFILES+= ${PROG_FULL} ${DEBUGFILE}
${PROG}: ${PROG_FULL} ${DEBUGFILE}
	${OBJCOPY} --strip-debug --add-gnu-debuglink=${DEBUGFILE} \
	    ${PROG_FULL} ${.TARGET}

${DEBUGFILE}: ${PROG_FULL}
	${OBJCOPY} --only-keep-debug ${PROG_FULL} ${.TARGET}
.endif

.if defined(LLVM_LINK)
${PROG_FULL}.bc: ${BCOBJS}
	${LLVM_LINK} -o ${.TARGET} ${BCOBJS}

${PROG_FULL}.ll: ${LLOBJS}
	${LLVM_LINK} -S -o ${.TARGET} ${LLOBJS}

CLEANFILES+=	${PROG_FULL}.bc ${PROG_FULL}.ll
.endif # defined(LLVM_LINK)

.if	${MK_MAN} != "no" && !defined(MAN) && \
	!defined(MAN1) && !defined(MAN2) && !defined(MAN3) && \
	!defined(MAN4) && !defined(MAN5) && !defined(MAN6) && \
	!defined(MAN7) && !defined(MAN8) && !defined(MAN9)
MAN=	${PROG}.1
MAN1=	${MAN}
.endif
.endif # defined(PROG)

.if defined(_SKIP_BUILD)
all:
.else
.if target(afterbuild)
.ORDER: ${PROG} afterbuild
all: ${PROG} ${SCRIPTS} afterbuild
.else
all: ${PROG} ${SCRIPTS}
.endif
.if ${MK_MAN} != "no"
all: all-man
.endif
.endif

.if defined(PROG)
CLEANFILES+= ${PROG} ${PROG}.bc ${PROG}.ll
.endif

.if defined(OBJS)
CLEANFILES+= ${OBJS} ${BCOBJS} ${LLOBJS}
.endif

.include <bsd.libnames.mk>

.if defined(PROG)
.if !defined(NO_EXTRADEPEND)
_EXTRADEPEND:
.if defined(LDFLAGS) && !empty(LDFLAGS:M-nostdlib)
.if defined(DPADD) && !empty(DPADD)
	echo ${PROG_FULL}: ${DPADD} >> ${DEPENDFILE}
.endif
.else
	echo ${PROG_FULL}: ${LIBC} ${DPADD} >> ${DEPENDFILE}
.if defined(PROG_CXX)
	echo ${PROG_FULL}: ${LIBCPLUSPLUS} >> ${DEPENDFILE}
.endif
.endif
.endif	# !defined(NO_EXTRADEPEND)
.endif

.if !target(install)

.if defined(PRECIOUSPROG)
.if !defined(NO_FSCHG)
INSTALLFLAGS+= -fschg
.endif
INSTALLFLAGS+= -S
.endif

_INSTALLFLAGS:=	${INSTALLFLAGS}
.for ie in ${INSTALLFLAGS_EDIT}
_INSTALLFLAGS:=	${_INSTALLFLAGS${ie}}
.endfor

.if !target(realinstall) && !defined(INTERNALPROG)
realinstall: _proginstall _debuginstall
.ORDER: beforeinstall _proginstall _debuginstall
_proginstall:
.if defined(PROG)
	${INSTALL} ${TAG_ARGS} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${_INSTALLFLAGS} ${PROG} ${DESTDIR}${BINDIR}/${PROGNAME}
.endif
.endif	# !target(realinstall)

.if defined(SCRIPTS) && !empty(SCRIPTS)
realinstall: _scriptsinstall
.ORDER: beforeinstall _scriptsinstall

SCRIPTSDIR?=	${BINDIR}
SCRIPTSOWN?=	${BINOWN}
SCRIPTSGRP?=	${BINGRP}
SCRIPTSMODE?=	${BINMODE}

STAGE_AS_SETS+= scripts
stage_as.scripts: ${SCRIPTS}
FLAGS.stage_as.scripts= -m ${SCRIPTSMODE}
STAGE_FILES_DIR.scripts= ${STAGE_OBJTOP}
.for script in ${SCRIPTS}
.if defined(SCRIPTSNAME)
SCRIPTSNAME_${script:T}?=	${SCRIPTSNAME}
.else
SCRIPTSNAME_${script:T}?=	${script:T:R}
.endif
SCRIPTSDIR_${script:T}?=	${SCRIPTSDIR}
SCRIPTSOWN_${script:T}?=	${SCRIPTSOWN}
SCRIPTSGRP_${script:T}?=	${SCRIPTSGRP}
SCRIPTSMODE_${script:T}?=	${SCRIPTSMODE}
STAGE_AS_${script:T}=		${SCRIPTSDIR_${script:T}}/${SCRIPTSNAME_${script:T}}
_scriptsinstall: _SCRIPTSINS_${script:T}
_SCRIPTSINS_${script:T}: ${script}
	${INSTALL} ${TAG_ARGS} -o ${SCRIPTSOWN_${.ALLSRC:T}} \
	    -g ${SCRIPTSGRP_${.ALLSRC:T}} -m ${SCRIPTSMODE_${.ALLSRC:T}} \
	    ${.ALLSRC} \
	    ${DESTDIR}${SCRIPTSDIR_${.ALLSRC:T}}/${SCRIPTSNAME_${.ALLSRC:T}}
.endfor
.endif

NLSNAME?=	${PROG}
.include <bsd.nls.mk>

.include <bsd.confs.mk>
.include <bsd.files.mk>
.include <bsd.incs.mk>

LINKOWN?=	${BINOWN}
LINKGRP?=	${BINGRP}
LINKMODE?=	${BINMODE}
.include <bsd.links.mk>

.if ${MK_MAN} != "no"
realinstall: maninstall
.ORDER: beforeinstall maninstall
.endif

.endif	# !target(install)

.if ${MK_MAN} != "no"
.include <bsd.man.mk>
.endif

.if defined(HAS_TESTS)
MAKE+=			MK_MAKE_CHECK_USE_SANDBOX=yes
SUBDIR_TARGETS+=	check
TESTS_LD_LIBRARY_PATH+=	${.OBJDIR}
TESTS_PATH+=		${.OBJDIR}
.endif

.if defined(PROG)
OBJS_DEPEND_GUESS+= ${SRCS:M*.h}
.endif

.include <bsd.debug.mk>
.include <bsd.dep.mk>
.include <bsd.clang-analyze.mk>
.include <bsd.obj.mk>
.include <bsd.sys.mk>
