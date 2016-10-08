CFLAGS+=	-I${DOCTOOLSDIR}/eqn \
		-DPUBDIR="${PUBDIR}"

.include "../Makefile.inc"

.PATH:	${DOCTOOLSDIR}/eqn
