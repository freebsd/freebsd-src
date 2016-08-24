#	from: @(#)sys.mk	8.2 (Berkeley) 3/21/94
# $FreeBSD$

unix		?=	We run FreeBSD, not UNIX.
.FreeBSD	?=	true

.if !defined(%POSIX)
#
# MACHINE_CPUARCH defines a collection of MACHINE_ARCH.  Machines with
# the same MACHINE_ARCH can run each other's binaries, so it necessarily
# has word size and endian swizzled in.  However, support files for
# these machines often are shared amongst all combinations of size
# and/or endian.  This is called MACHINE_CPU in NetBSD, but that's used
# for something different in FreeBSD.
#
MACHINE_CPUARCH=${MACHINE_ARCH:C/mips(n32|64)?(el)?/mips/:C/arm(v6)?(eb|hf)?/arm/:C/powerpc64/powerpc/:C/riscv64/riscv/}
.endif


# Some options we need now
__DEFAULT_NO_OPTIONS= \
	DIRDEPS_BUILD \
	DIRDEPS_CACHE

__DEFAULT_DEPENDENT_OPTIONS= \
	AUTO_OBJ/DIRDEPS_BUILD \
	META_MODE/DIRDEPS_BUILD \
	STAGING/DIRDEPS_BUILD \
	SYSROOT/DIRDEPS_BUILD

__ENV_ONLY_OPTIONS:= \
	${__DEFAULT_NO_OPTIONS} \
	${__DEFAULT_YES_OPTIONS} \
	${__DEFAULT_DEPENDENT_OPTIONS:H}

# early include for customization
# see local.sys.mk below
# Not included when building in fmake compatibility mode (still needed
# for older system support)
.if defined(.PARSEDIR)
.sinclude <local.sys.env.mk>

.include <bsd.mkopt.mk>

# Disable MK_META_MODE with make -B
.if ${MK_META_MODE} == "yes" && defined(.MAKEFLAGS) && ${.MAKEFLAGS:M-B}
MK_META_MODE=	no
.endif

.if ${MK_DIRDEPS_BUILD} == "yes"
.sinclude <meta.sys.mk>
.elif ${MK_META_MODE} == "yes"
# verbose will show .MAKE.META.PREFIX for each target.
META_MODE+=	meta verbose
.if !defined(NO_META_MISSING)
META_MODE+=	missing-meta=yes
.endif
# silent will hide command output if a .meta file is created.
.if !defined(NO_SILENT)
META_MODE+=	silent=yes
.endif
.if !exists(/dev/filemon)
META_MODE+= nofilemon
.endif
# Require filemon data with bmake
.if empty(META_MODE:Mnofilemon)
META_MODE+= missing-filemon=yes
.endif
.endif
META_MODE?= normal
.export META_MODE
.MAKE.MODE?= ${META_MODE}
.if !empty(.MAKE.MODE:Mmeta) && !defined(NO_META_IGNORE_HOST)
# Ignore host file changes that will otherwise cause
# buildworld -> installworld -> buildworld to rebuild everything.
# Since the build is self-reliant and bootstraps everything it needs,
# this should not be a real problem for incremental builds.
# XXX: This relies on the existing host tools retaining ABI compatibility
# through upgrades since they won't be rebuilt on header/library changes.
# Note that these are prefix matching, so /lib matches /libexec.
.MAKE.META.IGNORE_PATHS+= \
	${__MAKE_SHELL} \
	/bin \
	/lib \
	/rescue \
	/sbin \
	/usr/bin \
	/usr/include \
	/usr/lib \
	/usr/sbin \
	/usr/share \

.endif


.if ${MK_AUTO_OBJ} == "yes"
# This needs to be done early - before .PATH is computed
# Don't do this for 'make showconfig' as it enables all options where meta mode
# is not expected.
.if !make(showconfig)
.sinclude <auto.obj.mk>
.endif
.endif
.else # bmake
.include <bsd.mkopt.mk>
.endif

# If the special target .POSIX appears (without prerequisites or
# commands) before the first noncomment line in the makefile, make shall
# process the makefile as specified by the Posix 1003.2 specification.
# make(1) sets the special macro %POSIX in this case (to the actual
# value "1003.2", for what it's worth).
#
# The rules below use this macro to distinguish between Posix-compliant
# and default behaviour.
#
# This functionality is currently broken, since make(1) processes sys.mk
# before reading any other files, and consequently has no opportunity to
# set the %POSIX macro before we read this point.

.if defined(%POSIX)
.SUFFIXES:	.o .c .y .l .a .sh .f
.else
.SUFFIXES:	.out .a .ln .o .c .cc .cpp .cxx .C .m .F .f .e .r .y .l .S .asm .s .cl .p .h .sh
.endif

AR		?=	ar
.if defined(%POSIX)
ARFLAGS		?=	-rv
.else
ARFLAGS		?=	-crD
.endif
RANLIB		?=	ranlib
.if !defined(%POSIX)
RANLIBFLAGS	?=	-D
.endif

AS		?=	as
AFLAGS		?=
ACFLAGS		?=

.if defined(%POSIX)
CC		?=	c89
CFLAGS		?=	-O
.else
CC		?=	cc
.if ${MACHINE_CPUARCH} == "arm" || ${MACHINE_CPUARCH} == "mips"
CFLAGS		?=	-O -pipe
.else
CFLAGS		?=	-O2 -pipe
.endif
.if defined(NO_STRICT_ALIASING)
CFLAGS		+=	-fno-strict-aliasing
.endif
.endif
PO_CFLAGS	?=	${CFLAGS}

# cp(1) is used to copy source files to ${.OBJDIR}, make sure it can handle
# read-only files as non-root by passing -f.
CP		?=	cp -f

CPP		?=	cpp

# C Type Format data is required for DTrace
CTFFLAGS	?=	-L VERSION

CTFCONVERT	?=	ctfconvert
CTFMERGE	?=	ctfmerge

.if defined(CFLAGS) && (${CFLAGS:M-g} != "")
CTFFLAGS	+=	-g
.endif

CXX		?=	c++
CXXFLAGS	?=	${CFLAGS:N-std=*:N-Wnested-externs:N-W*-prototypes:N-Wno-pointer-sign:N-Wold-style-definition}
PO_CXXFLAGS	?=	${CXXFLAGS}

DTRACE		?=	dtrace
DTRACEFLAGS	?=	-C -x nolibs

.if empty(.MAKEFLAGS:M-s)
ECHO		?=	echo
ECHODIR		?=	echo
.else
ECHO		?=	true
.if ${.MAKEFLAGS:M-s} == "-s"
ECHODIR		?=	echo
.else
ECHODIR		?=	true
.endif
.endif

.if ${.MAKEFLAGS:M-N}
# bmake -N is supposed to skip executing anything but it does not skip
# exeucting '+' commands.  The '+' feature is used where .MAKE
# is not safe for the entire target.  -N is intended to skip building sub-makes
# so it executing '+' commands is not right.  Work around the bug by not
# setting '+' when -N is used.
_+_		?=
.else
_+_		?=	+
.endif

.if defined(%POSIX)
FC		?=	fort77
FFLAGS		?=	-O 1
.else
FC		?=	f77
FFLAGS		?=	-O
.endif
EFLAGS		?=

INSTALL		?=	install

LEX		?=	lex
LFLAGS		?=

LD		?=	ld
LDFLAGS		?=				# LDFLAGS is for CC, 
_LDFLAGS	=	${LDFLAGS:S/-Wl,//g}	# strip -Wl, for LD

LINT		?=	lint
LINTFLAGS	?=	-cghapbx
LINTKERNFLAGS	?=	${LINTFLAGS}
LINTOBJFLAGS	?=	-cghapbxu -i
LINTOBJKERNFLAGS?=	${LINTOBJFLAGS}
LINTLIBFLAGS	?=	-cghapbxu -C ${LIB}

MAKE		?=	make

.if !defined(%POSIX)
NM		?=	nm
NMFLAGS		?=

OBJC		?=	cc
OBJCFLAGS	?=	${OBJCINCLUDES} ${CFLAGS} -Wno-import

OBJCOPY		?=	objcopy

PC		?=	pc
PFLAGS		?=

RC		?=	f77
RFLAGS		?=
.endif

SHELL		?=	sh

.if !defined(%POSIX)
SIZE		?=	size
.endif

YACC		?=	yacc
.if defined(%POSIX)
YFLAGS		?=
.else
YFLAGS		?=	-d
.endif

.if defined(%POSIX)

# Posix 1003.2 mandated rules
#
# Quoted directly from the Posix 1003.2 draft, only the macros
# $@, $< and $* have been replaced by ${.TARGET}, ${.IMPSRC}, and
# ${.PREFIX}, resp.

# SINGLE SUFFIX RULES
.c:
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.IMPSRC}

.f:
	${FC} ${FFLAGS} ${LDFLAGS} -o ${.TARGET} ${.IMPSRC}

.sh:
	cp -f ${.IMPSRC} ${.TARGET}
	chmod a+x ${.TARGET}

# DOUBLE SUFFIX RULES

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC}

.f.o:
	${FC} ${FFLAGS} -c ${.IMPSRC}

.y.o:
	${YACC} ${YFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} -c y.tab.c
	rm -f y.tab.c
	mv y.tab.o ${.TARGET}

.l.o:
	${LEX} ${LFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} -c lex.yy.c
	rm -f lex.yy.c
	mv lex.yy.o ${.TARGET}

.y.c:
	${YACC} ${YFLAGS} ${.IMPSRC}
	mv y.tab.c ${.TARGET}

.l.c:
	${LEX} ${LFLAGS} ${.IMPSRC}
	mv lex.yy.c ${.TARGET}

.c.a:
	${CC} ${CFLAGS} -c ${.IMPSRC}
	${AR} ${ARFLAGS} ${.TARGET} ${.PREFIX}.o
	rm -f ${.PREFIX}.o

.f.a:
	${FC} ${FFLAGS} -c ${.IMPSRC}
	${AR} ${ARFLAGS} ${.TARGET} ${.PREFIX}.o
	rm -f ${.PREFIX}.o

.else

# non-Posix rule set

.sh:
	cp -f ${.IMPSRC} ${.TARGET}
	chmod a+x ${.TARGET}

.c.ln:
	${LINT} ${LINTOBJFLAGS} ${CFLAGS:M-[DIU]*} ${.IMPSRC} || \
	    touch ${.TARGET}

.cc.ln .C.ln .cpp.ln .cxx.ln:
	${LINT} ${LINTOBJFLAGS} ${CXXFLAGS:M-[DIU]*} ${.IMPSRC} || \
	    touch ${.TARGET}

.c:
	${CC} ${CFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}
	${CTFCONVERT_CMD}

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.cc .cpp .cxx .C:
	${CXX} ${CXXFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}

.cc.o .cpp.o .cxx.o .C.o:
	${CXX} ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.m.o:
	${OBJC} ${OBJCFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.p.o:
	${PC} ${PFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.e .r .F .f:
	${FC} ${RFLAGS} ${EFLAGS} ${FFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} \
	    -o ${.TARGET}

.e.o .r.o .F.o .f.o:
	${FC} ${RFLAGS} ${EFLAGS} ${FFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.S.o:
	${CC:N${CCACHE_BIN}} ${CFLAGS} ${ACFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.asm.o:
	${CC:N${CCACHE_BIN}} -x assembler-with-cpp ${CFLAGS} ${ACFLAGS} -c ${.IMPSRC} \
	    -o ${.TARGET}
	${CTFCONVERT_CMD}

.s.o:
	${AS} ${AFLAGS} -o ${.TARGET} ${.IMPSRC}
	${CTFCONVERT_CMD}

# XXX not -j safe
.y.o:
	${YACC} ${YFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} -c y.tab.c -o ${.TARGET}
	rm -f y.tab.c
	${CTFCONVERT_CMD}

.l.o:
	${LEX} -t ${LFLAGS} ${.IMPSRC} > ${.PREFIX}.tmp.c
	${CC} ${CFLAGS} -c ${.PREFIX}.tmp.c -o ${.TARGET}
	rm -f ${.PREFIX}.tmp.c
	${CTFCONVERT_CMD}

# XXX not -j safe
.y.c:
	${YACC} ${YFLAGS} ${.IMPSRC}
	mv y.tab.c ${.TARGET}

.l.c:
	${LEX} -t ${LFLAGS} ${.IMPSRC} > ${.TARGET}

.s.out .c.out .o.out:
	${CC} ${CFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}
	${CTFCONVERT_CMD}

.f.out .F.out .r.out .e.out:
	${FC} ${EFLAGS} ${RFLAGS} ${FFLAGS} ${LDFLAGS} ${.IMPSRC} \
	    ${LDLIBS} -o ${.TARGET}
	rm -f ${.PREFIX}.o
	${CTFCONVERT_CMD}

# XXX not -j safe
.y.out:
	${YACC} ${YFLAGS} ${.IMPSRC}
	${CC} ${CFLAGS} ${LDFLAGS} y.tab.c ${LDLIBS} -ly -o ${.TARGET}
	rm -f y.tab.c
	${CTFCONVERT_CMD}

.l.out:
	${LEX} -t ${LFLAGS} ${.IMPSRC} > ${.PREFIX}.tmp.c
	${CC} ${CFLAGS} ${LDFLAGS} ${.PREFIX}.tmp.c ${LDLIBS} -ll -o ${.TARGET}
	rm -f ${.PREFIX}.tmp.c
	${CTFCONVERT_CMD}

# Pull in global settings.
__MAKE_CONF?=/etc/make.conf
.if exists(${__MAKE_CONF})
.include "${__MAKE_CONF}"
.endif

# late include for customization
.sinclude <local.sys.mk>

.if defined(META_MODE)
META_MODE:=	${META_MODE:O:u}
.endif

.if defined(__MAKE_SHELL) && !empty(__MAKE_SHELL)
SHELL=	${__MAKE_SHELL}
.SHELL: path=${__MAKE_SHELL}
.endif

# Tell bmake to expand -V VAR by default
.MAKE.EXPAND_VARIABLES= yes

# Tell bmake the makefile preference
.MAKE.MAKEFILE_PREFERENCE= BSDmakefile makefile Makefile

# Tell bmake to always pass job tokens, regardless of target depending on
# .MAKE or looking like ${MAKE}/${.MAKE}/$(MAKE)/$(.MAKE)/make.
.MAKE.ALWAYS_PASS_JOB_QUEUE= yes

# By default bmake does *not* use set -e
# when running target scripts, this is a problem for many makefiles here.
# So define a shell that will do what FreeBSD expects.
.ifndef WITHOUT_SHELL_ERRCTL
__MAKE_SHELL?=/bin/sh
.SHELL: name=sh \
	quiet="set -" echo="set -v" filter="set -" \
	hasErrCtl=yes check="set -e" ignore="set +e" \
	echoFlag=v errFlag=e \
	path=${__MAKE_SHELL}
.endif

# Hack for ports compatibility. Historically, ports makefiles have
# assumed they can examine MACHINE_CPU without including anything
# because this was automatically included in sys.mk. For /usr/src,
# this file has moved to being included from bsd.opts.mk. Until all
# the ports files are modernized, and a reasonable transition
# period has passed, include it while we're in a ports tree here
# to preserve historic behavior.
.if exists(${.CURDIR}/../../Mk/bsd.port.mk)
.include <bsd.cpu.mk>
.endif

.endif # ! Posix
