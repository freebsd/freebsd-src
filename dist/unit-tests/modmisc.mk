# $Id: modmisc.mk,v 1.1.1.15 2020/08/23 15:52:08 sjg Exp $
#
# miscellaneous modifier tests

# do not put any dirs in this list which exist on some
# but not all target systems - an exists() check is below.
path=:/bin:/tmp::/:.:/no/such/dir:.
# strip cwd from path.
MOD_NODOT=S/:/ /g:N.:ts:
# and decorate, note that $'s need to be doubled. Also note that 
# the modifier_variable can be used with other modifiers.
MOD_NODOTX=S/:/ /g:N.:@d@'$$d'@
# another mod - pretend it is more interesting
MOD_HOMES=S,/home/,/homes/,
MOD_OPT=@d@$${exists($$d):?$$d:$${d:S,/usr,/opt,}}@
MOD_SEP=S,:, ,g

all:	modvar modvarloop modsysv emptyvar undefvar
all:	mod-quote
all:	mod-break-many-words

# See also sysv.mk.
modsysv:
	@echo "The answer is ${libfoo.a:L:libfoo.a=42}"

# Demonstrates modifiers that are given indirectly from a variable.
modvar:
	@echo "path='${path}'"
	@echo "path='${path:${MOD_NODOT}}'"
	@echo "path='${path:S,home,homes,:${MOD_NODOT}}'"
	@echo "path=${path:${MOD_NODOTX}:ts:}"
	@echo "path=${path:${MOD_HOMES}:${MOD_NODOTX}:ts:}"

.for d in ${path:${MOD_SEP}:N.} /usr/xbin
path_$d?= ${d:${MOD_OPT}:${MOD_HOMES}}/
paths+= ${d:${MOD_OPT}:${MOD_HOMES}}
.endfor

modvarloop:
	@echo "path_/usr/xbin=${path_/usr/xbin}"
	@echo "paths=${paths}"
	@echo "PATHS=${paths:tu}"

# When a modifier is applied to the "" variable, the result is discarded.
emptyvar:
	@echo S:${:S,^$,empty,}
	@echo C:${:C,^$,empty,}
	@echo @:${:@var@${var}@}

# The :U modifier turns even the "" variable into something that has a value.
# The resulting variable is empty, but is still considered to contain a
# single empty word. This word can be accessed by the :S and :C modifiers,
# but not by the :@ modifier since it explicitly skips empty words.
undefvar:
	@echo S:${:U:S,^$,empty,}
	@echo C:${:U:C,^$,empty,}
	@echo @:${:U:@var@empty@}


mod-quote:
	@echo $@: new${.newline:Q}${.newline:Q}line

# Cover the bmake_realloc in brk_string.
mod-break-many-words:
	@echo $@: ${UNDEF:U:range=500:[#]}

# To apply a modifier indirectly via another variable, the whole
# modifier must be put into a single variable.
.if ${value:L:${:US}${:U,value,replacement,}} != "S,value,replacement,}"
.warning unexpected
.endif

# Adding another level of indirection (the 2 nested :U expressions) helps.
.if ${value:L:${:U${:US}${:U,value,replacement,}}} != "replacement"
.warning unexpected
.endif

# Multiple indirect modifiers can be applied one after another as long as
# they are separated with colons.
.if ${value:L:${:US,a,A,}:${:US,e,E,}} != "vAluE"
.warning unexpected
.endif

# An indirect variable that evaluates to the empty string is allowed though.
# This makes it possible to define conditional modifiers, like this:
#
# M.little-endian=	S,1234,4321,
# M.big-endian=		# none
.if ${value:L:${:Dempty}S,a,A,} != "vAlue"
.warning unexpected
.endif

