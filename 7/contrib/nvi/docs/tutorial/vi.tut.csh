#!/bin/csh -f
#
# This makes the user's EXINIT variable set to the 'correct' things.
# I don't know what will happen if they also have a .exrc file!
#
# XXX
# Make sure that user is using a 24 line window!!!
#
if ($1 != "beginner" && $1 != "advanced") then
	echo Usage: $0 beginner or $0 advanced
	exit
endif

if ($?EXINIT) then
	set oexinit="$EXINIT"
	setenv EXINIT 'se ts=4 wm=8 sw=4'
endif

vi vi.{$1}

onintr:
	if ($?oexinit) then
		setenv EXINIT "$oexinit"
endif
