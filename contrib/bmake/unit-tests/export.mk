# $NetBSD: export.mk,v 1.12 2022/09/09 18:36:15 sjg Exp $

UT_TEST=	export
UT_FOO=		foo${BAR}
UT_FU=		fubar
UT_ZOO=		hoopie
UT_NO=		all
# believe it or not, we expect this one to come out with $UT_FU unexpanded.
UT_DOLLAR=	This is $$UT_FU

.export UT_FU UT_FOO
.export UT_DOLLAR

.if !defined(.MAKE.PID)
.  error .MAKE.PID must be defined
.endif
@=	at
%=	percent
*=	asterisk
${:U!}=	exclamation		# A direct != would try to run "exclamation"
				# as a shell command and assign its output
				# to the empty variable.
&=	ampersand

# This is ignored because it is internal.
.export .MAKE.PID
# These are ignored because they are local to the target.
.export @
.export %
.export *
.export !
# This is exported (see the .rawout file) but not displayed since the dash
# shell filters it out.  To reach consistent output for each shell, the
# ampersand is filtered out already by FILTER_CMD.
.export &
# This is ignored because it is undefined.
.export UNDEFINED

BAR=	bar is ${UT_FU}

.MAKE.EXPORTED+=	UT_ZOO UT_TEST

FILTER_CMD?=	${EGREP} -v '^(MAKEFLAGS|MALLOC_.*|PATH|PWD|SHLVL|_|&)='

all:
	@env | ${FILTER_CMD} | sort
