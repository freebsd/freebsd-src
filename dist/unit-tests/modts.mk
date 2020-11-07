# $NetBSD: modts.mk,v 1.7 2020/10/24 08:50:17 rillig Exp $

LIST=	one two three
LIST+=	four five six

FU_mod-ts=	a / b / cool

AAA=	a a a
B.aaa=	Baaa

all:   mod-ts mod-ts-space

# Use print or printf iff they are builtin.
# XXX note that this causes problems, when make decides
# there is no need to use a shell, so avoid where possible.
.if ${(type print) 2> /dev/null || echo:L:sh:Mbuiltin} != ""
PRINT=  print -r --
.elif ${(type printf) 2> /dev/null || echo:L:sh:Mbuiltin} != ""
PRINT=  printf '%s\n'
.else
PRINT=	echo
.endif

mod-ts:
	@${PRINT} 'LIST:tx="${LIST:tx}"'
	@${PRINT} 'LIST:ts/x:tu="${LIST:ts\X:tu}"'
	@${PRINT} 'FU_$@="${FU_${@:ts}:ts}"'
	@${PRINT} 'FU_$@:ts:T="${FU_${@:ts}:ts:T}" == cool?'
	@${PRINT} 'B.$${AAA:ts}="${B.${AAA:ts}}" == Baaa?'

mod-ts-space:
	# After the :ts modifier, the whole string is interpreted as a single
	# word since all spaces have been replaced with x.
	@${PRINT} ':ts :S => '${aa bb aa bb aa bb:L:tsx:S,b,B,:Q}

	# The :ts modifier also applies to word separators that are added
	# afterwards.
	@${PRINT} ':ts :S space    => '${a ababa c:L:tsx:S,b, ,g:Q}
	@${PRINT} ':ts :S space :M => '${a ababa c:L:tsx:S,b, ,g:M*:Q}

	# Not all modifiers behave this way though.  Some of them always use
	# a space as word separator instead of the :ts separator.
	# This seems like an oversight during implementation.
	@${PRINT} ':ts :S       => '${a ababa c:L:tsx:S,b, ,g:Q}
	@${PRINT} ':ts :S :@    => '${a ababa c:L:tsx:S,b, ,g:@v@${v}@:Q}

	# A final :M* modifier applies the :ts separator again, though.
	@${PRINT} ':ts :S :@ :M => '${a ababa c:L:tsx:S,b, ,g:@v@${v}@:M*:Q}
