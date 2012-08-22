.if !target(_DIRDEP_USE)
# first time read
.if ${MACHINE} == "host"
DIRDEPS_FILTER+= \
	Ninclude* \
	Nlib/* \
	Ngnu/lib/* \

.endif
.endif

# this is how we can handle optional dependencies
.if ${MK_SSP:Uno} != "no" && defined(PROG)
DIRDEPS += gnu/lib/libssp/libssp_nonshared
.endif
