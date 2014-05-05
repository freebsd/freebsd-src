.if !target(_DIRDEP_USE)
# we are the 1st makefile

.if !defined(MK_CTF)
.include "${SRCTOP}/share/mk/bsd.opts.mk"
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

# pseudo machines get no qualification
.for m in host common
M_dep_qual_fixes += C;($m),[^/.,]*$$;\1;
.endfor

#.info M_dep_qual_fixes=${M_dep_qual_fixes}
# we want to supress these dependencies for host tools
DIRDEPS_FILTER.host = \
	Ninclude* \
	Nlib/lib* \
	Nlib/csu* \
	Nlib/[mn]* \
	Ngnu/lib/csu* \
	Ngnu/lib/lib[a-r]* \


.endif

.if ${DEP_MACHINE} != "host"

# this is how we can handle optional dependencies
.if ${MK_SSP:Uno} != "no" && defined(PROG)
DIRDEPS += gnu/lib/libssp/libssp_nonshared
.endif

# some optional things
.if ${MK_CTF} == "yes" && ${DEP_RELDIR:U${RELDIR}:Mcddl/usr.bin/ctf*} == ""
DIRDEPS += \
	cddl/usr.bin/ctfconvert.host \
	cddl/usr.bin/ctfmerge.host
.endif

.endif

# we need pkgs/pseudo/stage to prep the stage tree
.if ${DEP_RELDIR:U${RELDIR}} != "pkgs/pseudo/stage"
DIRDEPS += pkgs/pseudo/stage
.endif

CSU_DIR.i386 = csu/i386-elf
DEP_MACHINE_ARCH = ${MACHINE_ARCH.${DEP_MACHINE}}
CSU_DIR.${DEP_MACHINE_ARCH} ?= csu/${DEP_MACHINE_ARCH}
CSU_DIR := ${CSU_DIR.${DEP_MACHINE_ARCH}}
BOOT_MACHINE_DIR:= ${BOOT_MACHINE_DIR.${DEP_MACHINE}}
KERNEL_NAME:= ${KERNEL_NAME.${DEP_MACHINE}}
