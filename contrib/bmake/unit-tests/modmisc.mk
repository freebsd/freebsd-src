# $NetBSD: modmisc.mk,v 1.52 2020/12/20 19:29:06 rillig Exp $
#
# miscellaneous modifier tests

# do not put any dirs in this list which exist on some
# but not all target systems - an exists() check is below.
path=		:/bin:/tmp::/:.:/no/such/dir:.
# strip cwd from path.
MOD_NODOT=	S/:/ /g:N.:ts:
# and decorate, note that $'s need to be doubled. Also note that
# the modifier_variable can be used with other modifiers.
MOD_NODOTX=	S/:/ /g:N.:@d@'$$d'@
# another mod - pretend it is more interesting
MOD_HOMES=	S,/home/,/homes/,
MOD_OPT=	@d@$${exists($$d):?$$d:$${d:S,/usr,/opt,}}@
MOD_SEP=	S,:, ,g

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
path_$d?=	${d:${MOD_OPT}:${MOD_HOMES}}/
paths+=		${d:${MOD_OPT}:${MOD_HOMES}}
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
# The value of the resulting expression is empty, but is still considered to
# contain a single empty word. This word can be accessed by the :S and :C
# modifiers, but not by the :@ modifier since it explicitly skips empty words.
undefvar:
	@echo S:${:U:S,^$,empty,}
	@echo C:${:U:C,^$,empty,}
	@echo @:${:U:@var@empty@}


mod-quote:
	@echo $@: new${.newline:Q}${.newline:Q}line

# Cover the bmake_realloc in Str_Words.
mod-break-many-words:
	@echo $@: ${UNDEF:U:range=500:[#]}
