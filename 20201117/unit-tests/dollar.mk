# $NetBSD: dollar.mk,v 1.4 2020/11/03 18:21:36 rillig Exp $
#
# Test the various places where a dollar sign can appear and
# see what happens.  There are lots of surprises here.
#

LIST=		plain 'single' "double" 'mix'"ed" back\ slashed
WORD=		word

DOLLAR1=	$
DOLLAR2=	$$
DOLLAR4=	$$$$

X=		VAR_X
DOLLAR_XY=	$$XY
DOLLAR_AXY=	$$AXY

H=	@header()	{ printf '\n%s\n\n' "$$*"; }; header
T=	@testcase()	{ printf '%23s => <%s>\n' "$$@"; }; testcase
C=	@comment()	{ printf '%s\n' "$$*"; }; comment

# These variable values are not accessed.
# The trailing dollar in the '1 dollar literal eol' test case accesses
# the empty variable instead, which is always guaranteed to be empty.
${:U }=			space-var-value
${:U${.newline}}=	newline-var-value
# But this one is accessed.
${:U'}=			single-quote-var-value'

all:
	$H 'Printing dollar from literals and variables'

	$C 'To survive the parser, a dollar sign must be doubled.'
	$T	'1 dollar literal'	'$'
	$T	'1 dollar literal eol'	''$
	$T	'2 dollar literal'	'$$'
	$T	'4 dollar literal'	'$$$$'

	$C 'Some hungry part of make eats all the dollars after a :U modifier.'
	$T	'1 dollar default'	''${:U$:Q}
	$T	'2 dollar default'	''${:U$$:Q}
	$T	'4 dollar default'	''${:U$$$$:Q}

	$C 'This works as expected.'
	$T	'1 dollar variable'	''${DOLLAR1:Q}
	$T	'2 dollar variable'	''${DOLLAR2:Q}
	$T	'4 dollar variable'	''${DOLLAR4:Q}

	$C 'Some hungry part of make eats all the dollars after a :U modifier.'
	$T	'1 dollar var-default'	''${:U${DOLLAR1}:Q}
	$T	'2 dollar var-default'	''${:U${DOLLAR2}:Q}
	$T	'4 dollar var-default'	''${:U${DOLLAR4}:Q}

	$H 'Dollar in :S pattern'

	$T	'S,$$,word,'		''${DOLLAR_XY:S,$,word,:Q}
	$T	'S,$$X,word,'		''${DOLLAR_XY:S,$X,word,:Q}
	$T	'S,$$$$X,word,'		''${DOLLAR_XY:S,$$X,word,:Q}
	$T	'S,$$$$$$X,word,'	''${DOLLAR_XY:S,$$$X,word,:Q}

	$T	'S,$$X,replaced,'	''${X:S,$X,replaced,:Q}
	$T	'S,$$$$X,replaced,'	''${X:S,$$X,replaced,:Q}
	$T	'S,$$$$$$X,replaced,'	''${X:S,$$$X,replaced,:Q}

	$H 'Dollar in :C character class'

	$C 'The A is replaced because the $$$$ is reduced to a single $$,'
	$C 'which is then resolved to the variable X with the value VAR_X.'
	$C 'The effective character class becomes [VAR_XY].'
	$T	'C,[$$$$XY],<&>,g'	''${DOLLAR_AXY:C,[$$XY],<&>,g:Q}

	$H 'Dollar in :C pattern'
	$C 'For some reason, multiple dollars are folded into one.'
	$T	'C,$$,dollar,g'		''${DOLLAR:C,$,dollar,g:Q}
	$T	'C,$$$$,dollar,g'	''${DOLLAR:C,$$,dollar,g:Q}

	$H 'Dollar in :S replacement'
	$C 'For some reason, multiple dollars are folded into one.'
	$T	'S,word,a$$Xo,'		''${WORD:S,word,a$Xo,:Q}
	$T	'S,word,a$$$$Xo,'	''${WORD:S,word,a$$Xo,:Q}
	$T	'S,word,a$$$$$$Xo,'	''${WORD:S,word,a$$$Xo,:Q}
