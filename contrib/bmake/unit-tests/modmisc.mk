# $Id: modmisc.mk,v 1.1.1.2 2020/07/04 17:52:46 sjg Exp $
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

all:	modvar modvarloop modsysv mod-HTE emptyvar undefvar
all:	mod-S mod-C mod-at-varname mod-at-resolve

modsysv:
	@echo "The answer is ${libfoo.a:L:libfoo.a=42}"

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

PATHNAMES=	a/b/c def a.b.c a.b/c a a.a .gitignore a a.a
mod-HTE:
	@echo "dirname of '"${PATHNAMES:Q}"' is '"${PATHNAMES:H:Q}"'"
	@echo "basename of '"${PATHNAMES:Q}"' is '"${PATHNAMES:T:Q}"'"
	@echo "suffix of '"${PATHNAMES:Q}"' is '"${PATHNAMES:E:Q}"'"
	@echo "root of '"${PATHNAMES:Q}"' is '"${PATHNAMES:R:Q}"'"

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

mod-S:
	@echo :${:Ua b b c:S,a b,,:Q}:
	@echo :${:Ua b b c:S,a b,,1:Q}:
	@echo :${:Ua b b c:S,a b,,W:Q}:
	@echo :${:Ua b b c:S,b,,g:Q}:
	@echo :${:U1 2 3 1 2 3:S,1 2,___,Wg:S,_,x,:Q}:

mod-C:
	@echo :${:Ua b b c:C,a b,,:Q}:
	@echo :${:Ua b b c:C,a b,,1:Q}:
	@echo :${:Ua b b c:C,a b,,W:Q}:
	@echo :${:Uword1 word2:C,****,____,g:C,word,____,:Q}:
	@echo :${:Ua b b c:C,b,,g:Q}:
	@echo :${:U1 2 3 1 2 3:C,1 2,___,Wg:C,_,x,:Q}:

# In the :@ modifier, the name of the loop variable can even be generated
# dynamically.  There's no practical use-case for this, and hopefully nobody
# will ever depend on this, but technically it's possible.
mod-at-varname:
	@echo :${:Uone two three:@${:Ubar:S,b,v,}@+${var}+@:Q}:

# The :@ modifier resolves the variables a little more often than expected.
# In particular, it resolves _all_ variables from the context, and not only
# the loop variable (in this case v).
#
# The d means direct reference, the i means indirect reference.
RESOLVE=	${RES1} $${RES1}
RES1=		1d${RES2} 1i$${RES2}
RES2=		2d${RES3} 2i$${RES3}
RES3=		3

mod-at-resolve:
	@echo $@:${RESOLVE:@v@w${v}w@:Q}:
