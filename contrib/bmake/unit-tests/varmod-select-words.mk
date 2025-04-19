# $NetBSD: varmod-select-words.mk,v 1.7 2025/03/30 01:27:13 rillig Exp $
#
# Tests for the :[...] variable modifier, which selects a single word
# or a range of words from a variable.
#
# History:
#	The variable modifier ':[...]' was added on 2003-09-27.
#
# See also:
#	modword.mk		(should be migrated here)

all: mod-squarebrackets mod-S-W mod-C-W mod-tW-tw

LIST=		one two three four five six
LONGLIST=	1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23

EMPTY=		# the space should be ignored
ESCAPEDSPACE=	\ # escaped space before the '#', the actual value is '\ '
REALLYSPACE:=	${:U }
HASH=		\#
AT=		@
STAR=		*
ZERO=		0
ONE=		1
MINUSONE=	-1

mod-squarebrackets: \
	mod-squarebrackets-empty \
	mod-squarebrackets-0-star-at \
	mod-squarebrackets-hash \
	mod-squarebrackets-n-ok-1 \
	mod-squarebrackets-n-error-1 \
	mod-squarebrackets-n-error-2 \
	mod-squarebrackets-n-ok-2 \
	mod-squarebrackets-n-error-3 \
	mod-squarebrackets-n-error-4 \
	mod-squarebrackets-n-ok-3 \
	mod-squarebrackets-start-end-error-1 \
	mod-squarebrackets-start-end-error-2 \
	mod-squarebrackets-start-end-error-3 \
	mod-squarebrackets-start-end-ok-1 \
	mod-squarebrackets-start-end-error-4 \
	mod-squarebrackets-start-end-ok-2 \
	mod-squarebrackets-start-end-error-5 \
	mod-squarebrackets-start-end-error-6 \
	mod-squarebrackets-start-end-ok-3 \
	mod-squarebrackets-nested-ok-1 \
	mod-squarebrackets-nested-error-1 \
	mod-squarebrackets-nested-ok-2 \
	mod-squarebrackets-space

mod-squarebrackets-empty:
# expect: make: Invalid modifier ":[]"
	@echo 'LIST:[]="${LIST:[]}" is an error'

mod-squarebrackets-0-star-at:
	@echo 'LIST:[0]="${LIST:[0]}"'
	@echo 'LIST:[0x0]="${LIST:[0x0]}"'
	@echo 'LIST:[000]="${LIST:[000]}"'
	@echo 'LIST:[*]="${LIST:[*]}"'
	@echo 'LIST:[@]="${LIST:[@]}"'
	@echo 'LIST:[0]:C/ /,/="${LIST:[0]:C/ /,/}"'
	@echo 'LIST:[0]:C/ /,/g="${LIST:[0]:C/ /,/g}"'
	@echo 'LIST:[0]:C/ /,/1g="${LIST:[0]:C/ /,/1g}"'
	@echo 'LIST:[*]:C/ /,/="${LIST:[*]:C/ /,/}"'
	@echo 'LIST:[*]:C/ /,/g="${LIST:[*]:C/ /,/g}"'
	@echo 'LIST:[*]:C/ /,/1g="${LIST:[*]:C/ /,/1g}"'
	@echo 'LIST:[@]:C/ /,/="${LIST:[@]:C/ /,/}"'
	@echo 'LIST:[@]:C/ /,/g="${LIST:[@]:C/ /,/g}"'
	@echo 'LIST:[@]:C/ /,/1g="${LIST:[@]:C/ /,/1g}"'
	@echo 'LIST:[@]:[0]:C/ /,/="${LIST:[@]:[0]:C/ /,/}"'
	@echo 'LIST:[0]:[@]:C/ /,/="${LIST:[0]:[@]:C/ /,/}"'
	@echo 'LIST:[@]:[*]:C/ /,/="${LIST:[@]:[*]:C/ /,/}"'
	@echo 'LIST:[*]:[@]:C/ /,/="${LIST:[*]:[@]:C/ /,/}"'

mod-squarebrackets-hash:
	@echo 'EMPTY="${EMPTY}"'
	@echo 'EMPTY:[#]="${EMPTY:[#]}" == 1 ?'
	@echo 'ESCAPEDSPACE="${ESCAPEDSPACE}"'
	@echo 'ESCAPEDSPACE:[#]="${ESCAPEDSPACE:[#]}" == 1 ?'
	@echo 'REALLYSPACE="${REALLYSPACE}"'
	@echo 'REALLYSPACE:[#]="${REALLYSPACE:[#]}" == 1 ?'
	@echo 'LIST:[#]="${LIST:[#]}"'
	@echo 'LIST:[0]:[#]="${LIST:[0]:[#]}" == 1 ?'
	@echo 'LIST:[*]:[#]="${LIST:[*]:[#]}" == 1 ?'
	@echo 'LIST:[@]:[#]="${LIST:[@]:[#]}"'
	@echo 'LIST:[1]:[#]="${LIST:[1]:[#]}"'
	@echo 'LIST:[1..3]:[#]="${LIST:[1..3]:[#]}"'

mod-squarebrackets-n-ok-1:
	@echo 'EMPTY:[1]="${EMPTY:[1]}"'
	@echo 'ESCAPEDSPACE="${ESCAPEDSPACE}"'
	@echo 'ESCAPEDSPACE:[1]="${ESCAPEDSPACE:[1]}"'
	@echo 'REALLYSPACE="${REALLYSPACE}"'
	@echo 'REALLYSPACE:[1]="${REALLYSPACE:[1]}" == "" ?'
	@echo 'REALLYSPACE:[*]:[1]="${REALLYSPACE:[*]:[1]}" == " " ?'
	@echo 'LIST:[1]="${LIST:[1]}"'
mod-squarebrackets-n-error-1:
# expect: make: Invalid modifier ":[1.]"
	@echo 'LIST:[1.]="${LIST:[1.]}" is an error'
mod-squarebrackets-n-error-2:
# expect: make: Extra text after "[1]"
	@echo 'LIST:[1].="${LIST:[1].}" is an error'
mod-squarebrackets-n-ok-2:
	@echo 'LIST:[2]="${LIST:[2]}"'
	@echo 'LIST:[6]="${LIST:[6]}"'
	@echo 'LIST:[7]="${LIST:[7]}"'
	@echo 'LIST:[999]="${LIST:[999]}"'
mod-squarebrackets-n-error-3:
# expect: make: Invalid modifier ":[-]"
	@echo 'LIST:[-]="${LIST:[-]}" is an error'
mod-squarebrackets-n-error-4:
# expect: make: Invalid modifier ":[--]"
	@echo 'LIST:[--]="${LIST:[--]}" is an error'
mod-squarebrackets-n-ok-3:
	@echo 'LIST:[-1]="${LIST:[-1]}"'
	@echo 'LIST:[-2]="${LIST:[-2]}"'
	@echo 'LIST:[-6]="${LIST:[-6]}"'
	@echo 'LIST:[-7]="${LIST:[-7]}"'
	@echo 'LIST:[-999]="${LIST:[-999]}"'
	@echo 'LONGLIST:[17]="${LONGLIST:[17]}"'
	@echo 'LONGLIST:[0x11]="${LONGLIST:[0x11]}"'
	@echo 'LONGLIST:[021]="${LONGLIST:[021]}"'
	@echo 'LIST:[0]:[1]="${LIST:[0]:[1]}"'
	@echo 'LIST:[*]:[1]="${LIST:[*]:[1]}"'
	@echo 'LIST:[@]:[1]="${LIST:[@]:[1]}"'
	@echo 'LIST:[0]:[2]="${LIST:[0]:[2]}"'
	@echo 'LIST:[*]:[2]="${LIST:[*]:[2]}"'
	@echo 'LIST:[@]:[2]="${LIST:[@]:[2]}"'
	@echo 'LIST:[*]:C/ /,/:[2]="${LIST:[*]:C/ /,/:[2]}"'
	@echo 'LIST:[*]:C/ /,/:[*]:[2]="${LIST:[*]:C/ /,/:[*]:[2]}"'
	@echo 'LIST:[*]:C/ /,/:[@]:[2]="${LIST:[*]:C/ /,/:[@]:[2]}"'
	@echo 'LONGLIST:[012..0x12]="${LONGLIST:[012..0x12]}"'

mod-squarebrackets-start-end-error-1:
# expect: make: Invalid modifier ":[1.]"
	@echo 'LIST:[1.]="${LIST:[1.]}" is an error'
mod-squarebrackets-start-end-error-2:
# expect: make: Invalid modifier ":[1..]"
	@echo 'LIST:[1..]="${LIST:[1..]}" is an error'
mod-squarebrackets-start-end-error-3:
# expect: make: Invalid modifier ":[1.. ]"
	@echo 'LIST:[1.. ]="${LIST:[1.. ]}" is an error'
mod-squarebrackets-start-end-ok-1:
	@echo 'LIST:[1..1]="${LIST:[1..1]}"'
mod-squarebrackets-start-end-error-4:
# expect: make: Invalid modifier ":[1..1.]"
	@echo 'LIST:[1..1.]="${LIST:[1..1.]}" is an error'
mod-squarebrackets-start-end-ok-2:
	@echo 'LIST:[1..2]="${LIST:[1..2]}"'
	@echo 'LIST:[2..1]="${LIST:[2..1]}"'
	@echo 'LIST:[3..-2]="${LIST:[3..-2]}"'
	@echo 'LIST:[-4..4]="${LIST:[-4..4]}"'
mod-squarebrackets-start-end-error-5:
# expect: make: Invalid modifier ":[0..1]"
	@echo 'LIST:[0..1]="${LIST:[0..1]}" is an error'
mod-squarebrackets-start-end-error-6:
# expect: make: Invalid modifier ":[-1..0]"
	@echo 'LIST:[-1..0]="${LIST:[-1..0]}" is an error'
mod-squarebrackets-start-end-ok-3:
	@echo 'LIST:[-1..1]="${LIST:[-1..1]}"'
	@echo 'LIST:[0..0]="${LIST:[0..0]}"'
	@echo 'LIST:[3..99]="${LIST:[3..99]}"'
	@echo 'LIST:[-3..-99]="${LIST:[-3..-99]}"'
	@echo 'LIST:[-99..-3]="${LIST:[-99..-3]}"'

mod-squarebrackets-nested-ok-1:
	@echo 'HASH="${HASH}" == "#" ?'
	@echo 'LIST:[$${HASH}]="${LIST:[${HASH}]}"'
	@echo 'LIST:[$${ZERO}]="${LIST:[${ZERO}]}"'
	@echo 'LIST:[$${ZERO}x$${ONE}]="${LIST:[${ZERO}x${ONE}]}"'
	@echo 'LIST:[$${ONE}]="${LIST:[${ONE}]}"'
	@echo 'LIST:[$${MINUSONE}]="${LIST:[${MINUSONE}]}"'
	@echo 'LIST:[$${STAR}]="${LIST:[${STAR}]}"'
	@echo 'LIST:[$${AT}]="${LIST:[${AT}]}"'
mod-squarebrackets-nested-error-1:
# expect: make: Invalid modifier ":[]"
	@echo 'LIST:[$${EMPTY}]="${LIST:[${EMPTY}]}" is an error'
mod-squarebrackets-nested-ok-2:
	@echo 'LIST:[$${LONGLIST:[21]:S/2//}]="${LIST:[${LONGLIST:[21]:S/2//}]}"'
	@echo 'LIST:[$${LIST:[#]}]="${LIST:[${LIST:[#]}]}"'
	@echo 'LIST:[$${LIST:[$${HASH}]}]="${LIST:[${LIST:[${HASH}]}]}"'

mod-squarebrackets-space:
	# As of 2020-11-01, it is possible to have spaces before the numbers
	# but not after them.  This is an unintended side-effect of using
	# strtol for parsing the numbers.
	@echo 'LIST:[  -1..   +3]="${LIST:[  -1..   +3]}"'

mod-C-W:
	@echo 'LIST:C/ /,/="${LIST:C/ /,/}"'
	@echo 'LIST:C/ /,/W="${LIST:C/ /,/W}"'
	@echo 'LIST:C/ /,/gW="${LIST:C/ /,/gW}"'
	@echo 'EMPTY:C/^/,/="${EMPTY:C/^/,/}"'
	@echo 'EMPTY:C/^/,/W="${EMPTY:C/^/,/W}"'

mod-S-W:
	@echo 'LIST:S/ /,/="${LIST:S/ /,/}"'
	@echo 'LIST:S/ /,/W="${LIST:S/ /,/W}"'
	@echo 'LIST:S/ /,/gW="${LIST:S/ /,/gW}"'
	@echo 'EMPTY:S/^/,/="${EMPTY:S/^/,/}"'
	@echo 'EMPTY:S/^/,/W="${EMPTY:S/^/,/W}"'

mod-tW-tw:
	@echo 'LIST:tW="${LIST:tW}"'
	@echo 'LIST:tw="${LIST:tw}"'
	@echo 'LIST:tW:C/ /,/="${LIST:tW:C/ /,/}"'
	@echo 'LIST:tW:C/ /,/g="${LIST:tW:C/ /,/g}"'
	@echo 'LIST:tW:C/ /,/1g="${LIST:tW:C/ /,/1g}"'
	@echo 'LIST:tw:C/ /,/="${LIST:tw:C/ /,/}"'
	@echo 'LIST:tw:C/ /,/g="${LIST:tw:C/ /,/g}"'
	@echo 'LIST:tw:C/ /,/1g="${LIST:tw:C/ /,/1g}"'
	@echo 'LIST:tw:tW:C/ /,/="${LIST:tw:tW:C/ /,/}"'
	@echo 'LIST:tW:tw:C/ /,/="${LIST:tW:tw:C/ /,/}"'
