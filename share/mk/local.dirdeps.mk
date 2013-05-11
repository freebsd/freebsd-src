
.if ${DEP_MACHINE} != "host"

# this is how we can handle optional dependencies
.if ${MK_SSP:Uno} != "no" && defined(PROG)
DIRDEPS += gnu/lib/libssp/libssp_nonshared
.endif

.endif

# we need pkgs/pseudo/stage to prep the stage tree
.if ${DEP_RELDIR:U${RELDIR}} != "pkgs/pseudo/stage"
DIRDEPS += pkgs/pseudo/stage
.endif

M_dep_qual_fixes += C;\.host,[^/.,]*$$;.host;
M_dep_qual_fixes += C;\.common,[^/.,]*$$;.common;

CSU_DIR.i386 = csu/i386-elf
CSU_DIR.${DEP_MACHINE_ARCH} ?= csu/${DEP_MACHINE_ARCH}
CSU_DIR := ${CSU_DIR.${DEP_MACHINE_ARCH}}

# we want to supress these dependencies for host tools
DIRDEPS_FILTER.host = \
	Ninclude* \
	Nlib/lib* \
	Nlib/csu* \
	Nlib/[mn]* \
	Ngnu/lib/csu* \
	Ngnu/lib/lib[a-r]* \

