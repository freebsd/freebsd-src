
# this is how we can handle optional dependencies
.if ${MK_SSP:Uno} != "no" && defined(PROG)
DIRDEPS += gnu/lib/libssp/libssp_nonshared
.endif

# we need pkgs/pseudo/stage to prep the stage tree
.if ${DEP_RELDIR:U${RELDIR}} != "pkgs/pseudo/stage"
DIRDEPS += pkgs/pseudo/stage
.endif

# we want to supress these dependencies for host tools
DIRDEPS_FILTER.host = \
	Ninclude* \
	Nlib/lib* \
	Nlib/csu* \
	Nlib/[mn]* \
	Ngnu/lib/csu* \
	Ngnu/lib/lib[a-r]* \

