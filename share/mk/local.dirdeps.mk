# $FreeBSD$
.if !target(_DIRDEP_USE)
# we are the 1st makefile

.if !defined(MK_CLANG)
.include "${SRCTOP}/share/mk/src.opts.mk"
.endif

# DEP_MACHINE is set before we get here, this may not be.
DEP_RELDIR ?= ${RELDIR}

.if ${.TARGETS:Uall:M*/*} && empty(DIRDEPS)
# This little trick let's us do
#
# mk -f dirdeps.mk some/dir.i386,bsd
#
DIRDEPS := ${.TARGETS:M*/*}
${.TARGETS:Nall}: all
.endif

# making universe is special
.if defined(UNIVERSE_GUARD)
# these should be done by now
DIRDEPS_FILTER+= N*.host
.endif

# pseudo machines get no qualification
.for m in host common
M_dep_qual_fixes += C;($m),[^/.,]*$$;\1;
.endfor

#.info M_dep_qual_fixes=${M_dep_qual_fixes}
# we want to supress these dependencies for host tools
# but some libs are sadly needed.
_need_host_libs= \
	lib/libc++ \
	lib/libcxxrt \
	lib/libdwarf \
	lib/libmd \

N_host_libs:= ${cd ${SRCTOP} && echo lib/lib*:L:sh:${_need_host_libs:${M_ListToSkip}}:${M_ListToSkip}}
DIRDEPS_FILTER.host = \
	${N_host_libs} \
	Ninclude* \
	Nlib/csu* \
	Nlib/[mn]* \
	Ngnu/lib/csu* \
	Ngnu/lib/lib[a-r]* \
	Nusr.bin/xinstall* \


DIRDEPS_FILTER+= \
	${DIRDEPS_FILTER.xtras:U}
.endif

# reset this each time
DIRDEPS_FILTER.xtras=
.if ${DEP_MACHINE:Npkgs*} != ""
DIRDEPS_FILTER.xtras+= Nusr.bin/clang/clang.host
.endif

.if ${DEP_MACHINE} != "host"

# this is how we can handle optional dependencies
.if ${DEP_RELDIR} == "lib/libc"
DIRDEPS += lib/libc_nonshared
.if ${MK_SSP:Uno} != "no" 
DIRDEPS += gnu/lib/libssp/libssp_nonshared
.endif
.else
DIRDEPS_FILTER.xtras+= Nlib/libc_nonshared
.endif

# some optional things
.if ${MK_CTF} == "yes" && ${DEP_RELDIR:Mcddl/usr.bin/ctf*} == ""
DIRDEPS += \
	cddl/usr.bin/ctfconvert.host \
	cddl/usr.bin/ctfmerge.host
.endif

.endif

.if ${MK_CLANG} == "yes" && ${DEP_RELDIR:Nlib/clang/lib*:Nlib/libc*} == ""
DIRDEPS+= lib/clang/include
.endif

.if ${MK_STAGING} == "yes"
# we need targets/pseudo/stage to prep the stage tree
.if ${DEP_RELDIR} != "targets/pseudo/stage"
DIRDEPS += targets/pseudo/stage
.endif
.endif

DEP_MACHINE_ARCH = ${MACHINE_ARCH.${DEP_MACHINE}}
CSU_DIR.${DEP_MACHINE_ARCH} ?= csu/${DEP_MACHINE_ARCH}
CSU_DIR := ${CSU_DIR.${DEP_MACHINE_ARCH}}
BOOT_MACHINE_DIR:= ${BOOT_MACHINE_DIR.${DEP_MACHINE}}
KERNEL_NAME:= ${KERNEL_NAME.${DEP_MACHINE}}
